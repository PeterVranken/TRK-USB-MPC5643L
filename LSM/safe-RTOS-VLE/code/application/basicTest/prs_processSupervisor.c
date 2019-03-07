/**
 * @file prs_processSupervisor.c
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
 * Local functions
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
#include "lbd_ledAndButtonDriver.h"
#include "syc_systemConfiguration.h"
#include "prf_processFailureInjection.h"
#include "prs_processSupervisor.h"


/*
 * Defines
 */


/*
 * Local type definitions
 */

/** The type of the prediction of the consequence of a command to inject the next error. */
typedef struct failureExpectation
{
    /** Expected number of process errors resulting from the failure. */
    unsigned int expectedNoProcessFailures;

    /** The injected error can cause subsequent errors due to the other tasks belonging to
        the process - they can be harmed, too. Therefore we don't look at an exact
        increment by one of reported errors but tolerante a few more. */
    unsigned int expectedNoProcessFailuresTolerance;

    /** Expected value for test case result. */
    uint32_t expectedValue;

} failureExpectation_t;




/*
 * Local prototypes
 */


/*
 * Data definitions
 */

/** Counter for test cycles. */
static long unsigned int DATA_PRC_SV(_cntTestCycles) = 0;

/** Expected test result. Set by prs_taskCommandError and tested by prs_taskEvaluateError
    after run of failure injection task prf_taskInjectError. */
static failureExpectation_t DATA_PRC_SV(_failureExpectation);


/*
 * Function implementation
 */

/**
 * Task function, cyclically activated every 17ms.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process.
 *   @param PID
 * A user task function gets the process ID as first argument.
 */
int32_t prs_taskCommandError(uint32_t PID ATTRIB_UNUSED)
{
    /* First cycles without any special action to prove basic operation of the software. */
    if(_cntTestCycles < 100)
    {
        /* At the beginning we should be error free. */
        if(_cntTestCycles == 0)
        {
            prf_cmdFailure = (prf_cmdFailure_t){ .kindOfFailure = prf_kof_noFailure
                                                 , .noRecursionsBeforeFailure = 0
                                                 , .value = 0
                                                 , .address = 0
                                                 , .expectedValue = 0
                                               };
        }

        return 0;
    }

    const unsigned int kindOfFailure = _cntTestCycles % (unsigned)prf_kof_noFailureTypes
                     , stackDepth = _cntTestCycles & 64;
    unsigned int noFailures = rtos_getNoTotalTaskFailure(syc_pidFailingTasks) + 1
               , tolerance = 2
               , expectedValue = 0;
    uint32_t value = 0
           , address = 0;

    switch(kindOfFailure)
    {
    case prf_kof_noFailure:
        noFailures -= 1;
        tolerance = 0;
        break;

    case prf_kof_userTaskError:
        /* Voluntary task termination with error code must be reported as error but it
           still needs to be clean termination without a possibly harmfully affected
           other task. Tolerance is zero. */
        tolerance = 0;
        break;

    case prf_kof_writeOsData:
        /* We need to take an address, where we can be sure that no change from other side
           will happen so that we can later double check that the write attempt really
           didn't alter the value. */
        expectedValue = prc_processAry[syc_pidSupervisor-1].cntTotalTaskFailure;
        value = ~expectedValue;
        address = (uint32_t)&prc_processAry[syc_pidSupervisor-1].cntTotalTaskFailure;
        tolerance = 0;
        break;
        
    case prf_kof_writeOtherProcData:
        expectedValue = _cntTestCycles;
        value = ~expectedValue;
        address = (uint32_t)&_cntTestCycles;
        tolerance = 0;
        break;
    
    case prf_kof_privilegedInstr:
    case prf_kof_triggerUnavailableEvent:
    case prf_kof_infiniteLoop:
        /* Test cases which cause an exception without any danger of destroying some still
           accessible properties like process owned data don't need a tolerance in the
           potential number of process failures. */
        tolerance = 0;
        break;

    default:
        /* Many test cases have the standard expectation: 1..3 reported process failures
           but no particular result to check. They go all here. */
        break;

    } /* End switch(Which test case yields which failure?) */

    prf_cmdFailure = (prf_cmdFailure_t){ .kindOfFailure = kindOfFailure
                                         , .noRecursionsBeforeFailure = stackDepth
                                         , .value = value
                                         , .address = address
                                         , .expectedValue = expectedValue
                                       };
    _failureExpectation = (failureExpectation_t)
                          { .expectedNoProcessFailures = noFailures
                            , .expectedNoProcessFailuresTolerance = tolerance
                            , .expectedValue = expectedValue
                          };
    return 0;

} /* End of prs_taskCommandError */



/**
 * Task function, cyclically activated every 17ms.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process.
 *   @param PID
 * A user task function gets the process ID as first argument.
 */
int32_t prs_taskEvaluateError(uint32_t PID ATTRIB_UNUSED)
{
    /** @todo Long lasting test could run into the saturation of the failure counter. We
        must not interpret this unexpected behavior. */
    const unsigned int noFailures = rtos_getNoTotalTaskFailure(syc_pidFailingTasks);

    bool testOkThisTime =
            noFailures >= _failureExpectation.expectedNoProcessFailures
            &&  noFailures <= _failureExpectation.expectedNoProcessFailures
                              + _failureExpectation.expectedNoProcessFailuresTolerance;

    switch(prf_cmdFailure.kindOfFailure)
    {
    case prf_kof_writeOsData:
    case prf_kof_writeOtherProcData:
        if(_failureExpectation.expectedValue != *(const uint32_t*)prf_cmdFailure.address)
            testOkThisTime = false;
        break;

    default:
        /* Many test cases don't require additional attention. No action. */
        break;
    }

    /* In DEBUG compilation we can halt the software execution to point to the problem. */
    if(!testOkThisTime)
    {
        /* Make this visible even if no debugger or terminal is connected. */
        lbd_setLED(lbd_led_D4_grn, /* isOn */ false);
        lbd_setLED(lbd_led_D4_red, /* isOn */ true);
        assert(false);
    }

    ++ _cntTestCycles;

    /* PRODUCTION compilation: If we return a task error here then we will see a process
       error and the watchdog task will halt the further SW execution. */
    return testOkThisTime? 0: -1;

} /* End of prs_taskEvaluateError */



/**
 * The watchdog task. It is running at highest intended RTOS user task priority. This
 * priority level is protected against locks by user code (user code cannot implement a
 * critical section with such a task). The watchdog checks whether the supervisor task is
 * running as expected. (Alive counter, number of errors.)\n
 *   This is a non cyclic task, which is activated by software trigger. The trigger
 * is regular but asynchronous to the normal RTOS scheduler. (This has no particular
 * advantage besides more intensive testing of context switches.)
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process.
 *   @param PID
 * A user task function gets the process ID as first argument.
 */
int32_t prs_taskWatchdog(uint32_t PID ATTRIB_UNUSED)
{
    /// @todo Stack check every Millisend costs about 15% CPU load. We don't need to do
    // this so often: Pi stacks are any way protected and OS could be checked every 100ms
    bool isOk = rtos_getNoActivationLoss(syc_idEvTest) == 0
                &&  rtos_getNoActivationLoss(syc_idEvPIT2) == 0
                &&  rtos_getNoTotalTaskFailure(syc_pidSupervisor) == 0
                &&  rtos_getNoTotalTaskFailure(syc_pidReporting) == 0
                &&  rtos_getStackReserve(syc_pidSupervisor) >= 512
                &&  rtos_getStackReserve(syc_pidReporting) >= 512
                &&  rtos_getStackReserve(/* PID */ 0 /* OS */) >= 3096;

    /* In PRODUCTION compilation we can't halt the system using the assert macro. We turn
       on the red LED to indicate a problem and enter an infinite loop. Since the watchdog
       has the highest user task priority this means effectively halting the software
       execution. Just some interrupts without further effect will remain. */
    if(!isOk)
    {
        lbd_setLED(lbd_led_D4_grn, /* isOn */ false);
        lbd_setLED(lbd_led_D4_red, /* isOn */ true);
        while(true)
            ;
    }

    return 0;

} /* End of prs_taskWatchdog */



