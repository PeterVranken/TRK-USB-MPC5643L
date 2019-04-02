#ifndef PRC_PROCESS_INCLUDED
#define PRC_PROCESS_INCLUDED
/**
 * @file prc_process.h
 * Definition of global interface of module prc_process.c
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

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>

#include "typ_types.h"
#include "ivr_ivorHandler.h"


/*
 * Defines
 */

/** The number of configured processes.\n
      Although it looks like a matter of application dependent configuration, this is a
    fixed setting in our RTOS. We have the HW constraint of a limited number of memory
    region descriptors in MMU and MPU. Four processes can be comfortably supported with
    enough descriptors each. Having less regions per process is possible with reduced
    programming comfort and so could have more processes even without changing the
    run-time code (i.e. dynamic change of regions). However, the aimed use cases of this
    RTOS, applications with higher safety integrity level, can be handled with two or three
    processes so that pre-configured four should always be fine.\n
      The big advantage of having a fixed number of processes is the avoidance of
    configuration changes. The MMU and MPU configuration would need changes on C code level
    and the linker script would need more or altered section filters.\n
      No using all pre-configured processes is doesn't matter. Just use a process in a task
    specification or leave it. An unused process doesn't produce any overhead. */
#define PRC_NO_PROCESSES    4


/** Index of implemented system call for aborting running tasks belonging to a given
    process and stopping that process forever (i.e. no further task or I/O driver callback
    execution). */
#define PRC_SYSCALL_SUSPEND_PROCESS     9

/** Helper for data initialization: Task time budget are internally represented in CPU
    clock ticks. Using this macro one can specify it nore conveniently in Milliseconds. The
    macro just converts its argument from Milliseconds to clock ticks. */
#define PRC_TI_MS2TICKS(tiInMs) ((tiInMs)*120000u)


/*
 * Global type definitions
 */

/** Type of a single interrupt service as registered with function
    prc_installINTCInterruptHandler(). */
typedef void (*prc_interruptServiceRoutine_t)(void);


/** Type of user task function, which is run in user mode and with process ID \a PID. This
    function signature is applied to regular tasks or callbacks - which is a distinction
    not in the implementing assembler code but in the upper SW layers only.\n
      @return
    The user code may return a positive value to the calling context. If the RTOS scheduler
    is the calling context then it'll normally not expect or evaluate the task return
    value. However, even here the value matters: Any negative value is interprted as error
    code and counted as error #IVR_CAUSE_TASK_ABBORTION_USER_ABORT in the owing process.
    This may have a safety impact, there might be a supervisory task that observes the
    number of process errors and potentially shutdown the system. Therefore a task will
    normally return zero.
      @param PID
    The task implementation receives the process ID it is invoked in. This enables having
    one callback implementation shared between different tasks.
      @param taskParam
    A value meaningless to the task execution mechanism. Specified by the task creating
    context and simply conveyed to the task implementation. */
typedef int32_t (*prc_taskFct_t)(uint32_t PID, uint32_t taskParam);


/** Configuration data of a user task. An object of this type can be kept in ROM. */
typedef struct prc_userTaskConfig_t
{
    /** User task function, which is run in user mode and with process ID \a PID.\n
          In the assembler code, this field is addressed to by offset O_TCONF_pFct. */
    prc_taskFct_t taskFct;

    /** Time budget for the user task in ticks of TBL (i.e. 8.33ns). This budget is
        granted for each activation of the task. The budget relates to deadline monitoring,
        i.e. it is a world time budget, not an execution time budget.\n
          Macro #PRC_TI_MS2TICKS can be used to state the time budget in Milliseconds.\n
          A value of zero means that deadline monitoring is disabled for the task.\n
          Note, this field doesn't care if \a userTask is NULL, i.e. if no user task is
        defined for the interrupt.\n
          In the assembler code, this field is addressed to by offset O_TCONF_tiMax. */
    uint32_t tiTaskMax;

    /** The process ID of the userTask in the range 1..#PRC_NO_PROCESSES (PID 0 is reserved
        for kernel operation). At the same time index into the array of process
        descriptors.\n
          In the assembler code, this field is addressed to by offset O_TCONF_pid. */
    uint8_t PID;

} prc_userTaskConfig_t;


/* Run-time data describing a process. An object of this type must be allocated in RAM,
   which is not write-permitted for user code. */
typedef struct prc_processDesc_t
{
    /** When preempting a task that belongs to this process then the IVOR #4 handler will
        store the current user mode stack pointer value in \a userSP. The stored value may
        be used later as initial stack pointer value of a newly started task from the same
        process.\n
          In the assembler code addressed to by offset #O_PDESC_USP. */
    uint32_t userSP;

    /** The state of the process. This field is e.g. checked at the end of a preemption of
        a task of this process to see if the task may be continued or if the process has
        been stopped meanwhile.\n
          A non zero value means process is running, zero means process stopped.\n
          In the assembler code addressed to by offset #O_PDESC_ST. */
    uint8_t state;

    /** The total count of errors for the process since the start of the kernel. Or the sum
        of all element in array \a cntTaskFailureAry. Or total number of abnormal abortions
        of tasks belonging to the process. */
    uint32_t cntTotalTaskFailure;

    /** This is an array of counters for task termination. The tasks of a process are not
        separated in these counters. Each array entry means another cause, where a cause
        normally is a specific CPU exception.\n
          See file ivr_ivorHandler.h, #IVR_CAUSE_TASK_ABBORTION_MACHINE_CHECK and
        following, for the enumerated causes. */
    uint32_t cntTaskFailureAry[IVR_NO_CAUSES_TASK_ABORTION];

} prc_processDesc_t;


/*
 * Global data declarations
 */

/** Array holding run-time data for all processes. */
extern prc_processDesc_t prc_processAry[PRC_NO_PROCESSES];


/*
 * Global prototypes
 */

/** Initialize the interrupt controller INTC. */
void prc_initINTCInterruptController(void);

/** Install an interrupt service for a given I/O device. */
/// @todo Make function disappear and have a ROM table as for system calls
void prc_installINTCInterruptHandler( prc_interruptServiceRoutine_t interruptServiceRoutine
                                    , unsigned short vectorNum
                                    , unsigned char psrPriority
                                    , bool isPreemptable
                                    );

/** Initialize the data structure with all process descriptors. */
bool prc_initProcesses(bool isProcessConfiguredAry[1+PRC_NO_PROCESSES]);

/** Grant permission to particular processes for using the service rtos_suspendProcess(). */
void prc_grantPermissionSuspendProcess(unsigned int pidOfCallingTask, unsigned int targetPID);


/*
 * Inline functions
 */

/// @todo We have different styles of having all user APIs in the name space rtos_*. Here, we hurt the rule that the symbol prefix needs to match the file name prefix at other location we use a #define to make a correctly named function appear in the desired name space

/**
 * Kernel function to suspend a process. All currently running tasks belonging to the process
 * are aborted and the process is stopped forever (i.e. there won't be further task starts
 * or I/O driver callback invocations).
 *   @param PID
 * The ID of the process to suspend in the range 1..4. Checked by assertion.
 *   @remark
 * Tasks of the suspended process can continue running for a short while until their abort
 * conditions are checked the next time. The likelihood of such a continuation is little
 * and the duration is in the magnitude of a Millisecond.
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user
 * code will lead to a privileged exception.
 */
static inline void rtos_OS_suspendProcess(uint32_t PID)
{
    /* The process array has no entry for the kernel process. An index offset by one
       results. */
    -- PID;
    
    assert(PID < sizeOfAry(prc_processAry));
    prc_processAry[PID].state = 0;

} /* End of rtos_OS_suspendProcess */



/**
 * Kernel function to read the suspend status of a process. This function is a simple
 * counterpart to rtos_OS_suspendProcess(). It will return \a true after the other function
 * had been called for the given process ID or if the process is not at all in use.
 *   @param PID
 * The ID of the queried process in the range 1..4. Checked by assertion.
 *   @remark
 * This function can be called from OS and user context.
 */
static inline bool rtos_isProcessSuspended(uint32_t PID)
{
    /* The process array has no entry for the kernel process. An index offset by one
       results. */
    -- PID;
    
    assert(PID < sizeOfAry(prc_processAry));
    return prc_processAry[PID].state == 0;

} /* End of rtos_isProcessSuspended */


#endif  /* PRC_PROCESS_INCLUDED */
