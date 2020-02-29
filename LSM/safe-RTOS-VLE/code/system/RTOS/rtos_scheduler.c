/**
 * @file rtos_scheduler.c
 * This file implements a simple yet "safe" Real Time Operating System (RTOS) for the
 * MPC5643L.\n
 *   The RTOS offers a strictly priority controlled scheduler. The user code is organized
 * in processes and tasks. Any task belongs to one of the processes. Different processes
 * have different privileges. The concept is to use the process with highest privileges for
 * the safety tasks.\n
 *   A task is activated by an event; an application will repeatedly use the API
 * rtos_osCreateEvent() to define the conditions or points in time, when the tasks have to
 * become due.\n
 *   Prior to the start of the scheduler (and thus prior to the beginning of the
 * pseudo-parallel, concurrent execution of the tasks) all later used task are registered
 * at the scheduler; an application will repeatedly use the APIs rtos_osRegisterUserTask()
 * and rtos_osRegisterOSTask().\n
 *   After all needed tasks are registered the application will start the RTOS' kernel
 * by calling the API rtos_osInitKernel() and task scheduling begins.\n
 *   A task is mainly characterized by the owning process, the task function and a
 * priority; the C code function is invoked in the context of the process and at given
 * priority level when the task is activated. The function is executed either until the
 * task function is left or the task function or one of its sub-routines requests task
 * termination by means of a system call or the task function is aborted by an exception.\n
 *   "Context of a process" mainly relates to the memory management concept. Any process
 * has its private memory. This memory is either write-accessible only for the owning
 * process or for the owning process and all other processes with higher privileges. The
 * "or" in this statement is a matter of project configuration. A few more elements are
 * process dependent; many system calls are restricted to processes of sufficient
 * privileges.\n
 *   "Activated" does still not necessarily mean executing for a task; the more precise
 * wording is that the activation makes a task immediately and unconditionally "ready"
 * (i.e. ready for execution). If more than a single task are ready at a time then the
 * function of the task with higher priority is executed first and the function of the
 * other task will be served only after completion of the first. Several tasks can be
 * simultaneously ready and one of them will be executed, this is the one and only
 * "running" task.\n
 *   "Are ready at a time" does not necessarily mean that both tasks have been activated at
 * the same point in time. If task A of priority Pa is activated first and as only task
 * then it'll be executed regardless of its priority. If task B of priority Pb is activated
 * later, but still before A has completed then we have two tasks which have been activated
 * "at a time". The priority relation decides what happens:\n
 *   If Pa >= Pb then A is completed and B will be started and executed only after A has
 * completed.\n
 *   If Pb > Pa then task A turns from state running back to state ready and B becomes the
 * running task until it completes. Now A as remaining ready, yet uncompleted task
 * becomes the running task again and it can complete.\n
 *   With other words, if a task is activated and it has a higher priority than the running
 * task then it'll preempt the running task and it'll become the running task itself.\n
 *   If no task is ready at all then the scheduler continues the original code thread,
 * which is the code thread starting in function main() and which first registers the tasks
 * and then starts the kernel. (Everything in this code thread, which is placed behind
 * the call of API rtos_osInitKernel() is called the "idle task".)\n
 *   The implemented scheduling scheme leads to a strictly hierarchical execution order of
 * tasks. This scheme is sometimes referred to as scheduling of tasks of Basic Conformance
 * Class (BCC). It's simple, less than what most RTOSs offer, but still powerful enough for
 * the majority of industrial use cases.\n
 * Basic conformance class means that a task cannot suspend intentionally and ahead of
 * its normal termination. Once started, it needs to be entirely executed. Due to the
 * strict priority scheme it'll temporarily suspend only for sake of tasks of higher
 * priority (but not voluntarily or on own desire). Another aspect of the same is that the
 * RTOS doesn't know task to task events -- such events are usually the way intentional
 * suspension and later resume of tasks is implemented.\n
 *   The activation of a task can be done by software, using API function
 * rtos_triggerEvent() or it can be done by the scheduler on a regular time base. In the
 * former case the task is called an event task, the latter is a cyclic task with fixed
 * period time.\n
 *   The RTOS implementation is tightly connected to the implementation of interrupt
 * services. Interrupt services, e.g. to implement I/O operations for the tasks, are
 * registered with rtos_osRegisterInterruptHandler().\n
 *   Any I/O interrupts can be combined with the tasks. Different to most RTOSs we don't
 * impose a priority ordering between tasks and interrupts. A conventional design would put
 * interrupt service routines (ISR) at higher priorities than the highest task priority but
 * this is not a must. However, certain constraints still result from safety considerations
 * - not from technical aspects.\n
 *   Effectively, there's no difference between tasks and ISRs. All what has been said for
 * tasks with respect to priority, states and preemption holds for ISRs and the combination
 * of tasks and ISRs, too.\n
 *
 * Safety:\n
 *   The RTOS is based on the "unsafe" counterpart published at
 * https://github.com/PeterVranken/TRK-USB-MPC5643L/tree/master/LSM/RTOS-VLE. All
 * explanations given there still hold. In this project, we add a safety concept. This
 * needs to start with a specification of what we expect from a "safe" RTOS:\n
 *   "If the implementation of a task, which is meant the supervisory or safety task, is
 * itself free of faults then the RTOS shall guarantee that this task is correctly and
 * timely executed regardless of whatever imaginable failures are made by any other
 * process."\n
 *   This requirement serves at the same time as the definition of the term "safe", when
 * used in the context of this RTOS. safe-RTOS promises no more than this requirement says.
 * As a consequence, a software made with this RTOS is not necessarily safe and even if it
 * is then the system using that software is still not necessarily safe.\n
 *   The implementation uses the CPU's "problem state" in conjunction with exception
 * handlers and memory protection to meet the requirement.
 *   More details can be found at
 * https://github.com/PeterVranken/TRK-USB-MPC5643L/tree/master/LSM/safe-RTOS-VLE#3-the-safety-concept.
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
 *   rtos_osCreateEvent
 *   rtos_osRegisterInitTask
 *   rtos_osRegisterUserTask
 *   rtos_osRegisterOSTask
 *   rtos_osGrantPermissionRunTask
 *   rtos_osInitKernel
 *   rtos_osTriggerEvent
 *   rtos_scFlHdlr_triggerEvent
 *   rtos_processTriggeredEvents
 *   rtos_scFlHdlr_runTask
 *   rtos_osSuspendAllTasksByPriority
 *   rtos_osResumeAllTasksByPriority
 *   rtos_getNoActivationLoss
 * Module inline interface
 * Local functions
 *   getEventByID
 *   getEventByIdx
 *   registerTask
 *   checkEventDue
 *   onOsTimerTick
 *   launchAllTasksOfEvent
 *   initRTOSClockTick
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "rtos_process.h"
#include "rtos_externalInterrupt.h"
#include "rtos_priorityCeilingProtocol.h"
#include "rtos_ivorHandler.h"
#include "rtos_systemMemoryProtectionUnit.h"
#include "rtos_scheduler_defSysCalls.h"
#include "rtos_scheduler.h"
#include "rtos.h"


/*
 * Defines
 */

/* The assembler code doesn't have access to all defines found in the sphere of C code.
   This makes it essential to have a cross check here, were we can access the definitions
   from both spheres. */
#if RTOS_NO_ERR_PRC != RTOS_NO_CAUSES_TASK_ABORTION                                 \
    ||  RTOS_ERR_PRC_PROCESS_ABORT != RTOS_CAUSE_TASK_ABBORTION_PROCESS_ABORT           \
    ||  RTOS_ERR_PRC_MACHINE_CHECK != RTOS_CAUSE_TASK_ABBORTION_MACHINE_CHECK           \
    ||  RTOS_ERR_PRC_DEADLINE != RTOS_CAUSE_TASK_ABBORTION_DEADLINE                     \
    ||  RTOS_ERR_PRC_DI_STORAGE != RTOS_CAUSE_TASK_ABBORTION_DI_STORAGE                 \
    ||  RTOS_ERR_PRC_SYS_CALL_BAD_ARG  != RTOS_CAUSE_TASK_ABBORTION_SYS_CALL_BAD_ARG    \
    ||  RTOS_ERR_PRC_ALIGNMENT != RTOS_CAUSE_TASK_ABBORTION_ALIGNMENT                   \
    ||  RTOS_ERR_PRC_PROGRAM_INTERRUPT  != RTOS_CAUSE_TASK_ABBORTION_PROGRAM_INTERRUPT  \
    ||  RTOS_ERR_PRC_FPU_UNAVAIL != RTOS_CAUSE_TASK_ABBORTION_FPU_UNAVAIL               \
    ||  RTOS_ERR_PRC_TBL_DATA != RTOS_CAUSE_TASK_ABBORTION_TBL_DATA                     \
    ||  RTOS_ERR_PRC_TBL_INSTRUCTION != RTOS_CAUSE_TASK_ABBORTION_TBL_INSTRUCTION       \
    ||  RTOS_ERR_PRC_TRAP != RTOS_CAUSE_TASK_ABBORTION_TRAP                             \
    ||  RTOS_ERR_PRC_SPE_INSTRUCTION != RTOS_CAUSE_TASK_ABBORTION_SPE_INSTRUCTION       \
    ||  RTOS_ERR_PRC_USER_ABORT != RTOS_CAUSE_TASK_ABBORTION_USER_ABORT
# error Inconsistencies found between definitions made in C and assembler code
#endif

/* The user API header file rtos.h doesn't recursively include all headers of all
   implementing files. Therefore it needs to make some assumptions about basically variable
   but normally never changed constants. These assumptions need of course to be double
   checked. We do this here at compile time of the RTOS. */
#if RTOS_IDX_SC_RUN_TASK != RTOS_SYSCALL_RUN_TASK                                       \
    ||  RTOS_IDX_SC_TRIGGER_EVENT != RTOS_SYSCALL_TRIGGER_EVENT                         \
    ||  RTOS_IDX_SC_SUSPEND_PROCESS != RTOS_SYSCALL_SUSPEND_PROCESS
# error Inconsistent definitions made in C modules and RTOS API header file rtos.h
#endif

/** A pseudo event ID. Used to register a process initialization task using registerTask(). */
#define EVENT_ID_INIT_TASK     (UINT_MAX)


/*
 * Local type definitions
 */

/** The runtime information for a task triggering event.
      Note, we use a statically allocated array of fixed size for all possible events. A
    resource optimized implementation could use an application defined macro to tailor the
    size of the array and it could put the event and task configuration data into ROM.
    (Instead of offering the run-time configuration by APIs.) */
typedef struct eventDesc_t
{
    /** The current state of the event. */
    enum eventState_t { evState_idle, evState_triggered, evState_inProgress } state;

    /** The next due time. At this time, the event will activate the associated task set.*/
    unsigned int tiDue;

    /** The period time of the (cyclic) event in ms. The permitted range is 0..2^30-1.\n
          0 means no regular, timer controlled activation. The event is only enabled for
        software trigger using rtos_triggerEvent() (by interrupts or other tasks). */
    unsigned int tiCycleInMs;

    /** The priority of the event (and thus of all associated user tasks, which inherit the
        prioritry) in the range 1..UINT_MAX. Whether different events can share the same
        priority or need to have all different priorities depends on configuration macro
        #RTOS_SUPPORT_EVENTS_OF_SAME_PRORITY. If they may, then the execution of their
        associated tasks will be sequenced when they become due at same time or with
        overlap.\n
          Note, if the event has a priority above #RTOS_MAX_LOCKABLE_TASK_PRIORITY then
        only those tasks can be associated, which belong to the process with highest PID in
        use. This is a safety constraint. */
    unsigned int priority;

    /** An event can be triggered by user code, using rtos_triggerEvent(). However, tasks
        belonging to less privileged processes must not generally granted permission to
        trigger events that may activate tasks of higher privileged processes. Since an
        event is not process related, we make the minimum process ID an explicitly configured,
        which is required to trigger this event.\n
          Only tasks belonging to a process with PID >= \a minPIDToTriggerThisEvent are
        permitted to trigger this event.\n
          The range of \a minPIDToTriggerThisEvent is 0 ... (#RTOS_NO_PROCESSES+1). 0 and 1
        both mean, all processes may trigger the event, #RTOS_NO_PROCESSES+1 means only OS
        code can trigger the event. */
    unsigned int minPIDForTrigger;

    /** We can't queue events. If at least one task is still busy, which had been activated by
        the event at its previous due time then an event (and the activation of the
        associated tasks) is lost. This situation is considered an overrun and it is counted
        for diagnostic purpose. The counter is saturated and will halt at the
        implementation maximum.
          @remark This field is shared with the external client code of the module. There,
        it is read only. Only the scheduler code must update the field. */
    unsigned int noActivationLoss;

    /** The set of associated tasks, which are activated by the event, is implemented by an
        array and the number of entries. Here we have the array. */
    const rtos_taskDesc_t * taskAry;

    /** The set of associated tasks, which are activated by the event, is implemented by an
        array and the number of entries. Here we have the number of entries. */
    unsigned int noTasks;

#if RTOS_SUPPORT_EVENTS_OF_SAME_PRORITY == 1
    /** Support the scheduler: If this event has been processed then check event * \a
        this->pNextScheduledEvent as next one. */
    struct eventDesc_t *pNextScheduledEvent;
#endif
} eventDesc_t;



/*
 * Local prototypes
 */

/** Hook function for ISRs: The tasks of triggered events are possibly executed (depending
    on priority rules).\n
      Note, this variable is not publically declared although it is global. It is called
    externally only from the assembly code, which can't read public declarations in header
    files. */
void rtos_processTriggeredEvents(void);


/*
 * Data definitions
 */

/* Note, in this module, the naming convention of not using a mnemonic of the unit name as
   common prefix for all static data objects has been replaced by the convention for global
   data objects for sake of easier source code debugging. The debugger lists all RTOS data
   objects, including the non-global, as rtos_*. */

/** The list of all tasks. */
static rtos_taskDesc_t DATA_OS(rtos_taskCfgAry)[RTOS_MAX_NO_TASKS] =
    { [0 ... (RTOS_MAX_NO_TASKS-1)] = { .addrTaskFct = 0
                                             , .PID = 0
                                             , .tiTaskMax = 0
                                           }
    };

/** The list of all process initialization tasks. */
static rtos_taskDesc_t DATA_OS(rtos_initTaskCfgAry)[1+RTOS_NO_PROCESSES] =
    { [0 ... RTOS_NO_PROCESSES] = { .addrTaskFct = 0
                                    , .PID = 0
                                    , .tiTaskMax = 0
                                  }
    };

/** The number of registered tasks. The range is 0..#RTOS_MAX_NO_TASKS. */
static unsigned int SDATA_OS(rtos_noTasks) = 0;

/** The list of task activating events. */
static eventDesc_t BSS_OS(rtos_eventAry)[RTOS_MAX_NO_EVENTS];

/** For performance reasons, all events are internally ordered by priority. At user API,
    they are identified by an ID, which can have an ordering. We need a mapping for the
    implementation of APIs that refer to an event. */
static eventDesc_t * BSS_OS(rtos_mapEventIDToPtr)[RTOS_MAX_NO_EVENTS];

/** The number of created events. The range is 0..#RTOS_MAX_NO_EVENTS. */
static unsigned int SDATA_OS(rtos_noEvents) = 0;

/** The number of events, which have been triggered using rtos_osTriggerEvent() or
    rtos_triggerEvent() and which have not yet been entirely processed (i.e. at least one
    of its associated tasks has not yet completed).\n
      Note, this variable is an interface with the assembly code. The variable is inspected
    after return from an ISR to see whether the ISR has triggered an event so that some
    tasks may need to be launched. */
uint32_t SDATA_OS(rtos_noEventsPending) = 0;

/** The priority of the currently executed task.\n
      This variable is an interface with the assembly code. The implementation of the PCP
    requires the value to terminate a critical section if a user task should end without
    doing so. */
uint32_t SDATA_OS(rtos_currentPrio) = 0;

/** Time increment of one tick of the RTOS system clock. It is set at kernel initialization
    time to the configured period time of the system clock in Milliseconds
    (#RTOS_CLOCK_TICK_IN_MS). This way the unit of all time designations in the RTOS API
    always stays Milliseconds despite of the actually chosen clock rate. (An application of
    the RTOS can reduce the clock rate to the lowest possible value in order to save
    overhead.) The normal settings are a clock rate of 1 kHz and
    #RTOS_CLOCK_TICK_IN_MS=1.\n
      The variable is initially set to zero to hold the scheduler during RTOS
    initialization. */
static unsigned long SDATA_OS(rtos_tiOsStep) = 0;

/** RTOS sytem time in Milliseconds since start of kernel. */
static unsigned long SDATA_OS(rtos_tiOs) = (unsigned long)-1;

/** The option for inter-process communication to let a task of process A run a task in
    process B (system call rtos_runTask()) is potentially harmful, as the started task can
    destroy on behalf of process A all data structures of the other process B. It's of
    course not generally permittable. An all-embracing privilege rule cannot be defined
    because of the different use cases of the mechanism. Therefore, we have an explicit
    table of granted permissions, which can be configured at startup time as an element of
    the operating system initialization code.\n
      The bits of the word correspond to the 16 possible combinations of four possible
    caller processes in four possible target processes.
      By default, no permission is granted. */
#if RTOS_NO_PROCESSES == 4
static uint16_t SDATA_OS(rtos_runTask_permissions) = 0;
#else
# error Implementation depends on four being the number of processes
#endif


/*
 * Function implementation
 */

/**
 * Helper: Resolve the linear event index used at the API into the actual object. The
 * mapping is not trivial since the events are internally ordered by priority.
 *   @return
 * Get the event object by reference.
 *   @param idEvent
 * The mapping is not essential for the kernel. It implies avoidable run-time effort. The
 * only reason for having the mapping is a user friendly configuration API. If we had a
 * configuration tool (similar to OSEK OIL tool) or if we would put some documented
 * restrictions on the configuration API then we could have a implicit one-by-one mapping
 * without any loss of functionality.
 */
static ALWAYS_INLINE eventDesc_t *getEventByID(unsigned int idEvent)
{
    assert(idEvent < rtos_noEvents);
    return rtos_mapEventIDToPtr[idEvent];

} /* End of getEventByID */



/**
 * Helper: Resolve the linear, zeror based, internally used array index into the actual
 * event object. This function is trivial and intended more for completeness: it
 * complements the other function getEventByID().
 *   @return
 * Get the event object by reference.
 *   @param idxEvent
 * Index of event object in the array rtos_eventAry.
 */
static ALWAYS_INLINE eventDesc_t *getEventByIdx(unsigned int idxEvent)
{
    return &rtos_eventAry[idxEvent];

} /* End of getEventByIdx */



/**
 * Registration of a task. Normal, event activated tasks and process initialization tasks
 * can be registered for later execution. This can be both, user mode tasks and operating
 * system tasks. This function is repeatedly called by the application code as often as
 * tasks are required.\n
 *   All calls of this function need to be done prior to the start of the kernel using
 * rtos_osInitKernel().
 *   @return
 * \a rtos_err_noError (zero) if the task could be registered. The maximum number of normal
 * tasks is limited to #RTOS_MAX_NO_TASKS (regardless, how they are distributed among
 * processes). The maximum number of initialization tasks is one per process or for the OS.
 * If the limit is exceeded or if the task specification is invalid then the function
 * returns a non zero value from enumeration \a rtos_errorCode_t.\n
 *   An assertion in the calling code is considered appropriate to handle the error because
 * it'll always be a static configuration error.
 *   @param idEvent
 * Any (normal) task is activated by an event and a task without related event is useless.
 * This call associates the registered task with an already created event. See
 * rtos_osCreateEvent().\n
 *   If a process or OS initialization task is registered, then \a idEvent is set to
 * #EVENT_ID_INIT_TASK. It is allowed not to register an init task for a process or
 * for the OS but it is not possible to register more than one (or to re-register an) init
 * task for a given process or the OS.\n
 *   The order of registration of several tasks with one and the same event matters. The
 * tasks will be acivated in order of registration whenever the event becomes due or is
 * triggered by software.\n
 *   The order of registration doesn't matter for initialization tasks! The OS
 * initialization task is served first. Processes are always initialized in order of rising
 * process ID. The most privileged process is served last and can thus override descisions
 * of its less privileged predecessors.
 *   @param addrTaskFct
 * The task function, which is run in process \a PID every time, the event \a idEvent
 * triggers.
 *   @param PID
 *   The process the task belongs to by identifier. We have a fixed, limited number of four
 * processes (#RTOS_NO_PROCESSES) plus the kernel process, which has ID 0. The range of
 * process IDs to be used here is 0 .. #RTOS_NO_PROCESSES.\n
 *   @param tiTaskMaxInUs
 * Time budget for the user task in Microseconds. This budget is granted for each
 * activation of the task, i.e. each run of \a addrTaskFct(). The budget relates to
 * deadline monitoring, i.e. it is a world time budget, not an execution time budget.\n
 *   Deadline monitoring is supported up to a maximum execution time of
 * #RTOS_TI_DEADLINE_MAX_IN_US Microseconds.\n
 *   A value of zero means that deadline monitoring is disabled for the task.\n
 *   There's no deadline monitoring for OS tasks. If \a PID is zero then \a
 * tiTaskMaxInUS meeds to be zero, too.
 *   @remark
 * Never call this function after the call of rtos_osInitKernel()!
 *   @remark
 * This function must be called by trusted code in supervisor mode only.
 */
static rtos_errorCode_t registerTask( unsigned int idEvent
                                    , uintptr_t addrTaskFct
                                    , unsigned int PID
                                    , unsigned int tiTaskMaxInUs
                                    )
{
    /* The scheduler should be in halted state. */
    if(rtos_tiOsStep != 0)
        return rtos_err_configurationOfRunningKernel;

    /* The event need to be created before the task can be registered. */
    if(idEvent >= rtos_noEvents  &&  idEvent != EVENT_ID_INIT_TASK)
        return rtos_err_badEventId;

    /* The process ID needs to be in the fixed and limited range. */
    if(PID > RTOS_NO_PROCESSES)
        return rtos_err_badProcessId;

    /* The number of runtime tasks is constraint by compile time configuration. */
    if(rtos_noTasks >= RTOS_MAX_NO_TASKS  &&  idEvent != EVENT_ID_INIT_TASK)
        return rtos_err_tooManyTasksRegistered;

    /* Task function not set. */
    if(addrTaskFct == 0)
        return rtos_err_badTaskFunction;

    /* Check execution budget: There's an upper boundary for user tasks and OS task cannot
       have deadline monitoring at all. */
    if(tiTaskMaxInUs > RTOS_TI_DEADLINE_MAX_IN_US  ||  PID == 0  &&  tiTaskMaxInUs > 0)
        return rtos_err_taskBudgetTooBig;

    /* We make the distinction between normal runtime tasks and initialization tasks. */
    if(idEvent == EVENT_ID_INIT_TASK)
    {
        /* An initialization task must not be configured repeatedly for one and the same
           process. */
        const unsigned int idxP = PID;
        assert(idxP < sizeOfAry(rtos_initTaskCfgAry));
        if(rtos_initTaskCfgAry[idxP].addrTaskFct != 0)
            return rtos_err_initTaskRedefined;

        rtos_initTaskCfgAry[idxP].addrTaskFct = addrTaskFct;
        rtos_initTaskCfgAry[idxP].tiTaskMax = tiTaskMaxInUs * 120;
        rtos_initTaskCfgAry[idxP].PID = PID;
    }
    else
    {
        /* Add the new runtime task to the array. All tasks associated with an event need
           to form a consecutive list. We need to find the right location to insert the
           task and we need to consider an update of all events with refer to tasks with
           higher index. */
        eventDesc_t * const pEvent = getEventByID(idEvent);
        rtos_taskDesc_t *pNewTaskDesc;
        if(pEvent->taskAry == NULL)
        {
            /* First task of given event, we append a new sequence of tasks to the end of
               the task list so far. Done. */
            pEvent->taskAry = pNewTaskDesc = &rtos_taskCfgAry[rtos_noTasks];

            /* Associate the task with the specified event. */
            pEvent->noTasks = 1;
        }
        else
        {
            /* This is a further task for the event. We will have to shift the tasks in the
               task list to still have a consecutive sequence of tasks for the event. */
            pNewTaskDesc = (rtos_taskDesc_t*)pEvent->taskAry + pEvent->noTasks;

            /* The event's task sequence can be in the middle of the task list. So need to
               check if we have to move some rightmost list entries. */
            rtos_taskDesc_t *pTaskCfg = &rtos_taskCfgAry[rtos_noTasks];
            assert(rtos_noTasks >= 2  ||  pTaskCfg <= pNewTaskDesc);
            for(; pTaskCfg>pNewTaskDesc; --pTaskCfg)
                *pTaskCfg = *(pTaskCfg-1);

            /* Update the reference to the task sequence for all events, which still point
               to the shifted area of the task list. Note, that the events don't have a
               particular order with respect to the user specified index. (Instead, they
               are sorted by priority.) */
            unsigned int idxEv;
            for(idxEv=0; idxEv<rtos_noEvents; ++idxEv)
            {
                eventDesc_t * const pCheckedEvent = getEventByIdx(idxEv);
                if(pCheckedEvent != pEvent  &&  pCheckedEvent->taskAry >= pNewTaskDesc)
                    ++ pCheckedEvent->taskAry;
            }

            /* Associate the task with the specified event. */
            ++ pEvent->noTasks;
        }

        /* Fill the new task descriptor. */
        pNewTaskDesc->addrTaskFct = addrTaskFct;
        pNewTaskDesc->tiTaskMax = tiTaskMaxInUs * 120;
        pNewTaskDesc->PID = PID;
        ++ rtos_noTasks;
    }

    return rtos_err_noError;

} /* End of registerTask */




/**
 * Process the conditions that trigger events. The events are checked for becoming
 * meanwhile due and the associated tasks are made ready in case by setting the according
 * state in the event object. However, no tasks are already started in this function.
 */
static inline void checkEventDue(void)
{
    eventDesc_t *pEvent = getEventByIdx(0);
    const eventDesc_t * const pEndEvent = getEventByIdx(rtos_noEvents);

    /* We iterate the events in order for decreasing priority. */
    while(pEvent < pEndEvent)
    {
        if(pEvent->tiCycleInMs > 0)
        {
            if((signed int)(pEvent->tiDue - rtos_tiOs) <= 0)
            {
/// @todo Code duplication with osTriggerEvent
                /* Task is due. Check event state. If it is not idle then we have a task
                   overrun otherwise we trigger it. The check requires a read-modify-write
                   and the event can be set coincidentally from an ISR of higher priority -
                   we need a critical section. */
                rtos_osSuspendAllInterrupts();
                if(pEvent->state == evState_idle)
                {
                    /* Operation successful. Event can be triggered. */
                    pEvent->state = evState_triggered;
                    ++ rtos_noEventsPending;
                    assert(rtos_noEventsPending <= rtos_noEvents);
                }
                else
                {
                    /* Processing of event has not completed yet, associated tasks have not
                       all terminated yet. */

                    /* Counting the loss events requires a critical section. The loss
                       counter can be written concurrently from a task invoking
                       rtos_triggerEvent(). */
                    const unsigned int noActivationLoss = pEvent->noActivationLoss + 1;
                    if(noActivationLoss > 0)
                        pEvent->noActivationLoss = noActivationLoss;
                }

                /* Adjust the due time.
                     Note, we could queue task activations for cyclic tasks by not adjusting
                   the due time. Some limitation code would be required to make this safe. */
/// @todo TBC: Can likely be done outside CS
                pEvent->tiDue += pEvent->tiCycleInMs;

                rtos_osResumeAllInterrupts();

            } /* End if(Event is due?) */
        }
        else
        {
            /* Non regular events: nothing to be done. These events are triggered only by
               explicit software call of rtos_triggerEvent(). */

        } /* End if(Timer or application software activated task?) */

        /* Proceed with next event. */
        ++ pEvent;

    } /* End While(All configured events) */

} /* End of checkEventDue */



/**
 * The OS default timer handler. In function rtos_osInitKernel(), it is associated with the
 * PIT0 interrupt. You must not call this function yourself. The routine is invoked once
 * every #RTOS_CLOCK_TICK_IN_MS Milliseconds and triggers most of the scheduler decisions.
 * The application code is expected to run mainly in regular tasks and these are activated
 * by this routine when they become due. All the rest is done by the interrupt controller
 * INTC.
 *   @remark
 * The INTC priority at which this function is executed is configured as
 * #RTOS_KERNEL_IRQ_PRIORITY.
 */
static void onOsTimerTick(void)
{
    /* Update the system time. */
    rtos_tiOs += rtos_tiOsStep;

    /* The scheduler is most simple; the only condition to make a task ready is the next
       periodic due time. The task activation is left to the pseudo-software-interrupt,
       which is raised either by true interrupts (if they use setEvent) or by system calls,
       which may cause a task switch. */
    checkEventDue();

    /* Acknowledge the timer interrupt in the causing HW device. */
    PIT.TFLG0.B.TIF = 0x1;

} /* End of onOsTimerTick */



/**
 * Trigger an event to activate all associated tasks.\n
 *   This function implements the operation. It is called from two API functions, one for
 * OS code and one for user code. See rtos_osTriggerEvent() and rtos_triggerEvent() for
 * more details.
 *   @return
 * Get \a true if event could be triggered, \a false otherwise.
 *   @param pEvent
 * The event to trigger by reference.
 */
static bool osTriggerEvent(eventDesc_t * const pEvent)
{
    bool success;

    rtos_osSuspendAllInterrupts();
    if(pEvent->state == evState_idle)
    {
        /* Operation successful. Event can be triggered. */
        pEvent->state = evState_triggered;
        ++ rtos_noEventsPending;
        assert(rtos_noEventsPending <= rtos_noEvents);
        success = true;

        /* Setting an event means a possible context switch to another task. We need to run
           the scheduler to double-check this.
             A small optimization is not calling the scheduler if the processed event has a
           priority equal to or lower then the priority of the currently processed event. In
           this case the scheduler would anyway not change the current task right now.
             A condition is needed to avoid calling the scheduler if this function is
           called from inside an ISR. ISRs will call the function a bit later, when the
           interrupt context is cleared and only if they serve the root level interrupt
           (i.e. not from nested interrupt). In this case calling
           rtos_processTriggeredEvents() is done from the assembly code (IVOR #4 handler)
           but not yet here.
             Note, the call of rtos_processTriggeredEvents() will mean a recursive call of
           the scheduler and we return here only after a couple of other task executions.
             Note, the critical section, we are currently in, will be left by the scheduler
           as soon as it finds a task to be launched. However, it'll return in a new
           critical section - which is the one we leave at the end of this function. */
        if(pEvent->priority > rtos_currentPrio)
        {
            const uint32_t interruptLevel = INTC.CPR_PRC0.R;
            if(interruptLevel == 0)
            {
                /* We will get here only if the function is called from a task (OS or user
                   through system call) */
                rtos_processTriggeredEvents();
            }
        }
    }
    else
    {
        /* Processing of event has not completed yet, associated tasks have not
           all terminated yet. */

        /* Counting the loss events requires a critical section. The loss counter can be
           written concurrently from another task invoking rtos_osTriggerEvent() or by the
           timer controlled scheduler. */
        const unsigned int noActivationLoss =
                                    pEvent->noActivationLoss + 1;
        if(noActivationLoss > 0)
            pEvent->noActivationLoss = noActivationLoss;

        success = false;
    }
    rtos_osResumeAllInterrupts();

    return success;

} /* End of osTriggerEvent */



/**
 * Processing a triggered event means to execute all associated tasks. If the scheduler
 * finds an event to be processed as next one, then it'll call this function to run the
 * tasks.
 *   @param pEvent
 * The event by reference, whose tasks are to be executed one after another.
 */
static inline void launchAllTasksOfEvent(const eventDesc_t * const pEvent)
{
    const rtos_taskDesc_t *pTaskConfig = &pEvent->taskAry[0];
    unsigned int u = pEvent->noTasks;
    while(u-- > 0)
    {
        if(pTaskConfig->PID > 0)
            rtos_osRunTask(pTaskConfig, /* taskParam */ pTaskConfig->PID);
        else
            ((void (*)(void))pTaskConfig->addrTaskFct)();

        ++ pTaskConfig;

    } /* End while(Run all tasks associated with the event) */

} /* End of launchAllTasksOfEvent */



/**
 * Initialize a timer and associate its wrap-around interrupt with the main clock tick
 * function of the RTOS, onOsTimerTick(). The wrap-around cycle frequency of the timer
 * determines the time resolution of the RTOS operations.\n
 *   The wrap-around cycle time is a compile-time configuration item, see
 * #RTOS_CLOCK_TICK_IN_MS for more details.
 */
static void initRTOSClockTick(void)
{
    _Static_assert( RTOS_CLOCK_TICK_IN_MS >= 1  &&  RTOS_CLOCK_TICK_IN_MS <= 35791
                  , "RTOS clock tick configuration is out of range"
                  );
                  
    /* Disable all PIT timers during configuration. */
    PIT.PITMCR.R = 0x2;

    /* Install the interrupt service routine for cyclic timer PIT 0. It drives the OS
       scheduler for cyclic task activation. */
    rtos_osRegisterInterruptHandler( &onOsTimerTick
                                   , /* vectorNum */ 59
                                   , RTOS_KERNEL_IRQ_PRIORITY
                                   , /* isPremptable */ true
                                   );

    /* Peripheral clock has been initialized to 120 MHz. To get a 1ms interrupt tick we
       need to count till 120000. We configure an interrupt rate of RTOS_CLOCK_TICK_IN_MS
       Milliseconds.
         -1: See MCU reference manual, 36.5.1, p. 1157. */
    PIT.LDVAL0.R = 120000u*RTOS_CLOCK_TICK_IN_MS -1;

    /* Enable interrupts by this timer and start it. */
    PIT.TCTRL0.R = 0x3;

    /* Enable timer operation and let them be stopped on debugger entry. Note, this is a
       global setting for all four timers, even if we use and reserve only one for the
       RTOS.
         Note, that this doesn't release the scheduler yet; the step size is still zero
       and the system time doesn't advance despite of the starting timer interrupt. */
    PIT.PITMCR.R = 0x1;
    
} /* End of initRTOSClockTick */




/**
 * Creation of an event. The event can be cyclically triggering or software triggered.
 * An event is needed to activate a user task. Therefore, any reasonable application will
 * create at least one event.\n
 *   This function is repeatedly called by the application code for each required event.
 * All calls of this function need to be done prior to the start of the kernel using
 * rtos_osInitKernel().\n
 *   @return
 * \a rtos_err_noError (zero) if the event could be created. The maximum number of events
 * is limited to #RTOS_MAX_NO_EVENTS by hardware constraints. If the event cannot be
 * created due to this constraint or if the event descriptor contains invalid data then
 * then the function returns a non zero value from enumeration \a rtos_errorCode_t.\n
 *   An assertion in the calling code is considered appropriate to handle the error because
 * it'll always be a static configuration error.
 *   @param pEventId
 * All events are identified by a positive integer. Normally this ID is returned by
 * reference in * \a pEventId. If the event cannot be created then #RTOS_INVALID_EVENT_ID
 * is returned in * \a pEventId.\n
 *   Note, it is guaranteed to the caller that the returned ID is not an arbitrary,
 * meaningless number. Instead, the ID is counted from zero in order of creating events.
 * The first call of this function will return 0, the second 1, and so on. This simplifies
 * ID handling in the application code, constants can mostly be applied as the IDs are
 * effectively known at compile time.
 *   @param tiCycleInMs
 * The period time for regularly triggering events in ms.\n
 *   The permitted range is 0..2^30-1. 0 means no regular, timer controlled trigger and the
 * event is only enabled for software trigger using rtos_triggerEvent() (permitted for
 * interrupts or other tasks).
 *   @param tiFirstActivationInMs
 * The first trigger of the event in ms after start of kernel. The permitted range is
 * 0..2^30-1.\n
 *   Note, this setting is useless if a cycle time zero in \a tiCycleInMs specifies a non
 * regular event. \a tiFirstActivationInMs needs to be zero in this case, too.
 *   @param priority
 * The priority of the event in the range 1..UINT_MAX. Whether different events can share
 * the same priority or need to have all different priorities depends on configuration
 * macro #RTOS_SUPPORT_EVENTS_OF_SAME_PRORITY. The priority of an event is the priority of
 * all associated tasks at the same time. The execution of tasks, which share the priority
 * will be serialized when they are activated at same time or with overlap.\n
 *   Note the safety constraint that task priorities above #RTOS_MAX_LOCKABLE_TASK_PRIORITY
 * are available only to events, which solely have tasks associated that belong to the
 * process with highest process ID in use.\n
 *   Note, the order in which events are created can affect the priority in a certain
 * sense. If two events are created with same priority and when they at runtime become due
 * at the same OS time tick then the earlier created event will trigger its associated user
 * tasks before the later ceated event.
 *   @param minPIDToTriggerThisEvent
 * An event can be triggered by user code, using rtos_triggerEvent(). However, tasks
 * belonging to less privileged processes must not generally have permission to trigger
 * events that may activate tasks of higher privileged processes. Since an event is not
 * process related, we make the minimum process ID, which is required to trigger this
 * event, an explicitly configured property of the event.\n
 *   Only tasks belonging to a process with PID >= \a minPIDToTriggerThisEvent are
 * permitted to trigger this event.\n
 *   The range of \a minPIDToTriggerThisEvent is 0 ... (#RTOS_NO_PROCESSES+1). 0 and 1 both
 * mean, all processes may trigger the event, (#RTOS_NO_PROCESSES+1) means only OS code can
 * trigger the event. (#RTOS_NO_PROCESSES+1) is available as
 * #RTOS_EVENT_NOT_USER_TRIGGERABLE, too.
 *   @remark
 * Never call this function after the call of rtos_osInitKernel()!
 *   @remark
 * This function must be called by trusted code in supervisor mode only.
 */
rtos_errorCode_t rtos_osCreateEvent( unsigned int *pEventId
                                   , unsigned int tiCycleInMs
                                   , unsigned int tiFirstActivationInMs
                                   , unsigned int priority
                                   , unsigned int minPIDToTriggerThisEvent
                                   )
{
    *pEventId = RTOS_INVALID_EVENT_ID;

    /* The number of events is constraint by hardware (INTC) */
    if(rtos_noEvents >= RTOS_MAX_NO_EVENTS)
        return rtos_err_tooManyEventsCreated;

    if(priority == 0)
        return rtos_err_invalidEventPrio;

    /* Check settings for non regularly activated tasks. */
    if(tiCycleInMs == 0)
    {
        /* Avoid a useless and misleading setting. */
        if(tiFirstActivationInMs != 0)
            return rtos_err_badEventTiming;
    }

    /* The full 32 Bit range is avoided for time designations in order to have safe and
       unambiguous before and after decisions in a cyclic time model.
         Furthermore, no task must have the initial due time of 0xffffffff in order to not
       invalidate the startup logic of the scheduler (see rtos_osInitKernel()). */
    else if(((tiCycleInMs | tiFirstActivationInMs) & 0xc0000000) != 0)
        return rtos_err_badEventTiming;

    /* Is the PID constraint plausible? */
    if(minPIDToTriggerThisEvent > RTOS_NO_PROCESSES+1)
        return rtos_err_eventNotTriggerable;

    /* Insert the new event into the array and initialize the data structure. The position
       to insert is such that the events appear in order of decreasing priority. */
    unsigned int idxNewEv, v;
    for(idxNewEv=0; idxNewEv<rtos_noEvents; ++idxNewEv)
    {
        if(getEventByIdx(idxNewEv)->priority < priority)
            break;
    }
#if RTOS_SUPPORT_EVENTS_OF_SAME_PRORITY != 1
    if(idxNewEv > 0  &&  getEventByIdx(idxNewEv-1) == priority)
        return rtos_err_eventsShareSamePriority;
#endif

    for(v=rtos_noEvents; v>idxNewEv; --v)
        *getEventByIdx(v) = *getEventByIdx(v-1);

    eventDesc_t * const pNewEvent = getEventByIdx(idxNewEv);
    pNewEvent->tiCycleInMs = tiCycleInMs;
    pNewEvent->tiDue = tiFirstActivationInMs;
    pNewEvent->priority = priority;
    pNewEvent->minPIDForTrigger = minPIDToTriggerThisEvent;
    pNewEvent->noActivationLoss = 0;
    pNewEvent->taskAry = NULL;
    pNewEvent->noTasks = 0;

    const unsigned int idNewEv = rtos_noEvents++;

    /* Update the mapping of (already issued, publically known) event IDs onto the (now
       modified) internal arry index. */
    rtos_mapEventIDToPtr[idNewEv] = getEventByIdx(idxNewEv);
    for(v=0; v<idNewEv; ++v)
        if(rtos_mapEventIDToPtr[v] >= getEventByIdx(idxNewEv))
            ++ rtos_mapEventIDToPtr[v];

    #define WARNING
    #define WARNING Temporary test code, to be removed after use
    for(v=1; v<rtos_noEvents; ++v)
    {
#if RTOS_SUPPORT_EVENTS_OF_SAME_PRORITY == 1
        if(getEventByIdx(v)->priority > getEventByIdx(v-1)->priority)
#else
        if(getEventByIdx(v)->priority >= getEventByIdx(v-1)->priority)
#endif
            return rtos_err_invalidEventPrio; /* Actually an internal implementation error */
    }

    /* Assign the next available array index as publically known event ID. */
    *pEventId = idNewEv;

    return rtos_err_noError;

} /* End of rtos_osCreateEvent */



/**
 * Registration of a process initialization task. This function is typically repeatedly
 * called by the operating system initialization code as often as there are processes,
 * which need initialization.\n
 *   Initialization functions are particularly useful for the user processes. They allow
 * having user provided code, that is run prior to the start of the scheduler, in a still
 * race condition free environment but already with full protection against runtime
 * failures.\n
 *   All calls of this function need to be done prior to the start of the kernel using
 * rtos_osInitKernel().
 *   @return
 * \a rtos_err_noError (zero) if the task could be registered. The maximum number of
 * initialization tasks is one per process and one for the OS. If the limit is exceeded or
 * if the task specification is invalid then the function returns a non zero value from
 * enumeration \a rtos_errorCode_t.\n
 *   An assertion in the calling code is considered appropriate to handle the error because
 * it'll always be a static configuration error.
 *   @param initTaskFct
 * The initialization function, which is run once in process \a PID.\n
 *   The function gets the ID of the process it belongs to as argument.\n
 *   The function returns a signed value. If the value is negative then it is considered an
 * error, which is counted as error #RTOS_ERR_PRC_USER_ABORT in the owning
 * process and the scheduler will not start up.
 *   @param PID
 *   The process the task belongs to by identifier. We have a fixed, limited number of four
 * processes (#RTOS_NO_PROCESSES) plus the kernel process, which has ID 0. The range of
 * process IDs to be used here is 0 .. #RTOS_NO_PROCESSES.\n
 *   At kernel initialization time, the registered user process initialization functions
 * will be called in the order of rising PID, followed by the registered kernel process
 * initialization function.
 *   @param tiTaskMaxInUs
 * Time budget for the function execution in Microseconds. The budget relates to
 * deadline monitoring, i.e. it is a world time budget, not an execution time budget.\n
 *   Deadline monitoring is supported up to a maximum execution time of
 * #RTOS_TI_DEADLINE_MAX_IN_US Microseconds.\n
 *   A value of zero means that deadline monitoring is disabled for the run of the
 * initialization function.\n
 *   There's no deadline monitoring for OS tasks. If \a PID is zero then \a
 * tiTaskMaxInUS meeds to be zero, too.
 *   @remark
 * Never call this function after the call of rtos_osInitKernel()!
 *   @remark
 * This function must be called by trusted code in supervisor mode only.
 */
rtos_errorCode_t rtos_osRegisterInitTask( int32_t (*initTaskFct)(uint32_t PID)
                                        , unsigned int PID
                                        , unsigned int tiTaskMaxInUs
                                        )
{
    return registerTask(EVENT_ID_INIT_TASK, (uintptr_t)initTaskFct, PID, tiTaskMaxInUs);

} /* End of rtos_osRegisterInitTask */




/**
 * Registration of an event triggered user mode task. Normal, event activated tasks can be
 * registered for later execution. This function is repeatedly called by the application
 * code as often as user mode tasks are required.\n
 *   All calls of this function need to be done prior to the start of the kernel using
 * rtos_osInitKernel().
 *   @return
 * \a rtos_err_noError (zero) if the task could be registered. The maximum number of
 * tasks is limited to #RTOS_MAX_NO_TASKS (regardless, how they are distributed among
 * processes). If the limit is exceeded or if the task specification is invalid then the
 * function returns a non zero value from enumeration \a rtos_errorCode_t.\n
 *   An assertion in the calling code is considered appropriate to handle the error because
 * it'll always be a static configuration error.
 *   @param idEvent
 * The task is activated by an event. This call associates the registered task with an
 * already created event. See rtos_osCreateEvent().\n
 *   The order of registration of several tasks (both, OS and user mode) with one and the
 * same event matters. The tasks will be acivated in order of registration whenever the
 * event becomes due or is triggered by software.
 *   @param userModeTaskFct
 * The task function, which is run in process \a PID every time, the event \a idEvent
 * triggers.\n
 *   The function gets the ID of the process it belongs to as argument.\n
 *   The function returns a signed value. If the value is negative then it is considered an
 * error, which is counted as error #RTOS_ERR_PRC_USER_ABORT in the owning
 * process. (And after a number of errors a supervisory task could force a shutdown of the
 * process).
 *   @param PID
 *   The process the task belongs to by identifier. We have a fixed, limited number of four
 * processes (#RTOS_NO_PROCESSES). The range of process IDs to be used here is 1 ..
 * #RTOS_NO_PROCESSES.\n
 *   @param tiTaskMaxInUs
 * Time budget for the task in Microseconds. This budget is granted for each activation of
 * the task, i.e. each run of \a userModeTaskFct(). The budget relates to deadline
 * monitoring, i.e. it is a world time budget, not an execution time budget.\n
 *   Deadline monitoring is supported up to a maximum execution time of
 * #RTOS_TI_DEADLINE_MAX_IN_US Microseconds.\n
 *   A value of zero means that deadline monitoring is disabled for the task.\n
 *   @remark
 * Never call this function after the call of rtos_osInitKernel()!
 *   @remark
 * This function must be called by trusted code in supervisor mode only.
 */
rtos_errorCode_t rtos_osRegisterUserTask( unsigned int idEvent
                                        , int32_t (*userModeTaskFct)(uint32_t PID)
                                        , unsigned int PID
                                        , unsigned int tiTaskMaxInUs
                                        )
{
    /* OS task functions have another signature and should be registered with the other
       function rtos_osRegisterOSTask(). */
    if(PID == 0)
        return rtos_err_badProcessId;

    return registerTask(idEvent, (uintptr_t)userModeTaskFct, PID, tiTaskMaxInUs);

} /* End of rtos_osRegisterUserTask */



/**
 * Registration of an event triggered operating system task. Event activated tasks can be
 * registered for later execution. This function is repeatedly called by the application
 * code as often as operating system tasks are required.\n
 *   All calls of this function need to be done prior to the start of the kernel using
 * rtos_osInitKernel().
 *   @return
 * \a rtos_err_noError (zero) if the task could be registered. The maximum number of
 * tasks is limited to #RTOS_MAX_NO_TASKS (regardless, how they are distributed among
 * processes). If the limit is exceeded or if the task specification is invalid then the
 * function returns a non zero value from enumeration \a rtos_errorCode_t.\n
 *   An assertion in the calling code is considered appropriate to handle the error because
 * it'll always be a static configuration error.
 *   @param idEvent
 * The task is activated by an event. This call associates the registered task with an
 * already created event. See rtos_osCreateEvent().\n
 *   The order of registration of several tasks (both, OS and user mode) with one and the
 * same event matters. The tasks will be acivated in order of registration whenever the
 * event becomes due or is triggered by software.
 *   @param osTaskFct
 * The task function, which is run in the OS context every time, the event \a idEvent
 * triggers.
 *   @remark
 * Never call this function after the call of rtos_osInitKernel()!
 *   @remark
 * This function must be called by trusted code in supervisor mode only.
 */
rtos_errorCode_t rtos_osRegisterOSTask( unsigned int idEvent
                                      , void (*osTaskFct)(void)
                                      )
{
    return registerTask(idEvent, (uintptr_t)osTaskFct, /* PID */ 0, /* tiTaskMaxInUs */ 0);

} /* End of rtos_osRegisterOSTask */



/**
 * Operating system initialization function: Grant permissions for using the service
 * rtos_runTask to particular processes. By default, the use of that service is not
 * allowed.\n
 *   By principle, offering service rtos_runTask makes all processes vulnerable, which are
 * allowed as target for the service. A failing, straying process can always hit some ROM
 * code executing the system call with arbitrary register contents, which can then lead to
 * errors in an otherwise correct process.\n
 *   This does not generally break the safety concept, the potentially harmed process can
 * for example be anyway supervised by another, unaccessible supervisory process.
 * Consequently, we can offer the service at least on demand. A call of this function
 * enables the service for a particular pair of calling process and targeted process.
 *   @param pidOfCallingTask
 * The tasks belonging to process with PID \a pidOfCallingTask are granted permission to
 * run a task in another process. The range is 1 .. #RTOS_NO_PROCESSES, which is
 * double-checked by assertion.
 *   @param targetPID
 * The tasks started with service rtos_runTask() may be run in process with PID \a
 * targetPID. The range is 1 .. maxPIDInUse-1, which is double-checked later.\n
 *   \a pidOfCallingTask and \a targetPID must not be identical, which is double-checked by
 * assertion.
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
void rtos_osGrantPermissionRunTask(unsigned int pidOfCallingTask, unsigned int targetPID)
{
    /* targetPID <= 3: Necessary but not sufficient to double-check
       "targetPID <= maxPIDInUse-1". */
    assert(pidOfCallingTask >= 1  &&  pidOfCallingTask <= 4
           &&  targetPID >= 1  &&  targetPID <= 3
          );

    /* It may be useful to grant process A the right to run a task in process A. This
       effectively implements a try/catch mechanism. The run task function has the option
       to abort its action at however deeply nested function invocation and using
       rtos_terminateTask(). Control returns to the call of rtos_runTask and the caller
       gets a negative response code as indication (otherwise a positive value computed by
       the called function). The called function belongs to the same process and its
       potential failures can of course harm the calling task, too. This does not break
       our safety concept, but nonetheless, offering a kind of try/catch could easily be
       misunderstood as a kind of full-flavored run-time protection, similar to what we
       have between processes. This potential misunderstanding makes the use of such a
       try/catch untransparent and therefore unsafe. Hence, we do not allow it here. */
    assert(targetPID != pidOfCallingTask);

    /* Caution, the code here depends on macro RTOS_NO_PROCESSES being four and needs to be
       consistent with the implementation of rtos_scFlHdlr_runTask(). */
#if RTOS_NO_PROCESSES != 4
# error Implementation requires the number of processes to be four
#endif
    const unsigned int idxCalledPrc = targetPID - 1u;
    const uint16_t mask = 0x1 << (4u*(pidOfCallingTask-1u) + idxCalledPrc);
    rtos_runTask_permissions |= mask;

} /* End of rtos_osGrantPermissionRunTask */



/**
 * Initialization and start of the RTOS kernel.\n
 *   The function initializes a hardware device to produce a regular clock tick and
 * connects the OS schedule function onOsTimerTick() with the interrupt raised by this
 * timer device. After return, the RTOS is running with a regular clock tick for scheduling
 * the tasks. Period time is #RTOS_CLOCK_TICK_IN_MS Milliseconds.\n
 *   The function can be called before or after the External Interrupts are enabled at the
 * CPU (see rtos_osResumeAllInterrupts()). Normal behavior is however, no to resume the
 * interrupt processing before and let this be done by rtos_osInitKernel().
 *   @return
 * \a rtos_err_noError (zero) if the scheduler could be started. The function returns a non
 * zero value from enumeration \a rtos_errorCode_t if a configuration error is detected.
 * The software must not start up in this case. Since it is only about static
 * configuration, the returned error may be appropriately handled by an assertion.
 *   @remark
 * The RTOS kernel applies the Periodic Interrupt Timer 0 (PIT0) as clock source. This
 * timer is reserved to the RTOS and must not be used at all by some other code.
 *   @remark
 * All application tasks need to be registered before invoking this function, see
 * rtos_osRegisterInitTask(), rtos_osRegisterUserTask() and rtos_osRegisterOSTask().
 *   @remark
 * This function must be called from the OS context only. The call of this function will
 * end the operating system initialization phase.
 */
rtos_errorCode_t rtos_osInitKernel(void)
{
    rtos_errorCode_t errCode = rtos_err_noError;

    rtos_noEventsPending = 0;
    rtos_currentPrio = 0;

/// @todo Additional error condition: All associated tasks should be different. A task must
// not be associated with different events. TBC: It's not a technical issue but rather an
// indication of a likely configuration problem. However, are there use cases for generaic
// task body implementations, used in different contexts, prcesses?

    /* The user must have registered at minimum one task and must have associated it with a
       event. */
    if(rtos_tiOsStep != 0)
        errCode = rtos_err_configurationOfRunningKernel;
    else if(rtos_noEvents == 0  ||  rtos_noTasks == 0)
        errCode = rtos_err_noEvOrTaskRegistered;

    /* Fill all process stacks with the empty-pattern, which is applied for computing the
       stack usage. */
    bool isProcessConfiguredAry[1+RTOS_NO_PROCESSES] = {[0 ... RTOS_NO_PROCESSES] = false};
    if(errCode == rtos_err_noError)
        errCode = rtos_initProcesses(isProcessConfiguredAry);

    unsigned int idxTask
               , maxPIDInUse = 0;
    /* Find the highest PID in use. */
    for(idxTask=0; idxTask<rtos_noTasks; ++idxTask)
        if(rtos_taskCfgAry[idxTask].PID > maxPIDInUse)
            maxPIDInUse = rtos_taskCfgAry[idxTask].PID;

    /* A task must not belong to an invalid configured process. This holds for init and for
       run time tasks. */
    unsigned int idxP;
    if(errCode == rtos_err_noError)
    {
        for(idxTask=0; idxTask<rtos_noTasks; ++idxTask)
        {
            assert(rtos_taskCfgAry[idxTask].PID < sizeOfAry(isProcessConfiguredAry));
            if(!isProcessConfiguredAry[rtos_taskCfgAry[idxTask].PID])
                errCode = rtos_err_taskBelongsToInvalidPrc;

        } /* For(All registered runtime tasks) */

        for(idxP=0; idxP<1+RTOS_NO_PROCESSES; ++idxP)
        {
            /* Note, the init task array is - different to the runtime task array
               rtos_taskCfgAry - ordered by PID. The field PID in the array entries are
               redundant. A runtime check is not appropriate as this had happened at
               registration time. We can place a simple assertion here. */
            if(rtos_initTaskCfgAry[idxP].addrTaskFct != 0)
            {
                assert(rtos_initTaskCfgAry[idxP].PID == idxP);
                if(!isProcessConfiguredAry[idxP])
                    errCode = rtos_err_taskBelongsToInvalidPrc;
            }
        } /* for(All possibly used processes) */
    }

    /* Now knowing, which is the process with highest privileges we can double-check the
       permissions granted for using the service rtos_runTask(). It must not be possible to
       run a task in the process with highest privileges. */
    if(errCode == rtos_err_noError)
    {
        /* Caution: Maintenance of this code is required consistently with
           rtos_osGrantPermissionRunTask() and rtos_scFlHdlr_runTask(). */
        assert(maxPIDInUse <= 4);
        const uint16_t mask = maxPIDInUse >= 1
                              ? 0x1111 << (maxPIDInUse-1)   /* Normal situation */
                              : 0xffff;                     /* No process in use */
        if((rtos_runTask_permissions & mask) != 0)
            errCode = rtos_err_runTaskBadPermission;
    }

    /* We could check if a process, an init task is registered for, has a least one runtime
       task. However, it is not harmful if not and there might be pathologic applications,
       which solely consist of I/O driver callbacks. */

    if(errCode == rtos_err_noError)
    {
        unsigned int idxEv;
        for(idxEv=0; idxEv<rtos_noEvents; ++idxEv)
        {
            const eventDesc_t * const pEvent = getEventByIdx(idxEv);
            const unsigned int noAssociatedTasks = pEvent->noTasks;

            /* Check task configuration: Events without an associated task are useless and
               point to a configuration error. */
            if(noAssociatedTasks == 0)
                errCode = rtos_err_eventWithoutTask;

            /* If an event has a priority above #RTOS_MAX_LOCKABLE_TASK_PRIORITY then only
               those tasks can be associated, which belong to the process with highest PID
               in use or OS tasks. This is a safety constraint. */
            if(pEvent->priority > RTOS_MAX_LOCKABLE_TASK_PRIORITY)
            {
                for(idxTask=0; idxTask<noAssociatedTasks; ++idxTask)
                {
                    const unsigned int PID = pEvent->taskAry[idxTask].PID;
                    if(PID > 0  &&  PID != maxPIDInUse)
                        errCode = rtos_err_highPrioTaskInLowPrivPrc;
                }
            } /* End if(Unblockable priority is in use by event) */
        } /* for(All registered events) */
    }

#if RTOS_SUPPORT_EVENTS_OF_SAME_PRORITY == 1
    /* The scheduling of events of potentially same priority is supported by a link
       pointer, which points the scheduler to the next event to check after the event had
       been processed. This next event is either the first one in a group of events of same
       priority or the linear successor if this would be the event itself. */
    if(errCode == rtos_err_noError)
    {
        unsigned int idxEv, idxEvNextPrio;
        unsigned int lastPrio;
        for(idxEv=0; idxEv<rtos_noEvents; ++idxEv)
        {
            eventDesc_t * const pEvent = getEventByIdx(idxEv);

            /* Is this event the first one of the group of next lower priority? */
            if(idxEv == 0  ||  pEvent->priority < lastPrio)
            {
                /* The first event in such a group is linked to the next one in order of
                   the list. It doesn't matter if this object doesn't exist if the end of
                   the list is reached. This pointer is anyway used as guard object. */
                pEvent->pNextScheduledEvent = getEventByIdx(idxEv+1);

                lastPrio = pEvent->priority;
                idxEvNextPrio = idxEv;
            }
            else
            {
                /* All further events is such a group are linked to the first event of the
                   group. */
                pEvent->pNextScheduledEvent = getEventByIdx(idxEvNextPrio);
            }
        } /* End for(All events) */
    }
#endif

    /* After checking the static configuration, we can enable the dynamic processes.
       Outline:
       - Disable all processes (which is their initial state). Once we enable the
         interrupts, the I/O drivers start working and they may invoke callbacks into the
         processes. The execution of these callbacks will be inhibited
       - Initialize the memory protection. This needs to be done before the very first user
         mode task function has the chance to become started. (The very first user mode task
         functions need to be the process initialization tasks.)
       - Disable the scheduler to trigger any events (which is its initial state).
         Triggering events would not cause any user tasks to execute (processes still
         disabled) but their due counters would already run and the configured startup
         conditions would not be met later when enabling the processes
       - Globally enable interrupt processing. I/O drivers and OS clock tick are running.
         This is a prerequisite for the deadline monitoring, which we want to have in place
         already for the init tasks
       - Sequentially execute all configured process initialization tasks. There are no
         crosswise race condition and nor with user tasks or I/O driver callbacks. Note,
         interrupts are already running and cause race conditions. Moreover, they could
         make use of rtos_osTriggerEvent() and if an OS task is associated with the
         triggered event then there would be race conditions with this OS task, too
       - Enable the processes and release the scheduler; scheduler, user tasks and I/O
         driver's callbacks start running */

    /* Arm the memory protection unit. */
    if(errCode == rtos_err_noError)
        rtos_initMPU();

    /* Stop the scheduler. It won't run although the RTOS clock starts spinning. We don't
       want to see a running user task during execution of the init tasks. */
    rtos_tiOs = (unsigned long)-1;
    rtos_tiOsStep = 0;

    /* We can register the interrupt service routine for the scheduler timer tick. */
    if(errCode == rtos_err_noError)
        initRTOSClockTick();

    /* All processes are initialized by rtos_initProcesses() in stopped state: We don't want
       to see a callback from an I/O driver after resuming interrupt processing and while
       an init task is executed. Note, it doesn't really matter if interrupt processing was
       resumed already before. */
    if(errCode == rtos_err_noError)
        rtos_osResumeAllInterrupts();

    /* When we get here (and if we didn't see a configuration error) then all kernel
       interrupts are configured, interrupts occur and are processed, but no user tasks are
       activated nor will an I/O driver run a callback. We can safely start our process
       initialization tasks. */

    /* Run all process initialization in order of increasing PID. A process with higher
       privileges is initialized after another with less privileges. The higher privileged
       could override settings made by its predecessor.
         In this consideration and despite of its PID zero, the operating system process
       has the highest privileges. This requires a loop counter like 1, 2, ..., N, 0. */
    idxP = 0;
    do
    {
#if RTOS_NO_PROCESSES > 0
        if(idxP < RTOS_NO_PROCESSES)
            ++ idxP;
        else
            idxP = 0;
#else
        /* No processes, just initialize the OS. idxP=0 is only loop cycle. */
#endif

        /* The specification of an initialization task is an option only. Check for NULL
           pointer. */
        if(rtos_initTaskCfgAry[idxP].addrTaskFct != 0)
        {
            if(isProcessConfiguredAry[idxP])
            {
                /* Everything is alright. Run the initialization task. A negative return
                   value is defined to be an error. (This needs to be considered by the
                   implementation of the task.) */
                int32_t resultInit;
                if(rtos_initTaskCfgAry[idxP].PID == 0)
                {
                    /* OS initialization function: It is a normal sub-function call; we are
                       here in the OS context. */
                    resultInit = ((int32_t (*)(void))rtos_initTaskCfgAry[idxP].addrTaskFct)();
                }
                else
                {
                    /* The initialization function of a process is run as a task in that
                       process, which involves full exception handling and possible abort
                       causes. */
                    resultInit = rtos_osRunInitTask(&rtos_initTaskCfgAry[idxP]);
                }
                if(resultInit < 0)
                    errCode = rtos_err_initTaskFailed;
            }
            else
            {
                /* An initialization task must not be registered for a process, which is not
                   configured. This had been checked above and we can never get here. */
                assert(false);
            }
        } /* End if(Init task configured for process?) */
    }
    while(idxP != 0);  /* End for(All possible processes, OS as last one) */

    /* After successfully completing all the initialization tasks, we can release the
       scheduler and the processes. We do this in a critical section in order to not
       endanger the specified relationship of initial task activations (specified in terms
       of task priority, period time and initial due time). */
    if(errCode == rtos_err_noError)
    {
        rtos_osSuspendAllInterrupts();

        /* Process state: Set to running (not zero) only if configuration generally okay. */
        unsigned int idxP;
        for(idxP=1; idxP<1+RTOS_NO_PROCESSES; ++idxP)
            if(isProcessConfiguredAry[idxP])
                rtos_osReleaseProcess(/* PID */ idxP);

        /* Release scheduler. */
        rtos_tiOsStep = RTOS_CLOCK_TICK_IN_MS;

        rtos_osResumeAllInterrupts();
        
        /** @todo Minor problem: We leave tiOs at -1 to trigger all actions specified for
            t=0 in the very first clock tick. This means unfortunately that the idle tasks
            the time designation UINT_MAX for a short while. Actually a fault, even if
            likely harmless. A simple way out would be a busy wait here till we see
            tiOs >= 0. */
    }

    /* @todo Shall we offer idle tasks per process? If so, we cannot leave the routine but
       would need to enter an infinite loop - and had to offer such a function for OS, too.
       For consistency reasons this would require an init function for OS, too. */

    return errCode;

} /* End of rtos_osInitKernel */




/**
 * Trigger an event to activate all associated tasks. A event, which had been registered
 * with cycle time zero is normally not executed. It needs to be triggered with this
 * function in order to make its associated tasks run once, i.e. to make its task functions
 * executed once as result of this call.\n
 *   This function can be called from any OS task or ISR. However, if the calling task
 * belongs to the set of tasks associated with \a idEvent, then it'll have no effect but an
 * accounted activation loss; an event can be re-triggered only after all associated
 * activations have been completed. There is no activation queueing. The function returns
 * \a false in this case.\n
 *   Note, the system respects the priorities of the activated tasks. If a task of priority
 * higher than the activating task is activated by the triggered event then the activating
 * task is immediately preempted to the advantage of the activated task. Otherwise the
 * activated task is chained and executed after the activating task.
 *   @return
 * There is no activation queuing. Consequently, triggering the event can fail if at least
 * one of the associated tasks has not yet completed after the previous trigger of the
 * event. The function returns \a false and the activation loss counter of the event is
 * incremented. (See rtos_getNoActivationLoss().) In this situation, the new trigger is
 * entirely lost, i.e. none of the associated tasks will be activated by the new trigger.
 *   @param idEvent
 * The ID of the event to activate as it had been got by the creation call for that event.
 * (See rtos_osCreateEvent().)
 *   @remark
 * The function is indented to start a non cyclic task by application software trigger but
 * can be applied to cyclic tasks, too. In which case the task function of the cyclic task
 * would be invoked once additionally. Note, that an event activation loss is not unlikely in
 * this case; the cyclic task may currently be busy.
 *   @remark
 * It is not forbidden but useless to let a task activate itself by triggering the event it
 * is associated with. This will have no effect besides incrementing the activation loss
 * counter for that event.
 *   @remark
 * This function must be called from the OS context only. It may be called from an ISR to
 * implement delegation to a user task.
 */
bool rtos_osTriggerEvent(unsigned int idEvent)
{
    return osTriggerEvent(getEventByID(idEvent));

} /* End of rtos_osTriggerEvent */



/**
 * System call handler implementation to trigger an event (and to activate the associated
 * tasks). Find all details in rtos_osTriggerEvent().
 *   @return
 * \a true if activation was possible, \a false otherwise.
 *   @param pidOfCallingTask
 * Process ID of calling user task. The operation is permitted only for tasks belonging to
 * those processes, which
 * have an ID that is greater of equal to the minimum specified for the event in question.
 * Otherwise an exception is raised, which aborts the calling task.
 *   @param idEvent
 * The ID of the event to trigger. This is the ID got from rtos_osCreateEvent().
 *   @remark
 * This function must never be called directly. The function is only made for placing it in
 * the global system call table.
 */
uint32_t rtos_scFlHdlr_triggerEvent(unsigned int pidOfCallingTask, unsigned int idEvent)
{
    if(idEvent < rtos_noEvents)
    {
        eventDesc_t * const pEvent = getEventByID(idEvent);
        if(pidOfCallingTask >= pEvent->minPIDForTrigger)
            return (uint32_t)osTriggerEvent(pEvent);
    }

    /* The user specified event ID is not in range or the calling process doesn't have the
       required privileges. Either is a severe user code error, which is handled with an
       exception, task abort and counted error.
         Note, this function does not return. */
    rtos_osSystemCallBadArgument();

} /* End of rtos_scFlHdlr_triggerEvent */



/**
 * This function implements the main part of the scheduler, which actually runs tasks. It
 * inspects all events, whether they have been triggered in an ISR or system call handler
 * and execute the associated tasks if furthermore the priority conditions are fullfilled.
 *   The function is called from the common part of the assembly implementation of the ISRs
 * and from all system call handlers, which could potentially lead to the start of tasks.\n
 *   This function is entered with all interrupt processing disabled (MSR[ee]=0).
 */
SECTION(.text.ivor.rtos_processTriggeredEvents) void rtos_processTriggeredEvents(void)
{
    /* This function and particularly this loop is the essence of the task scheduler. There
       are some tricky details to be understood.
         This function is called as a kind of "on-exit-hook" of any interrupt, which makes
       use of the rtos_osTriggerEvent() service. (Actually, this includes user tasks, which
       run the software interrupt rtos_triggerEvent().) It looks for the triggered event
       and runs the associated tasks if it has a priority higher then the priority of the
       currently running task.
         The operation looks uncomplicated for an event of higher priority. We acknowledge
       the event and run the tasks. Looking for and acknowleding means a read-modify-write
       operation and since the events can at any time be accessed by interrupts of higher
       priority we need a critical section for this.
         First complexity is an interrupt, which sets another event while we are processing
       the tasks of the first one. If this has in turn a higher priority then it's no new
       consideration but just a matter of recursive invocation of this same function.
       However, if it has a priority lower then that of the currently processed event and
       tasks, then we (and not the preempting context, which triggers the event) are
       obliged to run the tasks of this event, too, but - because of the lower priority -
       only later, after the current set of tasks. (Note, "later", there is no HW interrupt
       any more to get the new event be processed - so we need a loop here to not forget
       that event.) The tricky thing is how to span the critical sections:
         If we find the first event (searching from highest towards lower priorities) then
       we apply the CS just to acknowledge the event. (I.e. no potential later, recursive
       invocation of this function will compete in handling it.) When done with all the
       tasks of the event we release the event. We change the status from "in progress" to
       "idle" - of course again in a CS. However, this CS must now be merged with the CS at
       the beginning of the next cycle, the CS to acknowledge the next found event. The
       reason why:
         As soon as we release an event, it can be set again and in particular before we
       have left this function and killed its stack frame (a stack frame of significant
       size as this function still is element of an ISR). The newly set event would mean a
       recursive call of this function, so another stack frame for the same event. The same
       could then happen to the recursice function invocation and so forth - effectively
       there were no bounds anymore for the stack consumption, which is a fatal risk.
       Merging the CSs for releasing event A and acknowledging event B (of lower priority)
       means that the stack frame of this invokation is inherited by the next processed
       event B before a recursive call can process the next occurance of A. Which is
       alright; this leads to the pattern that there can be outermost one stack frame per
       event priority and this is the possible minimum.
         The same consideration requires that the CS for the final event release must not
       be left before return from the function. Return from the function still means
       several instructions until the stack frame is killed and event setting in this phase
       is just the same as outlined for the loop cycle-to-cycle situation. Actually, the
       final CS must not be ended before the stack frame has been killed, effectively at
       the very end of the ISR, with the rfi instruction.
         The same consideration requires that we are already inside the first
       acknowledge-CS when entering the function: Otherwise we could see a recursive
       function call before the loop. Which would be possible only once per defined event,
       thus not meaning unbounded stack usage, but still an avoidable and highly undesired,
       significant potential waste of stack. (All of this potential waste would need to go
       into the worst case stack usage estimation.) */

    /* Here, we are inside a critical section. External Interrupt handling is disabled. */
    assert(rtos_osGetAllInterruptsSuspended());

    /* Safe the current priority: It'll be replaced by that of the events, we find to be
       served, but finally we will have to restore the value here. */
    const unsigned int prioAtEntry = rtos_currentPrio;

    eventDesc_t *pEvent = getEventByIdx(0);
    const eventDesc_t * const pEndEvent = getEventByIdx(rtos_noEvents);

    /* We iterate the events in order of decreasing priority. */
    while(pEvent < pEndEvent)
    {
        if(pEvent->priority <= prioAtEntry)
        {
            /* Launching tasks must not be considered for events below the priority at
               start of this scheduler recursion. This priority level will mostly be
               because another, earlier call of this function, preempted and currently
               suspended somewhere deeper on the OS stack, which already handles the event
               but it can also happen if the PCP has been used to temporarily raise the
               current priority. This would hinder us to serve triggered event already now.
                 We leave the function still (or again) being in a critical section. */
            break;
        }
        else if(pEvent->state == evState_triggered)
        {
            /* Associated tasks are due and they have a priority higher than all other
               currently activated ones. Before we execute them we need to acknowledge the
               event - only then we my leave the critical section. */
            pEvent->state = evState_inProgress;

            /* The current priority is changed synchronously with the acknowledge of the
               event. We need to do this still inside the same critical section. */
            rtos_currentPrio = pEvent->priority;

            /* Now handle the event, i.e. launch and execute all associated tasks. This is
               of course not done inside the critical section. We leave it now. */
            rtos_osResumeAllInterrupts();
            launchAllTasksOfEvent(pEvent);

            /* The executed tasks can have temporarily changed the current priority, but
               here it needs to be the event's priority again.
                 The assertion can fire if an OS task raised the priority using the PCP API
               but didn't restore it again. */
            assert(rtos_currentPrio == pEvent->priority);

            /* The event is entirely processes, we can release it. This must not be done
               before we are again in the next critical section. */
            rtos_osSuspendAllInterrupts();
            assert(pEvent->state == evState_inProgress);
            pEvent->state = evState_idle;
            assert(rtos_noEventsPending > 0);
            -- rtos_noEventsPending;

            /* The next event to check is not necessarily the next in order. If we allow
               events to have the same priority then we need to ensure that we have checked
               all other events of same priority before we advance to one of lower
               priority. (If we don't allow same priorities then this condition is
               implicitly fulfilled and we can always advance with the next event in order,
               which is surely of lower priority.)
                 Note, this consideration leads to the repeated check of the same events.
               Example: A and B were events of same priority and appear in the list in this
               order. Event A can be triggered while event B is proceessed. We must check A
               before we check the successor of B, e.g. C, which has lower priority. While
               running the tasks of A, event B can have been triggered again, and so on. In
               an extreme situation, we would infinitly loop and alternatingly process A
               and B, but C would never been visited and suffer from starvation. (Easy to
               get: A task of A triggers event B and a task of B triggers event A.) */
#if RTOS_SUPPORT_EVENTS_OF_SAME_PRORITY == 1
            /* Same priorities are allowed for several events. After we have served an
               event of priority n, we check the first event of this priority n as next
               one. All event specifications, inlcuding priorities, are staically and we
               have prepared a link from each event to the first in list order, which has
               the same priority but which is not the event itself. Examples:
               - A, B, C have prio n, D is the successor of C with prio n-1:
                 - A is linked to B
                 - B and C are linked to A
               - A has prio n, B has prio n-1, C has prio n-2:
                 - A is linked to B
                 - B is linked to C
                 Note, this scheme doesn't lead to a fair round robin for groups of events
               with same priority but this is not a contradiction with the meaning of
               priorities or priority based scheduling. */

            /* Proceed with preceding events of same priority (if any). */
            pEvent = pEvent->pNextScheduledEvent;
#else
            /* Proceed with next event. */
            ++ pEvent;
#endif
        }
        else
        {
            /* Ignore events, which have not been set (yet). */

            /* There must be no events in state inProgress, which have a priority above the
               current one. */
            assert(pEvent->state == evState_idle);

            /* Proceed with next event. */
            ++ pEvent;

        } /* End if(Which event state?) */

    } /* End While(All events we possibly need to handle) */

    /* Here we are surely still or again inside a critical section. */
    rtos_currentPrio = prioAtEntry; /* The initial priority is restored again. */

} /* End of rtos_processTriggeredEvents */



/**
 * System call handler implementation to create and run a task in another process. Find
 * more details in rtos_osRunTask().\n
 *   Start a user task. A user task is a C function, which is executed in user mode and in
 * a given process context. The call is synchronous; the calling user context is
 * immediately preempted and superseded by the started task. The calling user context is
 * resumed when the task function ends - be it gracefully or by exception/abortion.\n
 *   The started task inherits the priority of the calling user task. It can be preempted
 * only by contexts of higher priority.\n
 *   The function requires sufficient privileges. By default the use of this function is
 * forbidden. The operating system startup code can however use
 * rtos_osGrantPermissionRunTask() to enable particular pairs of calling and target
 * process for this service. The task can generally not be started in the OS context.\n
 *   The function cannot be used recursively. The created task cannot in turn make use of
 * rtos_runTask().
 *   @return
 * The executed task function can return a value, which is propagated to the calling user
 * context if it is positive. A returned negative task function result is interpreted as
 * failing task and rtos_runTask() returns #RTOS_ERR_PRC_USER_ABORT instead.
 * Furthermore, this event is counted as process error in the target process.
 *   @param pidOfCallingTask
 * The ID of the process the task belongs to, which invoked the system call.
 *   @param pUserTaskConfig
 * The read-only configuration data for the task. In particular the task function pointer
 * and the ID of the target process.
 *   @param taskParam
 * This argument is meaningless to the function. The value is just passed on to the started
 * task function. The size is large enough to convey a pointer.
 *   @remark
 * This function must never be called directly. The function is only made for placing it in
 * the global system call table.
 */
uint32_t rtos_scFlHdlr_runTask( unsigned int pidOfCallingTask
                              , const rtos_taskDesc_t *pUserTaskConfig
                              , uintptr_t taskParam
                              )
{
    if(!rtos_checkUserCodeReadPtr(pUserTaskConfig, sizeof(rtos_taskDesc_t)))
    {
        /* User code passed in an invalid pointer. We must not even touch the contents.
             Note, the next function won't return. */
        rtos_osSystemCallBadArgument();
    }

    /* This code depends on specific number of processes, we need a check. The
       implementation requires consistent maintenance with other function
       rtos_osGrantPermissionRunTask() */
#if RTOS_NO_PROCESSES != 4
# error Implementation requires the number of processes to be four
#endif

    /* Now we can check the index of the target process. */
    const unsigned int idxCalledPrc = pUserTaskConfig->PID - 1u;
    if(idxCalledPrc > 3)
        rtos_osSystemCallBadArgument();

    const uint16_t mask = 0x1 << (4u*(pidOfCallingTask-1u) + idxCalledPrc);
    if((rtos_runTask_permissions & mask) != 0)
    {
        /* We forbid recursive use of this system call not because it would be technically
           not possible but to avoid an overflow of the supervisor stack. Each creation of
           a user task puts a stack frame on the SV stack. We cannnot detect a recursion
           but we can hinder the SV stack overflow by making the current context's priority
           a gate for further use of this function: The next invokation needs to appear at
           higher level. This will limit the number of stack frames similar as this is
           generally the case for interrupts.
             Note, a user task can circumvent the no-recursion rule by abusing the priority
           ceiling protocol to increment the level by one in each recursion. This is
           technically alright and doesn't impose a risk. The number of available PCP
           levels is strictly limited and so is then the number of possible recursions. The
           SV stack is protected. */
        static uint32_t SDATA_OS(minPriorityLevel_) = 0;

        uint32_t currentLevel = INTC.CPR_PRC0.R
               , minPriorityLevelOnEntry;

        rtos_osSuspendAllInterrupts();
        const bool isEnabled = currentLevel >= minPriorityLevel_;
        if(isEnabled)
        {
            minPriorityLevelOnEntry = minPriorityLevel_;
            minPriorityLevel_ = currentLevel+1;
        }
        rtos_osResumeAllInterrupts();

        if(isEnabled)
        {
            /* All preconditions fulfilled, lock is set, run the task. */
            const int32_t taskResult = rtos_osRunUserTask(pUserTaskConfig, taskParam);

            /* Restore the pre-requisite for future use of this system call. */
            rtos_osSuspendAllInterrupts();

            /* The warning "'minPriorityLevelOnEntry' may be used uninitialized" is locally
               switched off. Justification: Variable is only used if(isEnabled) and then it
               is initialized, too. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
            minPriorityLevel_ = minPriorityLevelOnEntry;
#pragma GCC diagnostic pop

            rtos_osResumeAllInterrupts();

            return (uint32_t)taskResult;
        }
        else
        {
            /* Bad use of function, penalty is task abortion.
                 Note, this function does not return. */
            rtos_osSystemCallBadArgument();
        }
    }
    else
    {
        /* The user doesn't have the privileges to run the aimed task. This is a severe
           user code error, which is handled with an exception, task abort and counted
           error.
             Note, this function does not return. */
        rtos_osSystemCallBadArgument();
    }
} /* End of rtos_scFlHdlr_runTask */




/**
 * Priority ceiling protocol (PCP), partial scheduler lock: All tasks up to the specified
 * task priority level won't be handled by the CPU any more. This function is intended for
 * implementing mutual exclusion of sub-sets of tasks.\n
 *   Note, the use of the other function pairs\n
 *   - rtos_osEnterCriticalSection() and\n
 *   - rtos_osLeaveCriticalSection()\n
 * or\n
 *   - rtos_osSuspendAllInterrupts() and\n
 *   - rtos_osResumeAllInterrupts()\n
 * locks all interrupt processing and no other task (or interrupt handler) can become
 * active while the task is inside the critical section code. With respect to behavior,
 * using the PCP API is much better: Call this function with the highest priority of all
 * tasks, which should be locked, i.e. which compete for the resource or critical section
 * to protect. This may still lock other, non competing tasks, but at least all interrupts
 * and all non competing tasks of higher priority will be served.\n
 *   The major drawback of using the PCP instead of the interrupt lock API is the
 * significantly higher expense; particularly at the end of the critical section, when
 * resuming the scheduling again: A recursive call of the scheduler is required to see if
 * some tasks of higher priority had become ready during the lock time. Therefore, locking
 * the interrupts is likely the better choice for very short critical sections.\n
 *   To release the protected resource or to leave the critical section, call the
 * counterpart function rtos_osResumeAllTasksByPriority(), which restores the original
 * task priority level.
 *   @return
 * The task priority level at entry into this function (and into the critical section) is
 * returned. This level needs to be restored on exit from the critical section using
 * rtos_osResumeAllTasksByPriority().
 *   @param suspendUpToThisTaskPriority
 * All tasks up to and including this priority will be locked, i.e. they won't be executed
 * even if they'd become ready. The CPU will not handle them until the priority level is
 * lowered again.
 *   @remark
 * The critical section shaped with this API from an OS task guarantees mutual exclusion
 * with critical section code shaped with the other API rtos_suspendAllTasksByPriority()
 * from a user code task.
 *   @remark
 * To support the use case of nested calls of OSEK/VDX like GetResource/ReleaseResource
 * functions, this function compares the stated value to the current priority level. If \a
 * suspendUpToThisTaskPriority is less than the current value then the current value is not
 * altered. The function still returns the current value and the calling code doesn't need
 * to take care: It can unconditionally end a critical section with
 * rtos_osResumeAllTasksByPriority() stating the returned priority level value. (The
 * resume function will have no effect in this case.) This makes the OSEK like functions
 * usable without deep inside or full transparency of the priority levels behind the scene;
 * just use the pairs of Get-/ResumeResource, be they nested or not.
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
 * Any change of the current priority level made with this function needs to be undone
 * using rtos_osResumeAllTasksByPriority() and still inside the same task. It is not
 * possible to consider this function a mutex, which can be acquired in one task activation
 * and which can be released in an arbitrary later task activation or from another task.\n
 *   An assertion in the scheduler will likely fire if the two PCP APIs are not properly
 * used in pairs.
 *   @remark
 * This function must be called from OS tasks only. Any attempt to use it from either an
 * ISR or in user mode code will lead to either a failure or privileged exception,
 * respectively.
 *   @remark
 * This function requires that msr[EE]=1 on entry.
 */
uint32_t rtos_osSuspendAllTasksByPriority(uint32_t suspendUpToThisTaskPriority)
{
#if 0
    /* The OS version of the API may even lock the supervisory tasks. Justification: All OS
       code belongs to the sphere of trusted code and generally has full control. OS code
       can e.g. lock all interrupts, which is even more blocking than this function. */
    if(suspendUpToThisTaskPriority > RTOS_MAX_LOCKABLE_TASK_PRIORITY)
        return;
#endif
    rtos_osSuspendAllInterrupts();
    const uint32_t prioBeforeChange = rtos_currentPrio;
    if(suspendUpToThisTaskPriority > prioBeforeChange)
        rtos_currentPrio = suspendUpToThisTaskPriority;
    rtos_osResumeAllInterrupts();

    return prioBeforeChange;
    
} /* rtos_osSuspendAllTasksByPriority */




/**
 * This function is called to end a critical section of code, which requires mutual
 * exclusion of two or more tasks. It is the counterpart of function
 * rtos_osSuspendAllInterruptsByPriority(), refer to that function for more details.\n
 *   @param resumeDownToThisTaskPriority
 * All tasks/interrupts above this priority level are resumed again. All tasks/interrupts
 * up to and including this priority remain locked.\n
 *   You will normally pass in the value got from the related call of
 * rtos_osSuspendAllInterruptsByPriority().\n
 *   Caution, this function lowers the current task priority level to the stated value
 * regardless of the initial value for the task. Accidentally lowering the task priority
 * level below the configured task priority (i. the priority inherited from the triggering
 * event) will have unpredictable consequences.
 *   @remark
 * This function must be called from OS tasks only. Any attempt to use it from either an
 * ISR or in user mode code will lead to either a failure or privileged exception,
 * respectively.
 *   @remark
 * This function requires that msr[EE]=1 on entry.
 */
void rtos_osResumeAllTasksByPriority(uint32_t resumeDownToThisTaskPriority)
{
    rtos_osSuspendAllInterrupts();
    if(resumeDownToThisTaskPriority < rtos_currentPrio)
    {
        rtos_currentPrio = resumeDownToThisTaskPriority;
        rtos_processTriggeredEvents();
    }
    rtos_osResumeAllInterrupts();

} /* rtos_osResumeAllTasksByPriority */



/**
 * An event, which becomes due may not be able to activate all its associated tasks because
 * they didn't terminate yet after their previous activation. It doesn't matter if this
 * happens because a cyclic task becomes due or because an event task has been triggered by
 * software (using rtos_triggerEvent()). The scheduler counts the failing activations on a
 * per event base. The current value can be queried with this function.
 *   @return
 * Get the current number of triggers of the given event, which have failed since start of
 * the RTOS scheduler. The counter is saturated and will not wrap around.\n
 *   The returned count can be understood as number of task overrun events for all
 * associated tasks.
 *   @param idEvent
 * Each event has its own counter. The value is returned for the given event. The range is 0
 * .. number of registered events minus one (double-checked by assertion).
 *   @remark
 * This function can be called from both, the OS context and a user task.
 */
unsigned int rtos_getNoActivationLoss(unsigned int idEvent)
{
    if(idEvent < rtos_noEvents)
        return getEventByID(idEvent)->noActivationLoss;
    else
    {
        assert(false);
        return UINT_MAX;
    }
} /* End of rtos_getNoActivationLoss */