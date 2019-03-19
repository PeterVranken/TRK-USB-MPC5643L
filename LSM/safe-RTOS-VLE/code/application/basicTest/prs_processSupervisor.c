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
long unsigned int SDATA_PRC_SV(prs_cntTestCycles) = 0;

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
    if(prs_cntTestCycles < 107)
    {
        /* At the beginning we should be error free. */
        if(prs_cntTestCycles == 0)
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

    const unsigned int kindOfFailure = prs_cntTestCycles % (unsigned)prf_kof_noFailureTypes;
    unsigned int stackDepth = prs_cntTestCycles & 64
               , minNoExpectedFailures = 1 /* Default: Immediate failure of injecting task */ 
               , maxNoExpectedFailures = 3 /* Tolerance: The two other tasks of the process
                                              may easily fail, too. */
               , expectedValue = 0;
    uint32_t value = 0
           , address = 0;

    switch(kindOfFailure)
    {
#if PRF_ENA_TC_PRF_KOF_NO_FAILURE == 1
    case prf_kof_noFailure:
        minNoExpectedFailures = 0;
        maxNoExpectedFailures = 0;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_USER_TASK_ERROR == 1
    case prf_kof_userTaskError:
        /* Voluntary task termination with error code must be reported as error but it
           still needs to be clean termination without a possibly harmfully affected
           other task. Tolerance is zero. */
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_PRIVILEGED_INSTR == 1
    case prf_kof_privilegedInstr:
        /* Test cases which cause an exception without any danger of destroying some still
           accessible properties like process owned data don't need a tolerance in the
           potential number of process failures. */
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_TRIGGER_UNAVAILABLE_EVENT == 1
    case prf_kof_triggerUnavailableEvent:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_WRITE_OS_DATA == 1
    case prf_kof_writeOsData:
        /* We need to take an address, where we can be sure that no change from other side
           will happen so that we can later double check that the write attempt really
           didn't alter the value. */
        expectedValue = prc_processAry[syc_pidSupervisor-1].cntTotalTaskFailure;
        value = ~expectedValue;
        address = (uint32_t)&prc_processAry[syc_pidSupervisor-1].cntTotalTaskFailure;
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_WRITE_OTHER_PROC_DATA == 1
    case prf_kof_writeOtherProcData:
        expectedValue = prs_cntTestCycles;
        value = ~expectedValue;
        address = (uint32_t)&prs_cntTestCycles;
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_WRITE_ROM == 1
    case prf_kof_writeROM:
        address = 0x00000100;
        expectedValue = *(const uint32_t*)address;
        value = ~expectedValue;
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_WRITE_PERIPHERAL == 1
    case prf_kof_writePeripheral:
# define INTC_CPR_PRC0      (0xfff48000u+0x8)
        address = INTC_CPR_PRC0;
        value = 15;
        maxNoExpectedFailures = 1;
        break;
# undef INTC_CPR_PRC0
#endif

#if PRF_ENA_TC_PRF_KOF_READ_PERIPHERAL == 1
# define INTC_CPR_PRC0      (0xfff48000u+0x8)
    case prf_kof_readPeripheral:
        address = INTC_CPR_PRC0;
        maxNoExpectedFailures = 1;
        break;
# undef INTC_CPR_PRC0
#endif

#if PRF_ENA_TC_PRF_KOF_INFINITE_LOOP == 1
    case prf_kof_infiniteLoop:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_MISALIGNED_WRITE == 1
    case prf_kof_misalignedWrite:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_MISALIGNED_READ == 1
    case prf_kof_misalignedRead:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_STACK_OVERFLOW == 1
    case prf_kof_stackOverflow:
        /* This test case should not flood the entire RAM. We try to keep the overflow
           little. This makes the literal here a maintenance issue. The recursion consumes
           24 Byte per stack frame (look for "nestStackInjectError:" in compiler artifact
           prf_processFailureInjection.s) and the stack is configured to have 2048 Byte. */
        stackDepth = 100;
        minNoExpectedFailures = 0;
        maxNoExpectedFailures = 3;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_STACK_CLEAR_BOTTOM == 1
    case prf_kof_stackClearBottom:
        stackDepth = 63;
        minNoExpectedFailures = 0;
        maxNoExpectedFailures = 3;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_SP_CORRUPT == 1
    case prf_kof_spCorrupt:
        if(stackDepth < 10)
            stackDepth = 10;
        minNoExpectedFailures = 0;
        maxNoExpectedFailures = 3;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_SP_CORRUPT_AND_WAIT == 1
    case prf_kof_spCorruptAndWait:
        if(stackDepth < 10)
            stackDepth = 10;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_PRIVILEGED_AND_MPU == 1
    case prf_kof_privilegedAndMPU:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_READ_SPR == 1
    case prf_kof_readSPR:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_WRITE_SPR == 1
    case prf_kof_writeSPR:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_WRITE_SVSP == 1
    case prf_kof_writeSVSP:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_CLEAR_SDA_PTRS == 1
    case prf_kof_clearSDAPtrs:
        minNoExpectedFailures = 0;
        maxNoExpectedFailures = 3;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_CLEAR_SDA_PTRS_AND_WAIT == 1
    case prf_kof_clearSDAPtrsAndWait:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_MMU_WRITE == 1
    case prf_kof_MMUWrite:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_MMU_READ == 1
    case prf_kof_MMURead:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_MMU_EXECUTE == 1
    case prf_kof_MMUExecute:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_MMU_EXECUTE_2 == 1
    case prf_kof_MMUExecute2:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_TRAP == 1
    case prf_kof_trap:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_TLB_INSTR == 1
    case prf_kof_tlbInstr:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_SPE_INSTR == 1
    case prf_kof_SPEInstr:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_BOOKE_FPU_INSTR == 1
    case prf_kof_BookEFPUInstr:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_DCACHE_INSTR == 1
    case prf_kof_dCacheInstr:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_UNDEF_SYS_CALL == 1
    case prf_kof_undefSysCall:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_RANDOM_WRITE == 1
    case prf_kof_randomWrite:
        minNoExpectedFailures = 0;
        maxNoExpectedFailures = 3;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_RANDOM_READ == 1
    case prf_kof_randomRead:
        minNoExpectedFailures = 0;
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_RANDOM_JUMP == 1
    case prf_kof_randomJump:
        minNoExpectedFailures = 0;
        maxNoExpectedFailures = 3;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_MPU_EXC_BEFORE_SC == 1
    case prf_kof_mpuExcBeforeSc:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_INVALID_CRIT_SEC == 1
    case prf_kof_invalidCritSec:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_LEAVE_CRIT_SEC == 1
    case prf_kof_leaveCritSec:
        minNoExpectedFailures = 0;
        maxNoExpectedFailures = 0;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_INVOKE_RTOS_OS_RUN_TASK == 1
    case prf_kof_invokeRtosOsRunTask:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_INVOKE_IVR_SYSTEM_CALL_BAD_ARGUMENT == 1
    case prf_kof_invokeIvrSystemCallBadArgument:
        maxNoExpectedFailures = 1;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_SYSTEM_CALL_ALL_ARGUMENTS_OKAY == 1
    case prf_kof_systemCallAllArgumentsOkay:
        minNoExpectedFailures = 0;
        maxNoExpectedFailures = 0;
#endif

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
                          { .expectedNoProcessFailures =
                                            rtos_getNoTotalTaskFailure(syc_pidFailingTasks)
                                            + minNoExpectedFailures
                            , .expectedNoProcessFailuresTolerance = maxNoExpectedFailures
                                                                    - minNoExpectedFailures
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
uint32_t SDATA_PRC_SV(prs_tmpCntCalls) = 0
       , SDATA_PRC_SV(prs_tmpBreakAtCntCalls) = 0xffffffff;

int32_t prs_taskEvaluateError(uint32_t PID ATTRIB_UNUSED)
{
    if(prs_tmpCntCalls == prs_tmpBreakAtCntCalls)
        prs_tmpCntCalls = 0;
    else
        ++ prs_tmpCntCalls;

    /** @todo Long lasting test could run into the saturation of the failure counter. We
        must not interpret this unexpected behavior. */
    const unsigned int noFailures = rtos_getNoTotalTaskFailure(syc_pidFailingTasks);

    bool testOkThisTime =
            noFailures >= _failureExpectation.expectedNoProcessFailures
            &&  noFailures <= _failureExpectation.expectedNoProcessFailures
                              + _failureExpectation.expectedNoProcessFailuresTolerance;

    switch(prf_cmdFailure.kindOfFailure)
    {
#if PRF_ENA_TC_PRF_KOF_WRITE_OS_DATA == 1
    case prf_kof_writeOsData:
        if(_failureExpectation.expectedValue != *(const uint32_t*)prf_cmdFailure.address)
            testOkThisTime = false;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_WRITE_OTHER_PROC_DATA == 1
    case prf_kof_writeOtherProcData:
        if(_failureExpectation.expectedValue != *(const uint32_t*)prf_cmdFailure.address)
            testOkThisTime = false;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_WRITE_ROM == 1
    case prf_kof_writeROM:
        if(_failureExpectation.expectedValue != *(const uint32_t*)prf_cmdFailure.address)
            testOkThisTime = false;
        break;
#endif

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

    ++ prs_cntTestCycles;

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
    /// @todo Add SW alive counters in all processes/tasks and add a need-to-change
    // condition here
#if 0
    bool isOk = rtos_getNoActivationLoss(syc_idEvTest) == 0
                &&  rtos_getNoActivationLoss(syc_idEvPIT2) == 0
                &&  rtos_getNoTotalTaskFailure(syc_pidSupervisor) == 0
                &&  rtos_getNoTotalTaskFailure(syc_pidReporting) == 0
                &&  rtos_getStackReserve(syc_pidSupervisor) >= 512
                &&  rtos_getStackReserve(syc_pidReporting) >= 512
                &&  rtos_getStackReserve(/* PID */ 0 /* OS */) >= 3096;
#else
    static uint32_t SDATA_P3(noActLossEvTest_) = 0;
    static uint32_t SDATA_P3(noActLossEvPIT2_) = 0;
    static uint32_t SDATA_P3(noTaskFailSV) = 0;
    static uint32_t SDATA_P3(noTaskFailRep) = 0;
    static uint32_t SDATA_P3(stackResSV) = 0;
    static uint32_t SDATA_P3(stackResRep) = 0;
    static uint32_t SDATA_P3(stackResOS) = 0;
    noActLossEvTest_ = rtos_getNoActivationLoss(syc_idEvTest);
    noActLossEvPIT2_ = rtos_getNoActivationLoss(syc_idEvPIT2);
    noTaskFailSV = rtos_getNoTotalTaskFailure(syc_pidSupervisor);
    noTaskFailRep = rtos_getNoTotalTaskFailure(syc_pidReporting);
    stackResSV = rtos_getStackReserve(syc_pidSupervisor);
    stackResRep = rtos_getStackReserve(syc_pidReporting);
    stackResOS = rtos_getStackReserve(/* PID */ 0 /* OS */);
    bool isOk = noActLossEvTest_ == 0
                &&  noActLossEvPIT2_ == 0
                &&  noTaskFailSV == 0
                &&  noTaskFailRep == 0
                &&  stackResSV >= 512
                &&  stackResRep >= 512
                &&  stackResOS >= 3096;
#endif

    if(isOk)
    {
#define TI_BLINK_IN_MS  750u
#define CNT_STEP    ((int16_t)((float)(UINT16_MAX)/(TI_BLINK_IN_MS) + 0.5f))
        static int16_t SDATA_PRC_SV(cnt_) = 0;
        lbd_setLED(lbd_led_D4_grn, /* isOn */ (cnt_+=CNT_STEP) >= 0);
#undef CNT_STEP
#undef TI_BLINK_IN_MS

        /* The LED driver is not protected in the sense that it grants different privileges
           to different processes. This makes it available to a failing, straying task. We
           indeed see occasional LED switch commands due to arbitrary code execution in the
           failing failure injection task. We correct the LED status regularly to maintain
           the signaling effect of the LED. */
        lbd_setLED(lbd_led_D4_red, /* isOn */ false);
    }
    else
    {
        /* In PRODUCTION compilation we can't halt the system using the assert macro. We turn
           on the red LED to indicate a problem and enter an infinite loop. Since the watchdog
           has the highest user task priority this means effectively halting the software
           execution. Just some interrupts without further effect will remain. */
        lbd_setLED(lbd_led_D4_grn, /* isOn */ false);
        lbd_setLED(lbd_led_D4_red, /* isOn */ true);
        while(true)
            ;
    }

    return 0;

} /* End of prs_taskWatchdog */



