#ifndef RTOS_INCLUDED
#define RTOS_INCLUDED
/**
 * @file rtos.h
 * Definition of global interface of module rtos.c
 *
 * Copyright (C) 2017-2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   rtos_OS_runTask
 *   rtos_terminateTask
 *   rtos_OS_suspendAllInterruptsByPriority
 *   rtos_suspendAllInterruptsByPriority
 *   rtos_OS_resumeAllInterruptsByPriority
 *   rtos_resumeAllInterruptsByPriority
 *   rtos_triggerEvent
 *   rtos_getNoTotalTaskFailure
 *   rtos_getNoTaskFailure
 *   rtos_suspendProcess
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "pcp_sysCallPCP.h"
#include "prc_process.h"
#include "ivr_ivorHandler.h"


/*
 * Defines
 */

/** The period time of the RTOS system timer. Unit is 1ms. */
#define RTOS_CLOCK_TICK_IN_MS       1 /* ms */

/** Priority of the scheduler. Normally this priority is the highest one in use but not
    necessarily. All user code needs to run at lower priority. Interrupts of same or higher
    priority may exist but they cannot have a secured callback into user code (no deadline
    monitoring) and user code cannot create a critical section with them. This makes them
    somewhat difficult in use. */
#define RTOS_KERNEL_PRIORITY        12

/** A user task is activated by an event. The implementation of events as software
    interrupts limits their maximum number to eight. Note, this is not a configuration
    setting, the value cannot be changed. */
#define RTOS_MAX_NO_EVENTS          8

/** The maximum number of user tasks, which can be activated by an event. The chosen number
    is a compile time configuration setting and there are no constraints in changing it
    besides the ammout of reserved RAM space for the resulting table size.\n
      The configured limit applies to the tasks registered with rtos_registerTask() only;
    callbacks from I/O driver, which are much of a user task, too, are not couted here. */
#define RTOS_MAX_NO_USER_TASKS      20

/** Deadline monitoring for tasks is supported up to a task maximum execution time of this
    number of Microseconds: (2^31-1)*T_c/1e-6, T_c is 120e6 (CPU clock rate). */
#define RTOS_TI_DEADLINE_MAX_IN_US  17895697

/** A pseudo event ID. Used to register a process initialization task in the API
    rtos_registerTask(). */
#define RTOS_EVENT_ID_INIT_TASK     (UINT_MAX)

/** \cond Two nested macros are used to convert a constant expression to a string which can be
    used e.g. as part of some inline assembler code.\n
      If for example PI is defined to be (355/113) you could use STR(PI) instead of
    "(355/113)" in the source code. ARG2STR is not called directly. */
#define ARG2STR(x) #x
#define STR(x) ARG2STR(x)
/** \endcond */


/**
 * System call wrapper; entry point into operating system function for user code.\n
 *   This macro doesn't really add functionality. It has only been shaped to make an entry
 * point into the assembly code accessible as C style function call and within the
 * expected namespace.
 *   @return
 * The return value depends on the system call.
 *   @param idxSysCall
 * Each system call is identified by an index. The index is an unsigned integer. The number
 * is the index into system call descriptor table \a sc_systemCallDescAry.\n
 *   The further function arguments depend on the particular system call.
 *   @remark
 * The C signature for system calls is formally not correct. The assembly code only
 * supports function arguments in CPU registers, which limits the total number to eight.
 * The ... stands for 0..7 arguments of up to 32 Bit. If a system call function has more
 * arguments or if it are 64 Bit arguments then the assembly code may not propagate all
 * arguments properly to the actually implementing system call handler and the behavior
 * will be undefined!
 */
#define /*uint32_t*/ rtos_systemCall(/*uint32_t*/ idxSysCall, ...) \
                                                ivr_systemCall(idxSysCall, __VA_ARGS__)

/** System call index of function rtos_triggerEvent(), offered by this module. */
#define RTOS_SYSCALL_TRIGGER_EVENT  5

/** System call index of function rtos_runTask(), offered by this module. */
#define RTOS_SYSCALL_RUN_TASK       10


/*
 * Global type definitions
 */

/** User visible description of an event. An object of this type is used by the client code
    of the RTOS to create an event. (It is not used at runtime in the kernel, see event_t
    instead.) */
typedef struct rtos_eventDesc_t
{
    /** The period time for regularly triggering event in ms.\n
          The permitted range is 0..2^30-1. 0 means no regular, timer controlled trigger
        and the event is only enabled for software trigger using rtos_triggerEvent()
        (permitted for interrupts or other tasks). */
    unsigned int tiCycleInMs;

    /** The first trigger of the event in ms after start of kernel. The permitted range
        is 0..2^30-1.\n
          Note, this setting is useless if a cycle time zero in \a tiCycleInMs
        specifies a non regular event. \a tiFirstActivationInMs needs to be zero in this
        case, too. */
    unsigned int tiFirstActivationInMs;

    /** The priority of the event in the range 1..(#RTOS_KERNEL_PRIORITY-1). Different
        events can share the same priority or have different priorities. The priotrity of
        an event is the priority of all associated tasks at the same time. The execution of
        tasks, which share the priority will be sequenced when they are activated at same
        time or with overlap.\n
          Note the safety constraint that the highest permitted priority,
        #RTOS_KERNEL_PRIORITY-1, is available only to events, which solely have tasks
        associated that belong to the process with highest process ID in use. */
    unsigned int priority;

    /** An event can be triggered by user code, using rtos_triggerEvent(). However, tasks
        belonging to less privileged processes must not generally granted permission to
        trigger events that may activate tasks of higher privileged processes. Since an
        event is not process related, we make the minimum process ID an explicitly configured,
        which is required to trigger this event.\n
          Only tasks belonging to a process with PID >= \a minPIDToTriggerThisEvent are
        permitted to trigger this event.\n
          The range of \a minPIDToTriggerThisEvent is 0 ... (#PRC_NO_PROCESSES+1). 0 and 1
        both mean, all processes may trigger the event, #PRC_NO_PROCESSES+1 means only OS code
        can trigger the event. */
    unsigned int minPIDToTriggerThisEvent;

} rtos_eventDesc_t;


/** User visible description of a task. An object of this type is used by the client code
    of the RTOS to register a task. (It is not used at runtime in the kernel, see
    prc_processDesc_t and prc_userTaskConfig_t instead.) */
typedef struct rtos_taskDesc_t
{
    /** The process the task belongs to by identifier. We have a fixed, limited number of
        four processes plus the kernel process, which has ID 0. The range of of process IDs
        to be used here is 1 .. 4.\n
          Note, the stack area for a process is configured in the linker script. To save
        RAM if a process is not use, its stack size shall be configured zero in the linker
        script. Double-check that this is not the case for any process used here. */
    unsigned int PID;

    /** The task function pointer. Note the different function signatures for task
        functions used in different contexts. */
    union
    {
        /** This is the prototype to be used for normal user tasks and for process
            initialization tasks, which may return a value. The specified function is
            executed under control of the RTOS kernel whenever the event trigger, which the
            task is associated with.\n
              If a user task returns a negative value then it is counted in the process
            (and after a number of errors a supervisory task could force a shutdown of the
            process).\n
              If a process init task returns a negative value then the system won't
            startup. */
        int32_t (*userTaskFct)(uint32_t PID);
        
        /** An event can have a task associated, which is used for the operating system. It
            is run as a normal function call, without supervision. The return value is
            meaningful only if the task is an initialization task: If the return value is
            negative then the system won't startup. */
        int32_t (*osTaskFct)(void);
    };
    
    /** Time budget for the user task in Microseconds. This budget is granted for each
        activation of the task. The budget relates to deadline monitoring, i.e. it is a
        world time budget, not an execution time budget.\n
          Deadline monitoring is supported up to a maximum execution time of
        #RTOS_TI_DEADLINE_MAX_IN_US Microseconds.\n
          A value of zero means that deadline monitoring is disabled for the task.\n
          There's no deadline monitoring for OS tasks. If \a PID is zero then \a
        tiTaskMaxInUS meeds to be zero, too. */
    uint32_t tiTaskMaxInUS;

} rtos_taskDesc_t;



/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Creation of an event. The event can be cyclically triggering or software triggerd. */
unsigned int rtos_createEvent(const rtos_eventDesc_t *pEventDesc);

/** Task registration. */
bool rtos_registerTask(const rtos_taskDesc_t *pTaskDesc, unsigned int idEvent);

/** Kernel initialization. */
bool rtos_initKernel(void);

/** The operating system's scheduler function. */
void rtos_onOsTimerTick(void);

/** Software triggered task activation. Can be called from OS context (incl. interrupts). */
bool rtos_OS_triggerEvent(unsigned int idTask);

/** Get the current number of failed task activations since start of the RTOS scheduler.
    Can be called from OS context (incl. interrupts). */
unsigned int rtos_OS_getNoActivationLoss(unsigned int idTask);

/** Compute how many bytes of the stack area are still unused. */
unsigned int rtos_OS_getStackReserve(unsigned int PID);

/** Get the current number of failed task activations since start of the RTOS scheduler. */
unsigned int rtos_getNoActivationLoss(unsigned int idTask);

/** Compute how many bytes of the stack area are still unused. */
unsigned int rtos_getStackReserve(unsigned int PID);


/*
 * Inline functions
 */

/**
 * Start a user task. A user task is a C function, which is executed in user mode and in a
 * given process context. The call is synchronous; the calling OS cpontext is immediately
 * preempted and superseded by the started task. The calling OS context is resumed when the
 * task function ends - be it gracefully or by exception/abortion.\n
 *   The started task inherits the priority of the calling OS context. It can be preempted
 * only by contexts of higher priority.
 *   @return
 * The executed task function can return a value, which is propagated to the calling OS
 * context if it is positive. A returned negative task function result is interpreted as
 * failing task and rtos_OS_runTask() returns #IVR_CAUSE_TASK_ABBORTION_USER_ABORT instead.
 * Furthermore, this event is counted as process error in the target process.
 *   @param pTaskConfig
 * The read-only configuration data for the task. In particular the task function pointer
 * and the ID of the target process.
 *   @param pTaskParam
 * This argument is meaningless to the function. The value is just passed on to the started
 * task function. The size is large enough to convey a pointer.
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user
 * code will lead to a privileged exception.
 */
static inline int32_t rtos_OS_runTask( const prc_userTaskConfig_t *pUserTaskConfig
                                     , uintptr_t taskParam
                                     )
{
    /* The function is assembler implemented, the C function is just a wrapper for
       convenient calling within the expected namespace. */
    return ivr_runUserTask(pUserTaskConfig, taskParam);

} /* End of rtos_OS_runTask */


/**
 * Start a user task. A user task is a C function, which is executed in user mode and in a
 * given process context. The call is synchronous; the calling user context is immediately
 * preempted and superseded by the started task. The calling user context is resumed when the
 * task function ends - be it gracefully or by exception/abortion.\n
 *   The started task inherits the priority of the calling OS context. It can be preempted
 * only by contexts of higher priority.\n
 *   The function requires sufficient privileges. The invoking task needs to belong to a
 * process with an ID greater then the target process. The task cannot be started in the OS
 * context.\n
 *   The function cannot be used recursively. The created task cannot in turn make use of
 * rtos_OS_runTask().
 *   @return
 * The executed task function can return a value, which is propagated to the calling user
 * context if it is positive. A returned negative task function result is interpreted as
 * failing task and rtos_runTask() returns #IVR_CAUSE_TASK_ABBORTION_USER_ABORT instead.
 * Furthermore, this event is counted as process error in the target process.
 *   @param pTaskConfig
 * The read-only configuration data for the task. In particular the task function pointer
 * and the ID of the target process.
 *   @param pTaskParam
 * This argument is meaningless to the function. The value is just passed on to the started
 * task function. The size is large enough to convey a pointer.
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user
 * code will lead to a privileged exception.
 */
static inline int32_t rtos_runTask( const prc_userTaskConfig_t *pUserTaskConfig
                                  , uintptr_t taskParam
                                  )
{
    return (int32_t)rtos_systemCall(RTOS_SYSCALL_RUN_TASK, pUserTaskConfig, taskParam);

} /* End of rtos_runTask */


/**
 * Abort the calling task immediately. A user task may use this system call at any time and
 * from any nested sub-routine. The task execution is immediately aborted. The function
 * does not return.
 *   @param taskReturnValue
 * The task can return a value to its initiator, i.e. to the context who had applied
 * ivr_runUserTask() or ivr_runInitTask() to create the task. The value is signed and
 * (only) the sign is meanigful to the assembly code to create/abort a task:\n
 *   Requesting task abortion is considered an error and counted in the owning process if
 * the returned value is negative. In this case, the calling context won't receive the
 * value but the error code #IVR_CAUSE_TASK_ABBORTION_USER_ABORT.\n
 *   Requesting task abortion is not considered an error if \a taskReturnValue is greater
 * or equal to zero. The value is propagated to the task creating context.
 *   @remark
 * This function must be called from the user task context only. Any attempt to use it from
 * OS code will lead to a crash.
 */
static inline _Noreturn void rtos_terminateTask(int32_t taskReturnValue)
{
    ivr_terminateUserTask(taskReturnValue);

} /* End of rtos_terminateTask */



/**
 * Priority ceiling protocol, partial interrupt lock: All interrupts up to the specified
 * priority level won't be handled by the CPU. This function is intended for implementing
 * mutual exclusion of sub-sets of tasks; the use of
 *   - ihw_enterCriticalSection() and
 *   - ihw_leaveCriticalSection()
 * or
 *   - ihw_suspendAllInterrupts() and
 *   - ihw_resumeAllInterrupts()
 * locks all interrupt processing and no other task (or interrupt handler) can become
 * active while the task is inside the critical section code. Using this function is much
 * better: Call it with the highest priority of all tasks, which should be locked, i.e. which
 * compete for the critical section to protect. This may still lock other, not competing
 * tasks, but at least all non competing tasks of higher priority will still be served (and
 * this will likely include most interrupt handlers).\n
 *   To leave the critical section, call the counterpart function
 * rtos_resumeAllInterruptsByPriority(), which restores the original interrupt/task
 * priority level.
 *   @return
 * The priority level at entry into this function (and into the critical section) is
 * returned. This level needs to be restored on exit from the critical section using
 * rtos_resumeAllInterruptsByPriority().
 *   @param suspendUpToThisPriority
 * All tasks/interrupts up to and including this priority will be locked. The CPU will
 * not handle them until the priority level is lowered again.
 *   @remark
 * To support the use case of nested calls of OSEK/VDX like GetResource/ReleaseResource
 * functions, this function compares the stated value to the current priority level. If \a
 * suspendUpToThisPriority is less than the current value then the current value is not
 * altered. The function still returns the current value and the calling code doesn't need
 * to take care: It can unconditionally end a critical section with
 * rtos_resumeAllInterruptsByPriority() stating the returned priority level value. (The
 * resume function will have no effect in this case.) This makes the OSEK like functions
 * usable without deep inside or full transparency of the priority levels behind the scene;
 * just use the pairs of Get-/ResumeResource, be they nested or not.
 *   @remark
 * The expense in terms of CPU consumption of using this function instead of the others
 * mentioned above is negligible for all critical section code, which consists of more than a
 * few machine instructions.
 *   @remark
 * The use of this function to implement critical sections is usually quite static. For any
 * protected entity (usually a data object or I/O device) the set of competing tasks
 * normally is a compile time known. The priority level to set for entry into the critical
 * section is the maximum of the priorities of all tasks in the set. The priority level to
 * restore on exit from the critical section is the priority of the calling task. All of
 * this static knowledge would typically be put into encapsulating macros that actually
 * invoke this function. (OSEK/VDX like environments would use this function pair to
 * implement the GetResource/ReleaseResource concept.)
 *   @remark
 * The ceiled priority is implicitly restored at the end of the interrupt, which triggered
 * the task and before it ends. It is not possible to consider this function a mutex, which
 * can be acquired in one task activation and which can be releases in an arbitrary later
 * task activation or from another task.
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user
 * code will lead to a privileged exception.
 *   @remark
 * This function requires that msr[EE]=1 on entry.
 */
static inline uint32_t rtos_OS_suspendAllInterruptsByPriority(uint32_t suspendUpToThisPriority)
{
    /* All priorities are in the range 0..15. Everything else points to an application
       error even if the hardware wouldn't mind. */
    assert((suspendUpToThisPriority & ~0xfu) == 0);

    /* MCU reference manual, section 28.6.6.2, p. 932: The change of the current priority
       in the INTC should be done under global interrupt lock. */
    ihw_suspendAllInterrupts();
    uint32_t priorityLevelSoFar = INTC.CPR_PRC0.R;

    /* It is useless and a waste of CPU but not a severe error to let this function set the
       same priority level we already have.
         It leads to immediate failure of the RTOS if we lower the level; however, from the
       perspective of the application code it is not necessarily an error to do so: If an
       application is organized in OSEK/VDX like resources it may be stringent (not
       optimal) to acquire different resources before an operation on them is started. These
       resources may be mapped onto different priority ceilings and the application may use
       nested calls of GetResource to acquire all of them. We must not force the
       application code to order the calls in order of increasing priority ceiling -- this
       will be untransparent to the application.
         These considerations leads to different strategies, and all of them are justified:
         To force the application be optimal in terms of CPU consumption, we would place an
       assertion: assert(suspendUpToThisPriority > priorityLevelSoFar);
         To be relaxed about setting the same priority ceiling, we would place another
       assertion: assert(suspendUpToThisPriority >= priorityLevelSoFar);
         If we want to build an OSEK/VDX like GetResource on top of this function, we
       should place a run-time condition: if(suspendUpToThisPriority > priorityLevelSoFar) */
    //assert(suspendUpToThisPriority > priorityLevelSoFar);
    //assert(suspendUpToThisPriority >= priorityLevelSoFar);
    if(suspendUpToThisPriority > priorityLevelSoFar)
        INTC.CPR_PRC0.R = suspendUpToThisPriority;

    /* Leave the critical section. */
    ihw_resumeAllInterrupts();

    /* Note, the next interrupt can still be a last one of priority less than or equal to
       suspendUpToThisPriority. This happens occasionally when the interrupt has asserted,
       while we were inside the critical section. Incrementing CPR does not un-assert an
       already asserted interrupt. The isync instruction ensures that this last interrupt
       has completed prior to the execution of the first code inside the critical section.
       See https://community.nxp.com/message/993795 for more. */
    asm volatile (
#ifdef __VLE__
                   "se_isync\n"
#else
                   "isync\n"
#endif
                   ::: "memory"
                 );

    return priorityLevelSoFar;

} /* End of rtos_OS_suspendAllInterruptsByPriority */



/**
 * See function rtos_OS_suspendAllInterruptsByPriority(), which has the same functionality
 * but offered to the OS context only. Differences to the OS function are:\n
 *   The priority can be raised only up to #RTOS_KERNEL_PRIORITY-2. An attempt to raise it
 * beyond this limit will lead to an #IVR_CAUSE_TASK_ABBORTION_SYS_CALL_BAD_ARG exception.
 * Safety rationale: A critical section cannot be shaped with the RTOS scheduler and nor
 * with the user task of highest priority. The intention is to inhibit a task from blocking
 * a safety task, which is assumed to be the only task running at highest priority.
 *   @return
 * Get the priority level at entry into this function (and into the critical section). This
 * priority level needs to be restored on exit from the critical section using system call
 * rtos_resumeAllInterruptsByPriority().
 *   @param suspendUpToThisPriority
 * \a suspendUpToThisPriority is the aimed priority level. All tasks/interrupts up to and
 * including this priority will be locked. The CPU will not handle them until the priority
 * level is lowered again.
 *   @remark
 * This function must be called from the user task context only. Any attempt to use it from
 * OS code will lead to a crash.
 */
static inline uint32_t rtos_suspendAllInterruptsByPriority(uint32_t suspendUpToThisPriority)
{
    return rtos_systemCall( PCP_SYSCALL_SUSPEND_ALL_INTERRUPTS_BY_PRIORITY
                          , suspendUpToThisPriority
                          );
} /* End of rtos_suspendAllInterruptsByPriority */




/**
 * This function is called to end a critical section of code, which requires mutual
 * exclusion of two or more tasks/ISRs. It is the counterpart of function
 * rtos_suspendAllInterruptsByPriority(), refer to this function for more details.\n
 *   Note, this function simply and unconditionally sets the current task/interrupt
 * priority level to the stated value. It can therefore be used to build further optimized
 * mutual exclusion code if it is applied to begin \a and to end a critical section. This
 * requires however much more control of the specified priority levels as when using the
 * counterpart function rtos_suspendAllInterruptsByPriority() on entry. Accidental
 * temporary lowering of the level will make the RTOS immediately fail.
 *   @param resumeDownToThisPriority
 * All tasks/interrupts above this priority level are resumed again. All tasks/interrupts
 * up to and including this priority remain locked.
 *   @remark
 * An application can (temporarily) rise the current priority level of handled tasks/ISRs
 * but must never lower them, the RTOS would fail: The hardware bit, that notified the
 * currently executing task/interrupt will be reset only at the end of the service routine,
 * so it is still pending. At the instance of lowering the priority level, the currently
 * executed task/ISR would be recursively called again. Furthermore, the INTC-internal
 * priority stack could easily overflow.
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user
 * code will lead to a privileged exception.
 *   @remark
 * This function requires that msr[EE]=1 on entry.
 */
static inline void rtos_OS_resumeAllInterruptsByPriority(uint32_t resumeDownToThisPriority)
{
    /* MCU reference manual, section 28.6.6.2, p. 932: The change of the current priority
       in the INTC should be done under global interrupt lock. */
    ihw_suspendAllInterrupts();
    INTC.CPR_PRC0.R = resumeDownToThisPriority;
    ihw_resumeAllInterrupts();

} /* End of rtos_OS_resumeAllInterruptsByPriority */



/**
 * See function rtos_OS_resumeAllInterruptsByPriority(), too, which has the same
 * functionality but offered to the OS context only. Differences to the OS function are:\n
 *   The priority can be raised only down to the initial task priority. An attempt to lower
 * it belower than the initial task priority will lead to an
 * #IVR_CAUSE_TASK_ABBORTION_SYS_CALL_BAD_ARG exception.
 *   @param resumeDownToThisPriority
 * All tasks/interrupts above this priority level are resumed again. All tasks/interrupts
 * up to and including this priority remain locked.\n
 *   The intended use of this function is to restore the priority level of a task after use
 * of a resource, which had been protected by an earlier call of the counterpart function
 * rtos_suspendAllInterruptsByPriority(). Normally, \a resumeDownToThisPriority will be set
 * to the value returned by the other function.
 *   @remark
 * It doesn't harm if a user tasks calls rtos_suspendAllInterruptsByPriority() to raise the
 * priority but doesn't lower it later using this function. The effect of
 * rtos_suspendAllInterruptsByPriority() will end with the termination of the task.
 *   @remark
 * This function must be called from the user task context only. Any attempt to use it from
 * OS code will lead to a crash.
 */
static inline void rtos_resumeAllInterruptsByPriority(uint32_t resumeDownToThisPriority)
{
    rtos_systemCall(PCP_SYSCALL_SUSPEND_ALL_INTERRUPTS_BY_PRIORITY, resumeDownToThisPriority);

} /* End of rtos_resumeAllInterruptsByPriority */



/**
 * Trigger an event to activate all associated tasks. A event, which had been registered
 * with cycle time zero is normally not executed. It needs to be triggered with this
 * function in order to make its associated tasks run once, i.e. to make its task functions
 * exceuted once as result of this call.\n
 *   This function must only be called from user tasks, which belong to a process with
 * sufficient privileges. The operation is permitted only for tasks belonging to those
 * processes, which have an ID that is greater of equal to the minimum specified for the
 * event in question. Otherwise an exception is raised, which aborts the calling task.\n
 *   If the calling task belongs to the set of tasks associated with \a idEvent, then it'll
 * have no effect but an accounted activation loss; an event can re-triggered only after
 * all associated activations have been completed. There is no activation queuing. The
 * function returns \a false in this case.\n
 *   Note, the system respects the priorities of the activated tasks. If a task of priority
 * higher than the activating task is activated by the triggered event then the activating
 * task is immediately preempted to the advantage of the activated task. Otherwise the
 * activated task is chained and executed after the activating task.
 *   @return
 * There is no activation queuing. If the aimed event is not yet done with processing its
 * previous triggering then the attempt is rejected. The function returns \a false and the
 * activation loss counter of the event is incremented. (See rtos_getNoActivationLoss().)
 *   @param idEvent
 * The ID of the event to activate as it had been got by the creation call for that event.
 * (See rtos_createEvent().)
 *   @remark
 * The function is indented to start a non cyclic task by application software trigger but
 * can be applied to cyclic tasks, too. In which case the task function of the cyclic task
 * would be invoked once additionally. Note, that an event activation loss is not unlikely
 * in this case; the cyclic task may currently be busy.
 *   @remark
 * It is not forbidden but useless to let a task activate itself by triggering the event it
 * is associated with. This will have no effect besides incrementing the activation loss
 * counter for that event.
 *   @remark
 * This function must be called from the user task context only. Any attempt to use it from
 * OS code will lead to a crash.
 */
static inline bool rtos_triggerEvent(unsigned int idEvent)
{
    return (bool)rtos_systemCall(RTOS_SYSCALL_TRIGGER_EVENT, idEvent);

} /* End of rtos_triggerEvent */


/**
 * Get the number of task failures (and task abortions at the same time) counted for the
 * given process since start of the kernel.
 *   @return
 * Get total number of errors counted for process \a PID.
 *   @param PID
 * The ID of the queried process in the range 1 .. PRC_NO_PROCESSES. An out of range PID
 * will always yield UINT_MAX.
 *   @remark
 * This function can be called from both, a user task or the OS context.
 */
static inline unsigned int rtos_getNoTotalTaskFailure(unsigned int PID)
{
    if(--PID < PRC_NO_PROCESSES)
        return prc_processAry[PID].cntTotalTaskFailure;
    else
        return UINT_MAX;

} /* End of rtos_getNoTotalTaskFailure */



/**
 * Get the number of task failures of given category counted for the given process since
 * start of the kernel.
 *   @return
 * Get total number of errors of category \a kindOfErr counted for process \a PID.
 *   @param PID
 * The ID of the queried process in the range 1 .. PRC_NO_PROCESSES. An out of range PID
 * will always yield UINT_MAX.
 *   @param kindOfErr
 * The category of the error. See file ivr_ivorHandler.h,
 * #IVR_CAUSE_TASK_ABBORTION_MACHINE_CHECK and following, for the enumerated error causes.
 *   @remark
 * This function can be called from both, a user task or the OS context.
 */
static inline unsigned int rtos_getNoTaskFailure(unsigned int PID, unsigned int kindOfErr)
{
    if(--PID < PRC_NO_PROCESSES
       &&  kindOfErr < sizeOfAry(prc_processAry[PID].cntTaskFailureAry)
      )
    {
        return prc_processAry[PID].cntTaskFailureAry[kindOfErr];
    }
    else
        return UINT_MAX;

} /* End of rtos_getNoTaskFailure */



/**
 * System call to suspend a process. All currently running tasks belonging to the process
 * are aborted and the process is stopped forever (i.e. there won't be further task starts
 * or I/O driver callback invocations).\n
 *   Suspending a process of PID i is permitted only to processes of PID j>i.
 *   @param PID
 * The ID of the process to suspend. Needs to be not zero (OS process) and lower than the
 * ID of the calling process. Otherwise the calling task is aborted with exception
 * #IVR_CAUSE_TASK_ABBORTION_SYS_CALL_BAD_ARG.
 *   @remark
 * Tasks of the suspended process can continue running for a short while until their abort
 * conditions are checked the next time. The likelihood of such a continuation is little
 * and the duration is in the magnitude of a Millisecond.
 *   @remark
 * This function must be called from the user task context only. Any attempt to use it from
 * OS code will lead to a crash.
 */
static inline void rtos_suspendProcess(uint32_t PID)
{
    rtos_systemCall(PRC_SYSCALL_SUSPEND_PROCESS, PID);

} /* End of rtos_suspendProcess */

#endif  /* RTOS_INCLUDED */
