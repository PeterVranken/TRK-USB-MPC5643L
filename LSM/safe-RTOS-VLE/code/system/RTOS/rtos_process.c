/**
 * @file rtos_process.c
 * This module implements the process related functionality like querying the number of
 * errors, recognized and counted for a process and suspension of a (failing) process.
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
 *   rtos_grantPermissionSuspendProcess
 *   rtos_scSmplHdlr_suspendProcess
 *   rtos_osReleaseProcess
 *   rtos_osSuspendProcess
 *   rtos_isProcessSuspended
 *   rtos_getNoTotalTaskFailure
 *   rtos_getNoTaskFailure
 *   rtos_getStackReserve
 * Module inline interface
 * Local functions
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "MPC5643L.h"

#include "typ_types.h"
#include "rtos_process.h"
#include "rtos_process_defSysCalls.h"
#include "rtos.h"
#include "rtos_systemCall.h"
#include "rtos_ivorHandler.h"


/*
 * Defines
 */

/** Companion of C's offsetof: The size of a field inside a struct. */
#define sizeoffield(type, fieldName) (sizeof(((type*)0)->fieldName))

/** The C code has an interface with the assembler code. It is used to exchange process and
    task related information. The interface is modeled twice, once as structs for C code
    and once as set of preprocessor macros, which hold size of data structures and offsets
    of fields. Here, we have a macro, which double-checks the equivalence of both
    definitions. The compiler will abort the compilation if there is an inconsistency.\n
      The macro needs to by compiled at least once, anywhere in the code. All checks are
    done at compile-time and the compiler won't emit any machine code; therefore, it
    doesn't matter where to place the macro. */
#define CHECK_INTERFACE_S2C                                                                 \
    static_assert( sizeof(rtos_taskDesc_t) == SIZE_OF_TASK_CONF                             \
                   &&  offsetof(rtos_taskDesc_t, addrTaskFct) == O_TCONF_pFct               \
                   &&  offsetof(rtos_taskDesc_t, addrTaskFct) == 0                          \
                   &&  sizeoffield(rtos_taskDesc_t, addrTaskFct) == 4                       \
                   &&  sizeoffield(rtos_taskDesc_t, addrTaskFct) == sizeof(uintptr_t)       \
                   &&  offsetof(rtos_taskDesc_t, tiTaskMax) == O_TCONF_tiMax                \
                   &&  sizeoffield(rtos_taskDesc_t, tiTaskMax) == 4                         \
                   &&  offsetof(rtos_taskDesc_t, PID) == O_TCONF_pid                        \
                   &&  sizeoffield(rtos_taskDesc_t, PID) == 1                               \
                 , "struct rtos_taskDesc_t: Inconsistent interface between"                 \
                   " assembler and C code"                                                  \
                 );                                                                         \
    static_assert( sizeof(processDesc_t) == SIZE_OF_PROCESS_DESC                            \
                   &&  offsetof(processDesc_t, userSP) == O_PDESC_USP                       \
                   &&  O_PDESC_USP == 0                                                     \
                   &&  offsetof(processDesc_t, state) == O_PDESC_ST                         \
                   &&  sizeoffield(processDesc_t, state) == 1                               \
                   &&  offsetof(processDesc_t, cntTotalTaskFailure) == O_PDESC_CNTTOT       \
                   &&  sizeoffield(processDesc_t, cntTotalTaskFailure) == 4                 \
                   &&  offsetof(processDesc_t, cntTaskFailureAry) == O_PDESC_CNTTARY        \
                   &&  sizeoffield(processDesc_t, cntTaskFailureAry)                        \
                       == RTOS_NO_CAUSES_TASK_ABORTION*4                                    \
                 , "struct processDesc_t: Inconsistent interface between"                   \
                   " assembler and C code"                                                  \
                 );

/*
 * Local type definitions
 */

/* Run-time data describing a process. An object of this type must be allocated in RAM,
   which is not write-permitted for user code. */
typedef struct processDesc_t
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
          See file rtos_ivorHandler.h, #RTOS_CAUSE_TASK_ABBORTION_MACHINE_CHECK and
        following, for the enumerated causes. */
    uint32_t cntTaskFailureAry[RTOS_NO_CAUSES_TASK_ABORTION];

} processDesc_t;


/*
 * Local prototypes
 */


/*
 * Data definitions
 */

/* Forward declaration of pointers to boundaries of stack areas, which are actually
   defined in the linker script. Used to initialize the global process structure. */
extern uint32_t ld_stackEndP1[0], ld_stackEndP2[0], ld_stackEndP3[0], ld_stackEndP4[0];

/** Array holding run-time data for all processes. Note the process IDs have a one based
    index (as 0 is reserved for the kernel process) but this is a normal array with zero
    based index. Use PID-1 as index into the array. */
processDesc_t SECTION(.data.OS.rtos_processAry) rtos_processAry[RTOS_NO_PROCESSES] =
{
    /** Process 1. */
    [0] = { .userSP = (uint32_t)ld_stackEndP1 - 16
            , .state = 0
            , .cntTotalTaskFailure = 0
            , .cntTaskFailureAry = {[0 ... (RTOS_NO_CAUSES_TASK_ABORTION-1)] = 0}
          }

    /** Process 2. */
    , [1] = { .userSP = (uint32_t)ld_stackEndP2 - 16
              , .state = 0
              , .cntTotalTaskFailure = 0
              , .cntTaskFailureAry = {[0 ... (RTOS_NO_CAUSES_TASK_ABORTION-1)] = 0}
            }

    /** Process 3. */
    , [2] = { .userSP = (uint32_t)ld_stackEndP3 - 16
              , .state = 0
              , .cntTotalTaskFailure = 0
              , .cntTaskFailureAry = {[0 ... (RTOS_NO_CAUSES_TASK_ABORTION-1)] = 0}
            }

    /** Process 4. */
    , [3] = { .userSP = (uint32_t)ld_stackEndP2 - 16
              , .state = 0
              , .cntTotalTaskFailure = 0
              , .cntTaskFailureAry = {[0 ... (RTOS_NO_CAUSES_TASK_ABORTION-1)] = 0}
            }
};


/** The option to let a task of process A suspend process B (system call
    rtos_suspendProcess()) is potentially harmful, as a safety relevant supervisory task
    could be hindered from running. This is of course not generally permittable. An
    all-embracing privilege rule cannot be defined because of the different use cases of
    the mechanism. Therefore, we have an explicit table of granted permissions, which can
    be configured at startup time as an element of the operating system initialization
    code.\n
      The bits of the word correspond to the 16 possible combinations of four possible
    caller processes in four possible target processes.
      By default, no permission is granted. */
#if RTOS_NO_PROCESSES == 4
static uint16_t SDATA_OS(_suspendProcess_permissions) = 0;
#else
# error Implementation depends on four being the number of processes
#endif


/*
 * Function implementation
 */

/**
 * Initialize the data structure with all process descriptors. This mainly means to
 * initialize the stack memory.
 *   @return
 * The function returns a non zero value from enumeration \a rtos_errorCode_t if a
 * configuration error is detected. The software must not start up in this case. Normally,
 * you will always receive a \a rtos_err_noError (zero). Since it is only about static
 * configuration, the returned error may be appropriately handled by an assertion.
*   @param isProcessConfiguredAry
 * This array contains an entry for each of the supported processes. After return, the
 * entry with index i tells, whether the process with PID i is configured for use. (Which
 * mainly relates to some stack space configured or not in the linker script.)\n
 *   Note, isProcessConfiguredAry[0] relates to the OS and will always be \a true.
 */
rtos_errorCode_t rtos_initProcesses(bool isProcessConfiguredAry[1+RTOS_NO_PROCESSES])
{
    /* We apply the macro that double-checks the consistency of the two independent
       definitions of the same interface between assembler code and C code. The compiler
       will abort the compilation of this module if there is a discrepancy. */
    CHECK_INTERFACE_S2C

    /* Here, where we have one time called code, we can double-check some static
       constraints of the assembler implementation.
         The configuration table rtos_systemCallDescAry is addressed with a short
       instruction, which requires that it resides at a 15 Bit address. This is ensured by
       the linker script, but better to check it. */
#ifdef DEBUG
    /// @todo Move this code to system call module to avoid the (even wrong) extern statement
    extern const uint8_t rtos_systemCallDescAry[RTOS_NO_SYSTEM_CALLS];
    assert((uintptr_t)rtos_systemCallDescAry < 0x8000);
#endif

    rtos_errorCode_t errCode = rtos_err_noError;

    /* If the kernel process wouldn't be configured correctly then we would never get here. */
    isProcessConfiguredAry[0] = true;

    /* Fill all process stacks with the empty-pattern, which is applied for computing the
       stackusage. */
    extern uint32_t ld_stackStartP1[], ld_stackStartP2[], ld_stackStartP3[], ld_stackStartP4[]
                  , ld_stackEndP1[], ld_stackEndP2[], ld_stackEndP3[], ld_stackEndP4[];
    uint32_t * const stackStartAry[RTOS_NO_PROCESSES] = { [0] = ld_stackStartP1
                                                        , [1] = ld_stackStartP2
                                                        , [2] = ld_stackStartP3
                                                        , [3] = ld_stackStartP4
                                                        };
    const uint32_t * const stackEndAry[RTOS_NO_PROCESSES] = { [0] = ld_stackEndP1
                                                            , [1] = ld_stackEndP2
                                                            , [2] = ld_stackEndP3
                                                            , [3] = ld_stackEndP4
                                                            };
    unsigned int idxP
               , maxPIDInUse = 0;
    for(idxP=0; idxP<RTOS_NO_PROCESSES; ++idxP)
    {
        /* Disable the process by default. */
        rtos_processAry[idxP].state = 0;
        isProcessConfiguredAry[idxP+1] = false;

        /* Stack size: May be zero if process is not used at all. Otherwise we demand a
           reasonable minimum stack size - anything else is almost certain a configuration
           error. */
        const unsigned int sizeOfStack = (unsigned int)stackEndAry[idxP]
                                         - (unsigned int)stackStartAry[idxP];
        if(sizeOfStack > 0)
        {
            if(sizeOfStack >= 256  &&  sizeOfStack <= 0x100000
               &&  ((uintptr_t)stackStartAry[idxP] & 0x7) == 0
               &&  (sizeOfStack & 0x7) == 0
              )
            {
                memset(stackStartAry[idxP], /* c */ 0xa5, sizeOfStack);
                stackStartAry[idxP][sizeOfStack/4 - 4] = 0u;
                stackStartAry[idxP][sizeOfStack/4 - 3] = 0xffffffffu;
                stackStartAry[idxP][sizeOfStack/4 - 2] = 0xffffffffu;
                stackStartAry[idxP][sizeOfStack/4 - 1] = 0xffffffffu;
                rtos_processAry[idxP].userSP = (uint32_t)stackEndAry[idxP] - 16;
                rtos_processAry[idxP].state = 1;

                /* Stack alright, process may be used. */
                isProcessConfiguredAry[idxP+1] = true;

                /* Keep track of the highest PID in use. */
                if(idxP+1 > maxPIDInUse)
                    maxPIDInUse = idxP+1;
            }
            else
                errCode = rtos_err_prcStackInvalid;
        }
        else
            rtos_processAry[idxP].userSP = 0;

        rtos_processAry[idxP].cntTotalTaskFailure = 0;
        memset( &rtos_processAry[idxP].cntTaskFailureAry[0]
              , /* c */ 0
              , sizeOfAry(rtos_processAry[idxP].cntTaskFailureAry)
              );
    } /* End for(All processes) */

    if(errCode == rtos_err_noError)
    {
        /* Caution: Maintenance of this code is required consistently with
           rtos_grantPermissionSuspendProcess() and rtos_scSmplHdlr_suspendProcess(). */
        assert(maxPIDInUse >= 1  &&  maxPIDInUse <= 4);
        const uint16_t mask = 0x1111 << (maxPIDInUse-1);
        if((_suspendProcess_permissions & mask) != 0)
            errCode = rtos_err_suspendPrcBadPermission;
    }

    return errCode;

} /* End of rtos_initProcesses */



/**
 * Operating system initialization function: Grant permissions for using the service
 * rtos_suspendProcess to particular processes. By default, the use of that service is not
 * allowed.\n
 *   By principle, offering service rtos_suspendProcess makes all processes vulnerable,
 * which are allowed as target for the service. A failing, straying process can always hit
 * some ROM code executing the system call with arbitrary register contents, which can then
 * lead to immediate task abortion in and suspension of an otherwise correct process.\n
 *   This does not generally break the safety concept, the potentially harmed process can
 * for example be anyway supervised by another, non-suspendable supervisory process.
 * Consequently, we can offer the service at least on demand. A call of this function
 * enables the service for a particular pair of calling process and targeted process.
 *   @param pidOfCallingTask
 * The tasks belonging to process with PID \a pidOfCallingTask are granted permission to
 * suspend another process. The range is 1 .. #RTOS_NO_PROCESSES, which is double-checked by
 * assertion.
 *   @param targetPID
 * The process with PID \a targetPID is suspended. The range is 1 .. maxPIDInUse-1, which is
 * double-checked later.
 *   @remark
 * It would break the safety concept if we permitted the process with highest privileges to
 * become the target of the service. This is double-checked not here (when it is not yet
 * defined, which particular process that will be) but as part of the RTOS startup
 * procedure; a bad configuration can therefore lead to a later reported run-time error.
 *   @remark
 * This function must be called from the OS context only. It is intended for use in the
 * operating system initialization phase. It is not reentrant. The function needs to be
 * called prior to rtos_osInitKernel().
 */
void rtos_grantPermissionSuspendProcess(unsigned int pidOfCallingTask, unsigned int targetPID)
{
    /* targetPID <= 3: Necessary but not sufficient to double-check
       "targetPID <= maxPIDInUse-1". */
    assert(pidOfCallingTask >= 1  &&  pidOfCallingTask <= 4
           &&  targetPID >= 1  &&  targetPID <= 3
          );

    /* Caution, the code here depends on macro RTOS_NO_PROCESSES being four and needs to be
       consistent with the implementation of rtos_scSmplHdlr_suspendProcess(). */
#if RTOS_NO_PROCESSES != 4
# error Implementation requires the number of processes to be four
#endif
    const unsigned int idxCalledPrc = targetPID - 1u;
    const uint16_t mask = 0x1 << (4u*(pidOfCallingTask-1u) + idxCalledPrc);
    _suspendProcess_permissions |= mask;

} /* End of rtos_grantPermissionSuspendProcess */



/**
 * System call implementation to suspend a process. All currently running tasks belonging
 * to the process are aborted and the process is stopped forever (i.e. there won't be
 * further task starts or I/O driver callback invocations).\n
 *   Suspending a process of PID i is permitted only to processes of PID j>i.
 *   @param pidOfCallingTask
 * Process ID of calling user task.
 *   @param PID
 * The ID of the process to suspend. Needs to be not zero (OS process) and lower than the
 * ID of the calling process. Otherwise the calling task is aborted with exception
 * #RTOS_ERR_PRC_SYS_CALL_BAD_ARG.
 *   @remark
 * The system call is the implementation of a system call of conformance class "simple".
 * Such a system call can be implemented in C but it needs to be run with all interrupts
 * suspended. It cannot be preempted. Suitable for short running services only.\n
 *   @remark
 * Tasks of the suspended process can continue running for a short while until their abort
 * conditions are checked the next time. The likelihood of such a continuation is little
 * and the duration is in the magnitude of a Millisecond.
 *   @remark
 * This function must never be called directly. The function is only made for placing it in
 * the global system call table.
 */
void rtos_scSmplHdlr_suspendProcess(uint32_t pidOfCallingTask, uint32_t PID)
{
    /* This code depends on specific number of processes, we need a check. The
       implementation requires consistent maintenance with other function
       rtos_grantPermissionSuspendProcess() */
#if RTOS_NO_PROCESSES != 4
# error Implementation requires the number of processes to be four
#endif

    /* Now we can check the index of the target process. */
    const unsigned int idxCalledPrc = PID - 1u;
    if(idxCalledPrc > 3)
        rtos_systemCallBadArgument();

    const uint16_t mask = 0x1 << (4u*(pidOfCallingTask-1u) + idxCalledPrc);
    if((_suspendProcess_permissions & mask) != 0)
        rtos_osSuspendProcess(PID);
    else
    {
        /* The calling process doesn't have enough privileges to suspend the aimed process.
           This is a severe user code error, which is handled with an exception, task abort
           and counted error.
             Note, this function does not return. */
        rtos_systemCallBadArgument();
    }
} /* End of rtos_scSmplHdlr_suspendProcess */




/**
 * Kernel function to initially release a process. "Initially" means that no state machine
 * is implemented, which would allow to alternatingly suspend and resume the operation of a
 * process. After startup, all processes are suspended. When the kernel is initialized it
 * may call this function once for each of the processes. It must however never se it again
 * for a process, e.g. after a call of rtos_osSuspendProcess().
 *   @param PID
 * The ID of the process to suspend in the range 1..4. Checked by assertion.
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user
 * code will lead to a privileged exception.
 */
void rtos_osReleaseProcess(uint32_t PID)
{
    /* The process array has no entry for the kernel process. An index offset by one
       results. */
    -- PID;

    assert(PID < sizeOfAry(rtos_processAry));
    rtos_processAry[PID].state = 1;

} /* End of rtos_osReleaseProcess */



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
void rtos_osSuspendProcess(uint32_t PID)
{
    /* The process array has no entry for the kernel process. An index offset by one
       results. */
    -- PID;

    assert(PID < sizeOfAry(rtos_processAry));
    rtos_processAry[PID].state = 0;

} /* End of rtos_osSuspendProcess */



/**
 * Kernel function to read the suspend status of a process. This function is a simple
 * counterpart to rtos_osSuspendProcess(). It will return \a true after the other function
 * had been called for the given process ID or if the process is not at all in use.
 *   @param PID
 * The ID of the queried process in the range 1..4. Checked by assertion.
 *   @remark
 * This function can be called from OS and user context.
 */
bool rtos_isProcessSuspended(uint32_t PID)
{
    /* The process array has no entry for the kernel process. An index offset by one
       results. */
    -- PID;

    assert(PID < sizeOfAry(rtos_processAry));
    return rtos_processAry[PID].state == 0;

} /* End of rtos_isProcessSuspended */


/**
 * Get the number of task failures (and task abortions at the same time) counted for the
 * given process since start of the kernel.
 *   @return
 * Get total number of errors counted for process \a PID.
 *   @param PID
 * The ID of the queried process in the range 1 .. RTOS_NO_PROCESSES. An out of range PID
 * will always yield UINT_MAX and an assertion fires in DEBUG compilation. An unused
 * process has no errors.
 *   @remark
 * This function can be called from both, a user task or the OS context.
 */
unsigned int rtos_getNoTotalTaskFailure(unsigned int PID)
{
    if(--PID < RTOS_NO_PROCESSES)
        return rtos_processAry[PID].cntTotalTaskFailure;
    else
    {
        assert(false);
        return UINT_MAX;
    }
} /* End of rtos_getNoTotalTaskFailure */




/**
 * Get the number of task failures of given category counted for the given process since
 * start of the kernel.
 *   @return
 * Get total number of errors of category \a kindOfErr counted for process \a PID.
 *   @param PID
 * The ID of the queried process in the range 1 .. RTOS_NO_PROCESSES. An out of range PID
 * will always yield UINT_MAX and an assertion fires in DEBUG compilation. An unused
 * process has no errors.
 *   @param kindOfErr
 * The category of the error. See file rtos_ivorHandler.h,
 * #RTOS_ERR_PRC_MACHINE_CHECK and following, for the enumerated error causes.
 *   @remark
 * This function can be called from both, a user task or the OS context.
 */
unsigned int rtos_getNoTaskFailure(unsigned int PID, unsigned int kindOfErr)
{
    if(--PID < RTOS_NO_PROCESSES
       &&  kindOfErr < sizeOfAry(rtos_processAry[PID].cntTaskFailureAry)
      )
    {
        return rtos_processAry[PID].cntTaskFailureAry[kindOfErr];
    }
    else
    {
        assert(false);
        return UINT_MAX;
    }
} /* End of rtos_getNoTaskFailure */



/**
 * Compute how many bytes of the stack area of a process are still unused. If the value is
 * requested after an application has been run a long while and has been forced to run
 * through all its conditional code paths, it may be used to optimize the static stack
 * allocation. The function is useful only for diagnosis purpose as there's no chance to
 * dynamically increase or decrease the stack area at runtime.\n
 *   The function may be called from a task, ISR and from the idle task.\n
 *   The algorithm is as follows: The unused part of the stack is initialized with a
 * specific pattern word. This routine counts the number of subsequent pattern words down
 * from the (logical) top of the stack area. The result is returned as number of bytes.\n
 *   The returned result must not be trusted too much: It could of course be that a pattern
 * word is found not because of the initialization but because it has been pushed onto the
 * stack - in which case the return value is too great (too optimistic). The probability
 * that this happens is significantly greater than zero. The chance that two pattern words
 * had been pushed is however much less and the probability of three, four, five such words
 * in sequence is negligible. Any stack size optimization based on this routine should
 * therefore subtract e.g. eight bytes from the returned reserve and diminish the stack
 * outermost by this modified value.\n
 *   Be careful with operating system stack size optimization based only on this routine.
 * The OS stack takes all interrupt stack frames. Even if the application ran a long time
 * there's a significant probability that there has not yet been the deepest possible
 * nesting of interrupts in the very instance that the code execution was busy in the
 * deepest nested sub-routine of any of the service routines, i.e. when having the largest
 * imaginable stack consumption for the OS stack. (Actually, the likelihood of not seeing
 * this is rather close to one than close to zero.) A good suggestion therefore is to add
 * the product of ISR stack frame size with the number of IRQ priority levels in use to the
 * measured OS stack use and reduce the allocated stack memory only on this basis.\n
 *   The IRQ stack frame is 96 Byte for normal IRQs and 200 Byte for those, which may start
 * a user task (SW IRQs and I/O IRQs with callback into user code).\n
 *   In the worst case, with 15 IRQ priority levels, this can sum up to 3 kByte. The stack
 * reserve of a "safe" application should be in this order of magnitude.
 *   @return
 * The number of still unused stack bytes of the given process. See function description
 * for details.
 *   @param PID
 * The process ID the query relates to. (Each process has its own stack.) ID 0 relates to
 * the OS/kernel stack.
 *   @remark
 * The computation is a linear search for the first non-pattern word and thus relatively
 * expensive. It's suggested to call it only in some specific diagnosis compilation or
 * occasionally from the idle task.
 *   @remark
 * This function can be called from both, the OS context and a user task.
 */
unsigned int rtos_getStackReserve(unsigned int PID)
{
    if(PID <= RTOS_NO_PROCESSES)
    {
        /* The stack area is defined by the linker script. We can access the information by
           declaring the linker defined symbols. */
        extern uint32_t ld_stackStartOS[], ld_stackStartP1[], ld_stackStartP2[]
                      , ld_stackStartP3[], ld_stackStartP4[]
                      , ld_stackEndOS[], ld_stackEndP1[], ld_stackEndP2[]
                      , ld_stackEndP3[], ld_stackEndP4[];
        static const uint32_t const *stackStartAry_[RTOS_NO_PROCESSES+1] =
                                                        { [0] = ld_stackStartOS
                                                        , [1] = ld_stackStartP1
                                                        , [2] = ld_stackStartP2
                                                        , [3] = ld_stackStartP3
                                                        , [4] = ld_stackStartP4
                                                        }
                                  , *stackEndAry_[RTOS_NO_PROCESSES+1] =
                                                        { [0] = ld_stackEndOS
                                                        , [1] = ld_stackEndP1
                                                        , [2] = ld_stackEndP2
                                                        , [3] = ld_stackEndP3
                                                        , [4] = ld_stackEndP4
                                                        };
        const uint32_t *sp = stackStartAry_[PID];
        if((intptr_t)stackEndAry_[PID] - (intptr_t)sp >= (intptr_t)sizeof(uint32_t))
        {
            /* The bottom of the stack is always initialized with a non pattern word (e.g.
               there's an illegal return address 0xffffffff). Therefore we don't need a
               limitation of the search loop - it'll always find a non-pattern word in the
               stack area. */
            while(*sp == 0xa5a5a5a5)
                ++ sp;
            return (uintptr_t)sp - (uintptr_t)stackStartAry_[PID];
        }
        else
            return 0;
    }
    else
        return 0;

} /* End of rtos_getStackReserve */

