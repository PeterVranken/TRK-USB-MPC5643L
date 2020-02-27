/**
 * @file mai_main.c
 *   C entry function. The core completes the HW initialization (clocks run at full speed,
 * drivers for MPU and devices are initialized).\n
 *   The safe-RTOS is configured to run a few tasks, both OS and user mode tasks, which
 * drive the user LEDs. The user tasks injects a very few failures in order to demonstrate
 * the error catching capabilities of the RTOS.\n
 *   Only if all the LEDs are blinking everything is alright.\n
 *   Progress information is permanently written into the serial output channel. A terminal
 * on the development host needs to use these settings: 115000 Bd, 8 Bit data word, no
 * parity, 1 stop bit.\n
 *   Serial input is only demonstrated by echoing all input received from the terminal on
 * the host.
 *
 * Copyright (C) 2017-2020 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   taskA
 *   taskB
 *   taskC
 *   taskH
 *   taskT
 *   taskS
 */

/*
 * Include files
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
#include "sio_serialIO.h"
#include "rtos.h"
#include "del_delay.h"
#include "gsl_systemLoad.h"
//#include "mai_main.h"


/*
 * Defines
 */

/** The demo can be compiled with a ground load. Most tasks produce some CPU load if this
    switch is set to 1. */
#define TASKS_PRODUCE_GROUND_LOAD   1


/*
 * Local type definitions
 */

/** The enumeration of all events, tasks and priority, to have them as symbols in the
    source code. Most relevant are the event IDs. Actually, these IDs are provided by the
    RTOS at runtime, when creating the event. However, it is guaranteed that the IDs, which
    are dealt out by rtos_osCreateEvent() form the series 0, 1, 2, ..., 7. So we don't need
    to have a dynamic storage of the IDs; we define them as constants and double-check by
    assertion that we got the correct, expected IDs from rtos_osCreateEvent(). Note, this
    requires that the order of creating the events follows the order here in the
    enumeration. */
enum
{
    /** The ID of the created events. They occupy the index range starting from zero. */
    idEvTaskA = 0,  /// Event for first of the three mutually triggering round tobin tasks
    idEvTaskB,      /// Event for second of the three mutually triggering round tobin tasks
    idEvTaskC,      /// Event for third of the three mutually triggering round tobin tasks
    idEvTaskH,      /// Event for task of higer priority, which can preempt A, B, C
    idEvTaskT,      /// Event for cyclic task of same priority as H
    idEvTaskS,      /// Event for cyclic supervisor task in other process
    
    /** The number of tasks to register. */
    noRegisteredEvents
};


/** The RTOS uses constant priorities for its events, which are defined here.\n
      Note, the priority is a property of an event rather than of a task. A task implicitly
    inherits the priority of the event it is associated with. */
enum
{
    prioTaskIdle = 0,            /* Prio 0 is implicit, cannot be chosen explicitly */
    prioEvA = 3,
    prioEvB = prioEvA,
    prioEvC = prioEvA,
    prioEvH = 99,
    prioEvT = 99,
    prioEvS = 33,
};


/** In safe-RTOS a task belongs to a process, characterized by the PID, 1..4. The
    relationship is defined here.\n
      Note, a process needs to be configured in the linker script (actually: assignment of
    stack space) before it can be used. */
enum
{
    pidOs = 0,              /* kernel always and implicitly has PID 0 */
    pidTaskA = 1,
    pidTaskB = pidTaskA,
    pidTaskC = pidTaskA,
    pidTaskH = 1,
    pidTaskT = 1,
    pidTaskS = 2,
};




/*
 * Local prototypes
 */


/*
 * Data definitions
 */

/** Counter of cycles of infinite main loop. */
volatile unsigned long SBSS_OS(mai_cntTaskIdle) = 0;

/** Counter of first round robin task. */
volatile unsigned long long SBSS_P1(mai_cntTaskA) = 0;  

/** Counter of second round robin task. */
volatile unsigned long long SBSS_P1(mai_cntTaskB) = 0;  

/** Counter of third round robin task. */
volatile unsigned long long SBSS_P1(mai_cntTaskC) = 0;

/** Counter of event task H. */
volatile unsigned long long SBSS_P1(mai_cntTaskH) = 0;

/** Counter of cyclic task T. */
volatile unsigned long long SBSS_P1(mai_cntTaskT) = 0;

/** Sum of counters of tasks H and T. Used for test of coherent data access. */
volatile unsigned long long SBSS_P1(mai_cntSharedTaskHAndT) = 0;

/** Counter of cyclic task S. */
volatile unsigned int SBSS_P2(mai_cntTaskS) = 0;


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

    /* Scheduler test: Check order of initialization calls. */
    assert(cnt_ == PID);
    return cnt_ == PID? 0: -1;

} /* End of taskInitProcess */



/**
 * Round robin task A, the first in the cyclic sequence. Just does some consistency check.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process. This mechanism is used to report a problem in PRODUCTION
 * compilation, when we have no assertions available.
 *   @param PID
 * The ID of the process, the task function is executed in.
 */
static int32_t taskA(uint32_t PID ATTRIB_UNUSED)
{
    /* Trigger the next task out of the three round robin tasks.
         The triggered task has same priority, so the trigger needs to be always possible. */
    bool evCouldBeTriggered ATTRIB_DBG_ONLY = rtos_triggerEvent(idEvTaskB);
    assert(evCouldBeTriggered);
    
    /* Scheduler test: No race conditions with other round robin tasks. */
    const bool success = mai_cntTaskA == mai_cntTaskB  &&  mai_cntTaskB == mai_cntTaskC;
    ++ mai_cntTaskA;
    assert(success);
    return success? 0: -1;

} /* End of taskA */




/**
 * Round robin task B, the first in the cyclic sequence. Just does some consistency check.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process. This mechanism is used to report a problem in PRODUCTION
 * compilation, when we have no assertions available.
 *   @param PID
 * The ID of the process, the task function is executed in.
 */
static int32_t taskB(uint32_t PID ATTRIB_UNUSED)
{
    /* Trigger the next task out of the three round robin tasks.
         The triggered task has same priority, so the trigger needs to be always possible. */
    bool evCouldBeTriggered ATTRIB_DBG_ONLY = rtos_triggerEvent(idEvTaskC);
    assert(evCouldBeTriggered);
    
    /* Trigger a task of higher priority. This will lead to an immediate task switch.
         The triggered task has same priority, so the trigger needs to be always possible. */
    evCouldBeTriggered = rtos_triggerEvent(idEvTaskH);
    assert(evCouldBeTriggered);

    /* Scheduler test: No race conditions with other round robin tasks. */
    ++ mai_cntTaskB;
    const bool success = mai_cntTaskA == mai_cntTaskB  &&  mai_cntTaskB == mai_cntTaskC+1;
    assert(success);
    return success? 0: -1;

} /* End of taskB */




/**
 * Round robin task C, the first in the cyclic sequence. Just does some consistency check.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process. This mechanism is used to report a problem in PRODUCTION
 * compilation, when we have no assertions available.
 *   @param PID
 * The ID of the process, the task function is executed in.
 */
static int32_t taskC(uint32_t PID ATTRIB_UNUSED)
{
    /* Trigger the next task out of the three round robin tasks.
         The triggered task has same priority, so the trigger needs to be always possible. */
    bool evCouldBeTriggered ATTRIB_DBG_ONLY = rtos_triggerEvent(idEvTaskA);
    assert(evCouldBeTriggered);
    
    /* Scheduler test: No race conditions with other round robin tasks. */
    ++ mai_cntTaskC;
    const bool success = mai_cntTaskA == mai_cntTaskB  &&  mai_cntTaskB == mai_cntTaskC;
    assert(success);
    return success? 0: -1;

} /* End of taskC */




/**
 * Event task H, which is of higher priority as the three round robins. It must not have
 * any race conditions with these as it is triggered only by them. We have a little
 * according test.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process. This mechanism is used to report a problem in PRODUCTION
 * compilation, when we have no assertions available.
 *   @param PID
 * The ID of the process, the task function is executed in.
 */
static int32_t taskH(uint32_t PID ATTRIB_UNUSED)
{
    /* Scheduler test: No race conditions with round robin tasks and nor with task T. */
    ++ mai_cntTaskH;
    ++ mai_cntSharedTaskHAndT;
    bool success = mai_cntSharedTaskHAndT == mai_cntTaskT + mai_cntTaskH;
    assert(success);

    if(mai_cntTaskA != mai_cntTaskH  ||  mai_cntTaskH != mai_cntTaskB+1)
        success = false;
    assert(success);

    return success? 0: -1;

} /* End of taskH */




/**
 * Timer triggered task T, which is of same priority as task H. It must not have any race
 * conditions with task H but can have race conditions with the round robins of lower
 * priority. We have a little according test.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process. This mechanism is used to report a problem in PRODUCTION
 * compilation, when we have no assertions available.
 *   @param PID
 * The ID of the process, the task function is executed in.
 */
static int32_t taskT(uint32_t PID ATTRIB_UNUSED)
{
    /* Scheduler test: No race conditions with task H. */
    ++ mai_cntTaskT;
    ++ mai_cntSharedTaskHAndT;
    const bool success = mai_cntSharedTaskHAndT == mai_cntTaskT + mai_cntTaskH;
    assert(success);
    
    if((mai_cntTaskT & (256-1)) == 1)
    {
        iprintf( "Task S: %u cycles. Tasks A, B, C: %llu cycles, task H: %llu cycles"
                 ", task T: %llu cycles\r\n"
               , mai_cntTaskS
               , mai_cntTaskA
               , mai_cntTaskH
               , mai_cntTaskT
               );
    }

    return success? 0: -1;

} /* End of taskT */




/**
 * Timer triggered supervisor task S. It looks for failures and flashes the LED as long as
 * no failure is recognized.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process. This mechanism is used to report a problem in PRODUCTION
 * compilation, when we have no assertions available.
 *   @param PID
 * The ID of the process, the task function is executed in.
 */
static int32_t taskS(uint32_t PID ATTRIB_DBG_ONLY)
{
    /* This task runs in another process as the supervised tasks. */
    assert(PID == 2);
    
    ++ mai_cntTaskS;
    
    const unsigned int stackReserveOs = rtos_getStackReserve(pidOs)
                     , stackReserveP1 = rtos_getStackReserve(/* PID */ 1)
                     , stackReserveP2 = rtos_getStackReserve(/* PID */ 2);
                     
    const bool success = rtos_getNoTotalTaskFailure(/* PID */ 1) == 0
                         &&  rtos_getNoTotalTaskFailure(/* PID */ 2) == 0
                         &&  stackReserveOs >= 4096
                         &&  stackReserveP1 >= 1024
                         &&  stackReserveP2 >= 1024
                         ;
    if(success)
    {
        /* Normal operation: Let green LED blink at about 1 Hz. */
        lbd_setLED(lbd_led_D4_grn, /* isOn */ (mai_cntTaskS & 32) != 0);
    }
    else if(!rtos_isProcessSuspended(/* PID */ 1))
    {
        lbd_setLED(lbd_led_D4_grn, /* isOn */ false);
        rtos_suspendProcess(/* PID */ 1);
    }
    else
    {
        /* Failure: Let red LED blink at higher rate. */
        lbd_setLED(lbd_led_D4_red, /* isOn */ (mai_cntTaskS & 16) != 0);
    }

    return success? 0: -1;

} /* End of taskS */




/**
 * C entry function main. Is used for and only for the Z7_0 core.
 *   @param noArgs
 * Number of arguments in \a argAry. Is actually always equal to zero.
 *   @param argAry
 * Array of string arguments to the function. Is actually always equal to NULL.
 */
int main(int noArgs ATTRIB_DBG_ONLY, const char *argAry[] ATTRIB_DBG_ONLY)
{
    /* The arguments of the main function are quite useless. Just check correctness. */
    assert(noArgs == 0  &&  argAry == NULL);


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
    
    /* The core is now running in the desired state. I/O driver initialization follows to
       the extend required by this simple sample. */

    /* The interrupt controller is configured. This is the first driver initialization
       call: Many of the others will register their individual ISRs and this must not be
       done prior to initialization of the interrupt controller. */
    rtos_osInitINTCInterruptController();

    /* Initialize the button and LED driver for the eval board. */
    lbd_osInitLEDAndButtonDriver( /* onButtonChangeCallback */ NULL
                                , /* pidOnButtonChangeCallback */ 0
                                );

    /* Initialize the serial output channel as prerequisite of using printf. */
    sio_osInitSerialInterface(/* baudRate */ 115200);

    /* Register the process initialization tasks. */
    bool initOk = true;
    if(rtos_osRegisterInitTask(taskInitProcess, /* PID */ 1, /* tiTaskMaxInUS */ 1000)
       != rtos_err_noError
      )
    {
        initOk = false;
    }


#define CREATE_TASK(name, tiCycleInMs)                                                      \
    if(rtos_osCreateEvent( &idEvent                                                         \
                         , /* tiCycleInMs */              tiCycleInMs                       \
                         , /* tiFirstActivationInMs */    0                                 \
                         , /* priority */                 prioEv##name                      \
                         , /* minPIDToTriggerThisEvent */ tiCycleInMs == 0                  \
                                                          ? 1                               \
                                                          : RTOS_EVENT_NOT_USER_TRIGGERABLE \
                         )                                                                  \
       == rtos_err_noError                                                                  \
      )                                                                                     \
    {                                                                                       \
        assert(idEvent == idEvTask##name);                                                  \
        if(rtos_osRegisterUserTask( idEvTask##name                                          \
                                  , task##name                                              \
                                  , pidTask##name                                           \
                                  , /* tiTaskMaxInUs */ 0                                   \
                                  )                                                         \
           != rtos_err_noError                                                              \
          )                                                                                 \
        {                                                                                   \
            initOk = false;                                                                 \
        }                                                                                   \
    }                                                                                       \
    else                                                                                    \
    {                                                                                       \
        initOk = false;                                                                     \
    }
/* End of macro CREATE_TASK */

    /* Create the events that trigger application tasks at the RTOS. Note, we do not really
       respect the ID, which is assigned to the event by the RTOS API rtos_osCreateEvent().
       The returned value is redundant. This technique requires that we create the events
       in the right order and this requires in practice a double-check by assertion - later
       maintenance errors are unavoidable otherwise. */
    unsigned int idEvent;
    CREATE_TASK(/* name */ A, /* tiCycleInMs */ 0)
    CREATE_TASK(/* name */ B, /* tiCycleInMs */ 0)
    CREATE_TASK(/* name */ C, /* tiCycleInMs */ 0)
    CREATE_TASK(/* name */ H, /* tiCycleInMs */ 0)
    CREATE_TASK(/* name */ T, /* tiCycleInMs */ 5)
    CREATE_TASK(/* name */ S, /* tiCycleInMs */ 13)

    /* The last check ensures that we didn't forget to register a task. */
    assert(initOk &&  idEvent == noRegisteredEvents-1);

    rtos_osGrantPermissionSuspendProcess( /* pidOfCallingTask */ 2 /* Supervisor */
                                        , /* targetPID */ 1        /* Tasks A, B, C, T, H */
                                        );

    /* Initialize the RTOS kernel. The global interrupt processing is resumed if it
       succeeds. The step involves a configuration check. We must not startup the SW if the
       check fails. */
    if(!initOk ||  rtos_osInitKernel() != rtos_err_noError)
        while(true)
            ;

    /* Here we are in the idle task. */
    ++ mai_cntTaskIdle;

    /* Trigger the first of the three round robin tasks. From now on, they will consume all
       computation time at their priority level and we must never get back into the idle
       task.
         Since we are here in idle, the trigger needs to be always possible. */
    bool evCouldBeTriggered ATTRIB_DBG_ONLY = rtos_osTriggerEvent(idEvTaskA);
    assert(evCouldBeTriggered);
    while(true)    
    {
        assert(false);

        /* Make spinning of the idle task observable in the debugger. */
        ++ mai_cntTaskIdle;
    }
} /* End of main */
