/**
 * @file prf_processFailureInjection.c
 * Implementation of task functions. The tasks and their implementation belong into the
 * sphere of the protected user code. They are are defined in the sphere of unprotected
 * operating system code and anything which relates to their configuration cannot be
 * changed anymore by user code.
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
 *   prf_nestStackInjectError
 *   prf_taskInjectError
 *   prf_task17ms
 *   prf_task1ms
 * Local functions
 *   injectError
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "typ_types.h"
#include "rtos.h"
#include "gsl_systemLoad.h"
#include "syc_systemConfiguration.h"
#include "prf_processFailureInjection.h"


/*
 * Defines
 */
 

/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
void prf_nestStackInjectError(unsigned int remainingLevels);
 
 
/*
 * Data definitions
 */
 
/** The next error to inject. This object is written by task prs_taskCommandError and read
    by prf_taskInjectError. There are no race conditions between these two tasks. */
prf_cmdFailure_t DATA_SHARED(prf_cmdFailure) = { .kindOfFailure = prf_kof_noFailure
                                                 , .noRecursionsBeforeFailure = 0
                                                 , .value = 0
                                                 , .address = 0
                                                 , .expectedNoProcessFailures = 0
                                                 , .expectedNoProcessFailuresTolerance = 0
                                                 , .expectedValue = 0
                                               };

/** Task invocation counter. Here for task1ms. */
static unsigned long SDATA_PRC_FAIL(_cntTask1ms) = 0;


/*
 * Function implementation
 */

/**
 * Implementation of the intentionally produced failures.
 */
static void injectError(void)
{
    switch(prf_cmdFailure.kindOfFailure)
    {
    case prf_kof_noFailure:
        /* Here, we can test the voluntary task termination for a deeply nested stack. */
        rtos_terminateTask(0);
        break;

    case prf_kof_userTaskError:
        rtos_terminateTask(-1);
        break;

    case prf_kof_jumpToResetVector:
        ((void (*)(void))0x00000010)();
        break;

    case prf_kof_jumpToIllegalInstr:
        /* This test case causes a problem with connected debugger: the illegal instruction
           is considered a break point by the debugger and we get a break instead of a
           continuating SW run. Without a debugger connected it's fine. */
#ifndef DEBUG
        ((void (*)(void))0x00000008)();
#else
        /* We compile a substitute to avoid disrupted execution with debugger. */
        ((void (*)(void))0x00000010)();
#endif
        break;

    case prf_kof_privilegedInstr:
        asm volatile ("wrteei 1\n\t" ::: /* Clobbers */ "memory");
        break;
        
    case prf_kof_callOsAPI:
        rtos_OS_suspendAllInterruptsByPriority(15);
        break;

    case prf_kof_triggerUnavailableEvent:
        rtos_triggerEvent(syc_idEvTest);
        break;
    
    case prf_kof_writeOsData:
    case prf_kof_writeOtherProcData:
        *(uint32_t*)prf_cmdFailure.address = prf_cmdFailure.value;
        break;

    case prf_kof_infiniteLoop:
        while(true)
            ;

    default:
        assert(false);
    }
} /* End of injectError */


/* The next function needs to be compiled without optimization. The compiler will otherwise
   recognize the useless recursion and kick it out. */
#pragma GCC push_options
#pragma GCC optimize ("O0")
/**
 * Helper function: Calls itself a number of times in order to operate the fault injection
 * on different stack nesting levels. Then it branches into error injection.
 */
static void nestStackInjectError(unsigned int remainingLevels)
{
    if(remainingLevels > 0)
        nestStackInjectError(remainingLevels-1);
    else
        injectError();
        
} /* End of prf_nestStackInjectError */
#pragma GCC pop_options


/**
 * Task function, cyclically activated every 17ms.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process.
 *   @param PID
 * A user task function gets the process ID as first argument.
 */
int32_t prf_taskInjectError(uint32_t PID ATTRIB_UNUSED)
{
    nestStackInjectError(/* remainingLevels */ prf_cmdFailure.noRecursionsBeforeFailure);
    return 0;
    
} /* End of prf_taskInjectError */



/**
 * Task function, cyclically activated every 17ms. The task belongs to process \a
 * syc_pidFailingTasks. In this process it has the lowest priority.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process.
 *   @param PID
 * A user task function gets the process ID as first argument.
 */
int32_t prf_task17ms(uint32_t PID ATTRIB_UNUSED)
{
    return 0;
    
} /* End of prf_task17ms */



/**
 * Task function, directly started from a regular timer ISR (PIT1). The task belongs to
 * process \a syc_pidFailingTasks. In this process it has the highest priority.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process.
 *   @param PID
 * A user task function gets the process ID as first argument.
 *   @param taskParam
 * Different to "normal" RTOS scheduled user tasks may directly started tasks have a task
 * parameter. In this test we just apply it for a consistency check.
 *   
 */
int32_t prf_task1ms(uint32_t PID ATTRIB_UNUSED, uint32_t taskParam)
{
    static uint32_t SDATA_PRC_FAIL(cnt_) = 0;
    
    ++ _cntTask1ms;
    
    /* Normally, taskParam (counts of starts of this task) and local counter will always
       match. But since this task belongs to the failing proces ther are potential crashes
       of this task, too, and we can see a mismatch. We report them as task error and they
       will be counted as further process errors. */
    if(taskParam != cnt_++)
    {
        cnt_ = taskParam+1;
        return -1;
    }
    else
        return 0;
    
} /* End of prf_task1ms */



