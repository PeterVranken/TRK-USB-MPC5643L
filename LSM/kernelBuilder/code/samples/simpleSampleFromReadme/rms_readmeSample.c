/**
 * @file rms_readmeSample.c
 * This is the simple sample from the readme of kernelBuilder, see
 * https://github.com/PeterVranken/TRK-USB-MPC5643L/tree/master/LSM/kernelBuilder#simple-sample-code.
 * It implements the most simple RTOS. There is one task besides the idle task. This
 * task is a real time task in that it is executed every 100ms. Both tasks regularly
 * print a hello world message. (Serial port at 115200 Bd, 8 Bit, 1 Start, 1 Stop bit)
 *
 * Copyright (C) 2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   rms_scheduler
 * Local functions
 *   isrRTOSSystemTimer
 *   enableRTOSSystemTimer
 *   sc_terminateTask
 *   task100ms
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "MPC5643L.h"

#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "int_interruptHandler.h"
#include "sc_systemCalls.h"
#include "sio_sysCallInterface.h"
#include "ccx_createContextSaveDesc.h"
#include "rms_readmeSample.h"


/*
 * Defines
 */

/* System call index: Terminate context. */
#define IDX_SYS_CALL_TERMINATE_TASK  (-1)

/*
 * Local type definitions
 */


/*
 * Local prototypes
 */

static uint32_t sc_terminateTask(int_cmdContextSwitch_t *pCmdContextSwitch);

/*
 * Data definitions
 */

/** We have two tasks, there are two context descriptors. */
static int_contextSaveDesc_t _contextSaveDescIdle, _contextSaveDescTask100ms;

/** The scheduler always keeps track, which context is the currently active one. */
static bool _isTask100msRunning = false;

/** Overrun counter for task activation. */
volatile unsigned int rms_cntOverrunTask100ms = 0;

/** The table of C functions, which implement the kernel relevant system calls. */
const SECTION(.rodata.ivor) int_systemCallFct_t int_systemCallHandlerAry[] =
    { [~IDX_SYS_CALL_TERMINATE_TASK] = (int_systemCallFct_t)sc_terminateTask,
    };

/** The table of C functions, which implement the simple system calls. */
const SECTION(.rodata.ivor) int_simpleSystemCallFct_t int_simpleSystemCallHandlerAry[] =
    { SIO_SIMPLE_SYSTEM_CALLS_TABLE_ENTRIES /* System calls for serial and printf */
    };

#ifdef DEBUG
/* The number of entries in the tables of system calls. Only required for boundary
   check in DEBUG compilation. */
const uint32_t int_noSystemCalls = sizeOfAry(int_systemCallHandlerAry);
const uint32_t int_noSimpleSystemCalls = sizeOfAry(int_simpleSystemCallHandlerAry);
#endif

/*
 * Function implementation
 */

/** 
 * This is the RTOS system timer, called once a 100 ms.
 */
static uint32_t isrRTOSSystemTimer(int_cmdContextSwitch_t *pCmdContextSwitch)
{
    /* Acknowledge the timer interrupt in the causing HW device. */
    PIT.TFLG0.B.TIF = 0x1;

    /* Create task context if (already) possible, otherwise report overrun. */
    if(_isTask100msRunning == false)
    {
        /* No race conditions inside scheduler: We can use ordinary variables to
           maintain our state. */
        _isTask100msRunning = true;

        /* Command a context switch from idle to task100ms. */
        pCmdContextSwitch->pSuspendedContextSaveDesc = &_contextSaveDescIdle;
        pCmdContextSwitch->pResumedContextSaveDesc = &_contextSaveDescTask100ms;
        pCmdContextSwitch->signalToResumedContext = (uint32_t)rms_cntOverrunTask100ms;
        return int_rcIsr_switchContext | int_rcIsr_createEnteredContext;
    }
    else
    {
        ++ rms_cntOverrunTask100ms;
        return int_rcIsr_doNotSwitchContext;
    }
} /* End of isrRTOSSystemTimer */



/**
 * Start the interrupt which clocks the RTOS.
 */
static void enableRTOSSystemTimer(void)
{
    /* Disable all PIT timers during configuration. */
    PIT.PITMCR.R = 0x2;

    /* Install the interrupt handler for cyclic timer PIT 0. */
    ihw_installINTCInterruptHandler
                ( (int_externalInterruptHandler_t){.kernelIsr = &isrRTOSSystemTimer}
                , /* vectorNum */ 59 /* Timer PIT 0 */
                , /* psrPriority */ 1
                , /* isPreemptable */ true
                , /* isKernelInterrupt */ true
                );

    /* Peripheral clock has been initialized to 120 MHz. To get a 100ms interrupt tick
       we need to count till 12000000.
         -1: See MCU reference manual, 36.5.1, p. 1157. */
    PIT.LDVAL0.R = 12000000-1;

    /* Enable interrupts by this timer and start it. */
    PIT.TCTRL0.R = 0x3;

    /* Enable timer operation and let them be stopped on debugger entry. Note, this is
       a global setting for all four timers, even if we use and reserve only one for
       the RTOS. */
    PIT.PITMCR.R = 0x1;

} /* End of enableRTOSSystemTimer */



/**
 * The implementation of our system call to terminate the task (to keep the context
 * descriptor usable for the next creation).
 */
static uint32_t sc_terminateTask(int_cmdContextSwitch_t *pCmdContextSwitch)
{
    /* No race conditions inside scheduler: We can use ordinary variables to maintain
       our state. */
    assert(_isTask100msRunning);
    _isTask100msRunning = false;

    /* Command a context switch from task100ms to idle. */
    pCmdContextSwitch->pSuspendedContextSaveDesc = &_contextSaveDescTask100ms;
    pCmdContextSwitch->pResumedContextSaveDesc = &_contextSaveDescIdle;
    return int_rcIsr_switchContext | int_rcIsr_terminateLeftContext;

} /* End of sc_terminateTask */



/**
 * Our 100ms single-shot task. This function is invoked every 100 ms in user mode.
 *   @param taskParam Data provided at creation of task context. Here: Number of lost
 * activations.
 */
static _Noreturn uint32_t task100ms(uint32_t taskParam)
{
    static unsigned int cnt_ = 0;
    printf("%s: %us, %lu lost activations so far\r\n", __func__, cnt_++/10, taskParam);

    /* We terminate explicit in order to keep the sample one function shorter. */
    int_systemCall(IDX_SYS_CALL_TERMINATE_TASK);
    assert(false);

} /* End of task100ms */



/** 
 * Main entry point into the scheduler. There are two tasks. The idle task, which
 * inherits the startup context and one real time task. The latter is a single-shot
 * task, which is called every 100ms and which shares the stack with the idle task.
 */
void _Noreturn rms_scheduler(void)
{
    /* Create a context descriptor of the idle task. */
    ccx_createContextSaveDescOnTheFly( &_contextSaveDescIdle
                                     , /* stackPointer */ NULL
                                     , /* fctEntryIntoOnTheFlyStartedContext */ NULL
                                     , /* privilegedMode */ true
                                     );

    /* Create a context descriptor for the other task: Single-shot, share stack. */
    ccx_createContextSaveDescShareStack
                                ( &_contextSaveDescTask100ms
                                , /* pPeerContextSaveDesc */ &_contextSaveDescIdle
                                , /* fctEntryIntoOnTheFlyStartedContext */ task100ms
                                , /* privilegedMode */ false
                                );

    /* All contexts are ready for use, we can start the RTOS system timer. */
    enableRTOSSystemTimer();

    /* We continue in the idle context. */
    while(true)
    {
        volatile unsigned long u;
        for(u=0; u<2500000; ++u)
            ;
        printf("%s: This is the idle task\r\n", __func__);
    }
} /* End of rms_scheduler */
