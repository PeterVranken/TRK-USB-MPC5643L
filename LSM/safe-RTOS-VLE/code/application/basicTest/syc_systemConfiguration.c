/**
 * @file syc_systemConfiguration.c
 * System configuration: Configuration of tasks and I/O drivers as required for the
 * application.\n
 *   The code in this file is executed in supervisor mode and it belongs in to the sphere
 * of trusted code.
 *
 * Copyright (C) 2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
/* Module interface
 *   main
 * Local functions
 *   taskInitProcess
 *   isrPit1
 *   isrPit2
 *   isrPit3
 *   installInterruptServiceRoutines
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
#include "sio_serialIO.h"
#include "rtos.h"
#include "gsl_systemLoad.h"
#include "tcx_testContext.h"
#include "prr_processReporting.h"
#include "prf_processFailureInjection.h"
#include "prs_processSupervisor.h"
#include "syc_systemConfiguration.h"


/*
 * Defines
 */


/*
 * Local type definitions
 */

/*
 * Local prototypes
 */


/*
 * Data definitions
 */

/** The current, averaged CPU load in tens of percent. */
volatile unsigned int SDATA_OS(syc_cpuLoad) = 1000;

/** A counter of the invocations of the otherwise useless PIT3 ISR. */
volatile unsigned long long SBSS_OS(syc_cntISRPit3) = 0;


/*
 * Function implementation
 */

/**
 * Initialization task of process \a PID.
 *   @return
 * The function returns the Boolean descision, whether the initialization was alright and
 * the system can start up. "Not alright" is expressed by a negative number, which hinders
 * the RTOS to startup.
 *   @param PID
 * The ID of the process, the task function is executed in.
 *   @remark
 * In this sample, we demonstrate that different processes' tasks can share the same task
 * function implementation. This is meant a demonstration of the technical feasibility but
 * not of good practice; the implementation needs to use shared memory, which may break a
 * safety constraint, and it needs to consider the different privileges of the processes.
 */
static int32_t taskInitProcess(uint32_t PID)
{
    static unsigned int cnt_ SECTION(.data.Shared.cnt_) = 0;
    ++ cnt_;

    /* Only process 1 has access to the C lib (more precise: to those functions of the C
       lib, which write to lib owned data objects) and can write a status message. */
    if(PID == 1)
        iprintf("taskInitPID%lu(): %u\r\n", PID, cnt_);

    return cnt_ == PID? 0: -1;

} /* End of taskInitProcess */



/**
 * A regularly triggered interrupt handler for the timer PIT1. The handler starts a user
 * task, which belongs to the failing process. In this process it has the highest priority.
 * The ISR must not be affected by the failures occurring in that process.
 *   @remark
 * This is a normal interrupt running in the kernel context (supervisor mode, no MPU
 * restrictions).
 */
static void isrPit1(void)
{
    static unsigned int SDATA_OS(cnt_) = 0;

    /* Directly start a user task. It is executed synchronously with this ISR and on the
       same priority level. */
    static const rtos_taskDesc_t taskConfig = { .addrTaskFct = (uintptr_t)prf_task1ms
                                              , .PID = syc_pidFailingTasks
                                              , .tiTaskMax = 0 //RTOS_TI_MS2TICKS(5)
                                              };
    rtos_osRunTask(&taskConfig, /* taskParam */ cnt_);
    ++ cnt_;

    /* Acknowledge the interrupt in the causing HW device. Can be done as this is "trusted
       code" that is running in supervisor mode. */
    PIT.TFLG1.B.TIF = 0x1;

} /* End of isrPit1 */



/**
 * A regularly triggered interrupt handler for the timer PIT2. It triggers an RTOS event
 * such that the watchdog task in the supervisor process is started. As long as we don't
 * see any activation losses the watchdog task will execute synchronous with this ISR. The
 * difference to the task directly started by isrPIT1 is that the watchdog task has a lower
 * priority than the triggering ISR.
 *   @remark
 * This is a normal interrupt running in the kernel context (supervisor mode, no MPU
 * restrictions).
 */
static void isrPit2(void)
{
    /* Indirectly start a user task. It is executed asynchronously to this ISR and on has its
       own, irrelated priority level. */
    rtos_osTriggerEvent(syc_idEvPIT2);

    /* Acknowledge the interrupt in the causing HW device. Can be done as this is "trusted
       code" that is running in supervisor mode. */
    PIT.TFLG2.B.TIF = 0x1;

} /* End of isrPit2 */



/**
 * A regularly triggered interrupt handler for the timer PIT3. The interrupt does nothing
 * but counting a variable. It is triggered at high frequency and asynchronously to the
 * kernel's clock tick to prove the system stability and properness of the context switches.
 *   @remark
 * This is a normal interrupt running in the kernel context (supervisor mode, no MPU
 * restrictions).
 */
static void isrPit3(void)
{
    ++ syc_cntISRPit3;

    /* Acknowledge the interrupt in the causing HW device. Can be done as this is "trusted
       code" that is running in supervisor mode. */
    PIT.TFLG3.B.TIF = 0x1;

} /* End of isrPit3 */



/**
 * This demonstration software uses a number of fast interrupts to produce system load and
 * prove stability. The interrupts are timer controlled (for simplicity) but the
 * activations are chosen as asynchronous to the operating system clock as possible to
 * provoke a most variable preemption pattern.
 */
static void installInterruptServiceRoutines(void)
{
    /* 0x2: Disable all PIT timers during configuration. Note, this is a
       global setting for all four timers. Accessing the bits makes this rountine have race
       conditions with the RTOS initialization that uses timer PIT0. Both routines must not
       be called in concurrency. */
    PIT.PITMCR.R |= 0x2u;

    /* Install the ISRs now that all timers are stopped.
         Vector numbers: See MCU reference manual, section 28.7, table 28-4. */
    rtos_osRegisterInterruptHandler( &isrPit1
                                   , /* vectorNum */ 60
                                   , /* psrPriority */ syc_prioISRPit1
                                   , /* isPreemptable */ true
                                   );
    rtos_osRegisterInterruptHandler( &isrPit2
                                   , /* vectorNum */ 61
                                   , /* psrPriority */ syc_prioISRPit2
                                   , /* isPreemptable */ true
                                   );
    rtos_osRegisterInterruptHandler( &isrPit3
                                   , /* vectorNum */ 127
                                   , /* psrPriority */ syc_prioISRPit3
                                   , /* isPreemptable */ true
                                   );

    /* Peripheral clock has been initialized to 120 MHz. The timer counts at this rate. The
       RTOS operates in ticks of 1ms. Here, we use prime numbers to get good asynchronicity
       with the RTOS clock.
         -1: See MCU reference manual, 36.5.1, p. 1157. */
    PIT.LDVAL1.R = 119993-1;/* Interrupt rate approx. 1kHz */
    PIT.LDVAL2.R = 120011-1;/* Interrupt rate approx. 1kHz for watchdog */
    PIT.LDVAL3.R = 3989-1;  /* Interrupt rate approx. 30kHz */

    /* Enable interrupts by the timers and start them. */
    PIT.TCTRL1.R = 0x3;
    PIT.TCTRL2.R = 0x3;
    PIT.TCTRL3.R = 0x3;

    /* Enable timer operation, all four timers are affected. Interrupt processing should
       start. */
    PIT.PITMCR.R &= ~0x2u;

} /* End of installInterruptServiceRoutines */



/**
 * Entry point into C code. The C main function is entered without arguments and despite
 * of its usual return code definition it must never be left in this environment.
 * (Returning from main would enter an infinite loop in the calling assembler startup
 * code.)
 */
void main(void)
{
    /* The first operation of the main function is the call of ihw_initMcuCoreHW(). The
       assembler implemented startup code has brought the MCU in a preliminary working
       state, such that the C compiler constructs can safely work (e.g. stack pointer is
       initialized, memory access through MMU is enabled).
         ihw_initMcuCoreHW() does the remaining hardware initialization, that is still
       needed to bring the MCU in a basic stable working state. The main difference to the
       preliminary working state of the assembler startup code is the selection of
       appropriate clock rates.
         This part of the hardware configuration is widely application independent. The
       only reason, why this code has not been called directly from the assembler code
       prior to entry into main() is code transparency. It would mean to have a lot of C
       code without an obvious point, where it is called. */
    ihw_initMcuCoreHW();

    /* The interrupt controller is configured. */
    rtos_osInitINTCInterruptController();

    /* Initialize the button and LED driver for the eval board. */
    lbd_initLEDAndButtonDriver( /* onButtonChangeCallback */ NULL
                              , /* pidOnButtonChangeCallback */ 0
                              );

    /* Initialize the serial output channel as prerequisite of using printf. */
    sio_initSerialInterface(/* baudRate */ 115200);

    /* Register the process initialization tasks. Here, we used always the same function. */
    bool initOk = true;
    unsigned int u;
    for(u=1; u<=3; ++u)
    {
        if(rtos_osRegisterInitTask(taskInitProcess, /* PID */ u, /* tiTaskMaxInUs */ 1000)
           != rtos_err_noError
          )
        {
            initOk = false;
        }
    }

    /* Create the events that trigger application tasks at the RTOS. Note, we do not really
       respect the ID, which is assigned to the event by the RTOS API rtos_osCreateEvent().
       The returned value is redundant. This technique requires that we create the events
       in the right order and this requires in practice a double-check by assertion - later
       maintenance errors are unavoidable otherwise. */
    unsigned int idEvent;
    if(rtos_osCreateEvent
                    ( &idEvent
                    , /* tiCycleInMs */              997 /* about 1s but prime to others */
                    , /* tiFirstActivationInMs */    19
                    , /* priority */                 syc_prioEvReporting
                    , /* minPIDToTriggerThisEvent */ RTOS_EVENT_NOT_USER_TRIGGERABLE
                    )
       != rtos_err_noError
      )
    {
        initOk = false;
    }
    else
        assert(idEvent == syc_idEvReporting);

    if(rtos_osCreateEvent( &idEvent
                         , /* tiCycleInMs */              10
                         , /* tiFirstActivationInMs */    0
                         , /* priority */                 syc_prioEvTest
                         , /* minPIDToTriggerThisEvent */ RTOS_EVENT_NOT_USER_TRIGGERABLE
                         )
       != rtos_err_noError
      )
    {
        initOk = false;
    }
    else
        assert(idEvent == syc_idEvTest);

    if(rtos_osCreateEvent( &idEvent
                         , /* tiCycleInMs */              11
                         , /* tiFirstActivationInMs */    0
                         , /* priority */                 syc_prioEvTestCtxSw
                         , /* minPIDToTriggerThisEvent */ RTOS_EVENT_NOT_USER_TRIGGERABLE
                         )
       != rtos_err_noError
      )
    {
        initOk = false;
    }
    else
        assert(idEvent == syc_idEvTestCtxSw);

    if(rtos_osCreateEvent( &idEvent
                         , /* tiCycleInMs */              0
                         , /* tiFirstActivationInMs */    0
                         , /* priority */                 syc_prioEvPIT2
                         , /* minPIDToTriggerThisEvent */ RTOS_EVENT_NOT_USER_TRIGGERABLE
                         )
       != rtos_err_noError
      )
    {
        initOk = false;
    }
    else
        assert(idEvent == syc_idEvPIT2);

    if(rtos_osCreateEvent( &idEvent
                         , /* tiCycleInMs */              17
                         , /* tiFirstActivationInMs */    0
                         , /* priority */                 syc_prioEv17ms
                         , /* minPIDToTriggerThisEvent */ RTOS_EVENT_NOT_USER_TRIGGERABLE
                         )
       != rtos_err_noError
      )
    {
        initOk = false;
    }
    else
        assert(idEvent == syc_idEv17ms);

    /* The tasks are associated with the events. We have two tasks, which are not triggered
       by the RTOS scheduler but by independent interrupts. One is triggered through an
       event from an asynchronous interrupt service routine (i.e. it may run on a lower
       priority as the ISR) and the other one is directly started from the ISR and
       necessarily shares the priority with the ISR. This one is the only task, which is
       not found here in the list of registrations. */
    if(rtos_osRegisterUserTask( syc_idEvReporting
                              , prr_taskReporting
                              , syc_pidReporting
                              , /* tiTaskMaxInUs */ 1500000
                              )
       != rtos_err_noError
      )
    {
        initOk = false;
    }

    /* The next three tasks share the same event for triggering. The order of registration
       matters: When the event becomes due the tasks will activated in the order of
       registration. We need to first see the task, which commands the (failing) action to
       take, then the task which executes the action and finally the task, which
       double-checks the system behavior. */
    if(rtos_osRegisterUserTask( syc_idEvTest
                              , prs_taskCommandError
                              , syc_pidSupervisor
                              , /* tiTaskMaxInUs */ 1500
                              )
       != rtos_err_noError
      )
    {
        initOk = false;
    }
    if(rtos_osRegisterUserTask( syc_idEvTest
                              , prf_taskInjectError
                              , syc_pidFailingTasks
                              , /* tiTaskMaxInUs */ 2500
                              )
       != rtos_err_noError
      )
    {
        initOk = false;
    }
    if(rtos_osRegisterUserTask( syc_idEvTest
                              , prs_taskEvaluateError
                              , syc_pidSupervisor
                              , /* tiTaskMaxInUs */ 1500
                              )
       != rtos_err_noError
      )
    {
        initOk = false;
    }

    if(rtos_osRegisterUserTask( syc_idEvTestCtxSw
                              , prr_taskTestContextSwitches
                              , syc_pidReporting
                              , /* tiTaskMaxInUs */ 0
                              )
       != rtos_err_noError
      )
    {
        initOk = false;
    }
    if(rtos_osRegisterUserTask( syc_idEvPIT2
                              , prs_taskWatchdog
                              , syc_pidSupervisor
                              , /* tiTaskMaxInUs */ 0
                              )
       != rtos_err_noError
      )
    {
        initOk = false;
    }
    if(rtos_osRegisterUserTask( syc_idEv17ms
                              , prf_task17ms
                              , syc_pidFailingTasks
                              , /* tiTaskMaxInUs */ 0
                              )
       != rtos_err_noError
      )
    {
        initOk = false;
    }

    /* The watchdog uses the reporting process, which owns the C library and can do a
       printf, to regularly print a progress message. We need to grant the required
       permissions. */
    rtos_osGrantPermissionRunTask( /* pidOfCallingTask */ syc_pidSupervisor
                                 , /* targetPID */ syc_pidReporting
                                 );

    /* The watchdog uses service rtos_suspendProcess() if it recognizes an error. We need
       to grant the required permissions. */
    rtos_osGrantPermissionSuspendProcess( /* pidOfCallingTask */ syc_pidSupervisor
                                        , /* targetPID */ syc_pidFailingTasks
                                        );

    /* Initialize the RTOS kernel. The global interrupt processing is resumed if it
       succeeds. The step involves a configuration check. We must not startup the SW if the
       check fails. */
    if(!initOk ||  rtos_osInitKernel() != rtos_err_noError)
        while(true)
            ;

    /* Installing more interrupts should be possible while the system is already running. */
    installInterruptServiceRoutines();

    /* The code down here becomes our idle task. It is executed when and only when no
       application task or ISR is running. */

    while(true)
    {
        /* Compute the average CPU load. Note, this operation lasts about 1.5s and has a
           significant impact on the cycling speed of this infinite loop. Furthermore, it
           measures only the load produced by the tasks and system interrupts but not that
           of the rest of the code in the idle loop. */
        syc_cpuLoad = gsl_getSystemLoad();
    }
} /* End of main */
