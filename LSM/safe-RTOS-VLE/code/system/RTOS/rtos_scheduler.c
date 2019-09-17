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
 *   The activation of a task can be done by software, using API function
 * rtos_triggerEvent() or it can be done by the scheduler on a regular time base. In the
 * former case the task is called an event task, the latter is a cyclic task with fixed
 * period time.\n
 *   The RTOS implementation is tightly connected to the implementation of interrupt
 * services. Interrupt services, e.g. to implement I/O operations for the tasks, are
 * registered with rtos_osInstallInterruptHandler().\n
 *   Any I/O interrupts can be combined with the tasks. Different to most RTOS we don't
 * impose a priority ordering between tasks and interrupts. A conventional design would put
 * interrupt service routines (ISR) at higher priorities than the highest task priority but
 * this is not a must. Certain constraints result from safety considerations not from
 * technical aspects.\n
 *   Effectively, there's no difference between tasks and ISRs. All what has been said for
 * tasks with respect to priority, states and preemption holds for ISRs and the combination
 * of tasks and ISRs, too.\n
 *   Compared to other "safe" RTOSs a quite little number of lines of code is required to
 * implement the RTOS - this is because of the hardware capabilities of the interrupt
 * controller INTC, which has much of an RTOS kernel in hardware. The RTOS is just a
 * wrapper around these hardware capabilities. The reference manual of the INTC partly
 * reads like an excerpt from the OSEK/VDX specification; it effectively implements the
 * basic task conformance classes BCC1 and mostly BCC2 from the standard. Since we barely
 * add software support, our operating system is by principle restricted to these
 * conformance classes.\n
 *   Basic conformance class means that a task cannot suspend intentionally and ahead of
 * its normal termination. Once started, it needs to be entirely executed. Due to the
 * strict priority scheme it'll temporarily suspend only for the sake of tasks of higher
 * priority (but not voluntarily or on own desire). Another aspect of the same is that the
 * RTOS doesn't know task to task events -- such events are usually the way intentional
 * suspension and later resume of tasks is implemented.
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
 *   rtos_osCreateEvent
 *   rtos_osRegisterInitTask
 *   rtos_osRegisterUserTask
 *   rtos_osRegisterOSTask
 *   rtos_osGrantPermissionRunTask
 *   rtos_osInitKernel
 *   rtos_osTriggerEvent
 *   rtos_scFlHdlr_triggerEvent
 *   rtos_scFlHdlr_runTask
 *   rtos_getNoActivationLoss
 * Module inline interface
 * Local functions
 *   registerTask
 *   checkEventDue
 *   onOsTimerTick
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

/** Abbreviation of initialization code for a task descriptor. Default is an inactive
    task. */
#define DEFAULT_EVENT(idEv)                         \
            [idEv] = { .tiDue = 0                   \
                       , .tiCycleInMs = 0           \
                       , .noActivationLoss = 0      \
                       , .taskAry = NULL            \
                       , .noTasks = 0               \
                       , .priority = 0              \
                     }

/* The assembler code doesn't have access to all defines found in the sphere of C code.
   This makes it essential to have a cross check here, were we can access the definitions
   from both spheres. */
#if RTOS_KERNEL_PRIORITY != RTOS_PCP_KERNEL_PRIO                                        \
    ||  RTOS_MAX_LOCKABLE_PRIORITY != RTOS_PCP_MAX_LOCKABLE_PRIO                        \
    ||  RTOS_NO_ERR_PRC != RTOS_NO_CAUSES_TASK_ABORTION                                 \
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
      Note, since we are hardware limited to eight events we use a statically allocated
    array of fixed size for all possible events. A resource optimized implementation could
    use an application defined macro to tailor the size of the array and it could put the
    task configuration data into ROM. */
typedef struct eventDesc_t
{
    /** The next due time. At this time, the event will activate the associated task set.*/
    unsigned int tiDue;

    /** The period time of the (cyclic) event in ms. The permitted range is 0..2^30-1.\n
          0 means no regular, timer controlled activation. The event is only enabled for
        software trigger using rtos_triggerEvent() (by interrupts or other tasks). */
    unsigned int tiCycleInMs;

    /** The priority of the event (and thus of all associated user tasks, which inherit the
        prioritry) in the range 1..(#RTOS_KERNEL_PRIORITY-1). Different events can share
        the same priority or have different priorities: The execution of their associated
        tasks, will then be sequenced when they become due at same time or with overlap.\n
          Note, if the event has a priority above #RTOS_MAX_LOCKABLE_PRIORITY then only
        those tasks can be associated, which belong to the process with highest PID in use.
        This is a safety constraint. */
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

} eventDesc_t;


/*
 * Local prototypes
 */

/* The prototypes of all possible software interrupt service routines. */
#define swInt(idEv)    static void swInt##idEv(void);
swInt(0)
swInt(1)
swInt(2)
swInt(3)
swInt(4)
swInt(5)
swInt(6)
swInt(7)
#undef swInt


/*
 * Data definitions
 */

/** The list of all tasks. */
static rtos_taskDesc_t _taskCfgAry[RTOS_MAX_NO_USER_TASKS] SECTION(.data.OS._taskCfgAry) =
    { [0 ... (RTOS_MAX_NO_USER_TASKS-1)] = { .addrTaskFct = 0
                                             , .PID = 0
                                             , .tiTaskMax = 0
                                           }
    };

/** The list of all process initialization tasks. */
static rtos_taskDesc_t _initTaskCfgAry[1+RTOS_NO_PROCESSES] SECTION(.data.OS._initTaskCfgAry) =
    { [0 ... RTOS_NO_PROCESSES] = { .addrTaskFct = 0
                                    , .PID = 0
                                    , .tiTaskMax = 0
                                  }
    };

/** The number of registered tasks. The range is 0..#RTOS_MAX_NO_USER_TASKS. */
static unsigned int _noTasks SECTION(.sdata.OS._noTasks) = 0;

/** The list of task activating events. */
static eventDesc_t SECTION(.data.OS._eventAry) _eventAry[RTOS_MAX_NO_EVENTS] =
    { DEFAULT_EVENT(0), DEFAULT_EVENT(1), DEFAULT_EVENT(2), DEFAULT_EVENT(3)
      , DEFAULT_EVENT(4), DEFAULT_EVENT(5), DEFAULT_EVENT(6), DEFAULT_EVENT(7)
    };

/** The number of created events. The range is 0..8. The events are implemented by
    software interrupts and these are limited by hardware to a number of eight. */
static unsigned int _noEvents SECTION(.sdata.OS._noEvents) = 0;

/** Time increment of one tick of the RTOS system clock. It is set at kernel initialization
    time to the configured period time of the system clock in Milliseconds
    (#RTOS_CLOCK_TICK_IN_MS). This way the unit of all time designations in the RTOS API
    always stays Milliseconds despite of the actually chosen clock rate. (An application of
    the RTOS can reduce the clock rate to the lowest possible value in order to save
    overhead.) The normal settings are a clock rate of 1 kHz and
    #RTOS_CLOCK_TICK_IN_MS=1.\n
      The variable is initially set to zero to hold the scheduler during RTOS
    initialization. */
static unsigned long _tiOsStep SECTION(.sdata.OS._tiStepOs) = 0;

/** RTOS sytem time in Milliseconds since start of kernel. */
static unsigned long _tiOs SECTION(.sdata.OS._tiOs) = (unsigned long)-1;

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
static uint16_t SDATA_OS(_runTask_permissions) = 0;
#else
# error Implementation depends on four being the number of processes
#endif


/*
 * Function implementation
 */

/**
 * Registration of a task. Normal, event activated tasks and process initialization tasks
 * can be registered for later execution. This can be both, user mode tasks and operating
 * system tasks. This function is repeatedly called by the application code as often as
 * tasks are required.\n
 *   All calls of this function need to be done prior to the start of the kernel using
 * rtos_osInitKernel().
 *   @return
 * \a rtos_err_noError (zero) if the task could be registered. The maximum number of normal
 * tasks is limited to #RTOS_MAX_NO_USER_TASKS (regardless, how they are distributed among
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
    if(_tiOsStep != 0)
        return rtos_err_configurationOfRunningKernel;

    /* The event need to be created before the task can be registered. */
    if(idEvent >= _noEvents  &&  idEvent != EVENT_ID_INIT_TASK)
        return rtos_err_badEventId;

    /* The process ID needs to be in the fixed and limited range. */
    if(PID > RTOS_NO_PROCESSES)
        return rtos_err_badProcessId;

    /* The number of runtime tasks is constraint by compile time configuration. */
    if(_noTasks >= RTOS_MAX_NO_USER_TASKS  &&  idEvent != EVENT_ID_INIT_TASK)
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
        assert(idxP < sizeOfAry(_initTaskCfgAry));
        if(_initTaskCfgAry[idxP].addrTaskFct != 0)
            return rtos_err_initTaskRedefined;

        _initTaskCfgAry[idxP].addrTaskFct = addrTaskFct;
        _initTaskCfgAry[idxP].tiTaskMax = tiTaskMaxInUs * 120;
        _initTaskCfgAry[idxP].PID = PID;
    }
    else
    {
        /* Add the new runtime task to the array. All tasks associated with an event need
           to form a consecutive list. We need to find the right location to insert the
           task and we need to consider an update of all events with higher index. */
        unsigned int idxEv
                   , noTasksBefore;
        for(noTasksBefore=0, idxEv=0; idxEv<=idEvent; ++idxEv)
            noTasksBefore += _eventAry[idxEv].noTasks;

        unsigned int idxTask;
        for(idxTask=_noTasks; idxTask>noTasksBefore; --idxTask)
            _taskCfgAry[idxTask] = _taskCfgAry[idxTask-1];

        _taskCfgAry[noTasksBefore].addrTaskFct = addrTaskFct;
        _taskCfgAry[noTasksBefore].tiTaskMax = tiTaskMaxInUs * 120;
        _taskCfgAry[noTasksBefore].PID = PID;
        ++ _noTasks;

        /* Associate the task with the specified event. */
        if(_eventAry[idEvent].taskAry == NULL)
        {
            _eventAry[idEvent].taskAry = &_taskCfgAry[noTasksBefore];
            assert(_eventAry[idEvent].noTasks == 0);
        }
        ++ _eventAry[idEvent].noTasks;

        for(/* it is: idxEv=idEvent+1 */; idxEv<_noEvents; ++idxEv)
            if(_eventAry[idxEv].taskAry != NULL)
                ++ _eventAry[idxEv].taskAry;
    }

    return rtos_err_noError;

} /* End of registerTask */




/*
 * The SW interrupt service routines are implemented by macro and created multiple times.
 * Each function implements one event. It activates all associated tasks and acknowledges
 * the software interrupt flag.
 */
#define swInt(idEv)                                                                 \
    static void swInt##idEv(void)                                                   \
    {                                                                               \
        const rtos_taskDesc_t *pTaskConfig = &_eventAry[idEv].taskAry[0];           \
        unsigned int u = _eventAry[idEv].noTasks;                                   \
        while(u-- > 0)                                                              \
        {                                                                           \
            if(pTaskConfig->PID > 0)                                                \
                rtos_osRunTask(pTaskConfig, /* taskParam */ idEv);                  \
            else                                                                    \
                ((void (*)(void))pTaskConfig->addrTaskFct)();                       \
                                                                                    \
            ++ pTaskConfig;                                                         \
        } /* End while(Run all tasks associated with the event) */                  \
                                                                                    \
        /* Acknowledge the interrupt bit of the software interrupt. */              \
        *((vuint8_t*)&INTC.SSCIR0_3.R + (idEv)) = 0x01;                             \
    }

swInt(0)
swInt(1)
swInt(2)
swInt(3)
swInt(4)
swInt(5)
swInt(6)
swInt(7)
#undef swInt


/**
 * Process the up to eight events. They are checked for becoming due meanwhile and the
 * associated tasks are made ready in case (i.e. the software interrupt is raised in the
 * INTC).
 */
static inline void checkEventDue(void)
{
    vuint8_t *pINTC_SSCIR = (vuint8_t*)&INTC.SSCIR0_3.R;
    unsigned int idxEvent;
    eventDesc_t *pEvent = &_eventAry[0];

    /* In the implementation of this routine we neglect a minor race condition in
       recognizing and reposting an activation loss. If #RTOS_KERNEL_PRIORITY is lower than
       15 and if an ISR with priority greater than #RTOS_KERNEL_PRIORITY makes use of
       rtos_osTriggerEvent() and if this function and the ISR try to trigger one and the
       same event at the same time then an activation loss may or may not be reported. The
       expected and reasonable behavior is not affected:\n
         The event is surely triggered if it was not yet triggered. The activation loss
       counter should be incremented by one (as there is surely one instead of two
       simultaneously attempted triggers) but it may be that none of the two competitors
       have found the event already triggered and the counter is not incremented.\n
         The event is surely not triggered if it was already triggered. Both competitors
       will recognize the situation but now the race condition is in incrementing the
       counter; one may override the increment of the other resulting in the same effect -
       the counter is not surely incremented by two but may be incremented only by one.\n
         We could easily sort this out by adding a critical section. We don't do this for
       the follwoing reasons:\n
         We can put the test-and-modify operation on the interrupt flag into a critical
       section. Then we need to do this up to eight times, which is expensive.\n
         We can put the entire loop into the critical section, which makes the duration of
       the critical section long - and we are talking about a scenario with IRQs of very
       high priority.\n
         The probablity of ever seeing the effect is little. If we have an ISR of high
       priority using rtos_osTriggerEvent() then it will most likely trigger a dedicated,
       non-cyclic event, but not one, which is regularly triggered by the scheduler, too.\n
         Pathologic contructs, in which two or more high prior ISRs compete with the
       scheduler for one and the same event are not considered here, the results are
       accordingly. */
    for(idxEvent=0; idxEvent<_noEvents; ++idxEvent, ++pEvent, ++pINTC_SSCIR)
    {
        if(pEvent->tiCycleInMs > 0)
        {
            if((signed int)(pEvent->tiDue - _tiOs) <= 0)
            {
                /* Task is due. Read software interrupt bit. If it is still set then we
                   have a task overrun otherwise we activate task by requesting the
                   software interrupt. */
                if(*pINTC_SSCIR == 0)
                {
                    /* Put task into ready state (and leave the activation to the INTC). */
                    *pINTC_SSCIR = 3;

                    /* Operation successful. */
                }
                else
                {
                    /* CLRi is still set, interrupt has not completed yet, task has not
                       terminated yet. We neither set nor clear the interrupt bit. */

                    /* Counting the loss events requires a critical section. The loss
                       counter can be written concurrently from a task invoking
                       rtos_triggerEvent(). Here, the implementation of the critical
                       section is implicit, this code is running on highest interrupt
                       level. */
                    const unsigned int noActivationLoss = _eventAry[idxEvent].noActivationLoss
                                                          + 1;
                    if(noActivationLoss > 0)
                        _eventAry[idxEvent].noActivationLoss = noActivationLoss;
                }

                /* Adjust the due time.
                     Note, we could queue task activations for cyclic tasks by not adjusting
                   the due time. Some limitation code would be required to make this safe.
                     Alternatively, we can implement an activation queue of length one by
                   acknowledging the IRQ flag on entry into the SW IRQ handler: The next SW
                   IRQ can be requested while the previous one is still being handled. */
                pEvent->tiDue += pEvent->tiCycleInMs;

            } /* End if(Event is due?) */
        }
        else
        {
            /* Non regular events: nothing to be done. These events are triggered only by
               explicit software call of rtos_triggerEvent(). */

        } /* End if(Timer or application software activated task?) */
    } /* End for(All configured events) */

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
 * #RTOS_KERNEL_PRIORITY. One can set it to the highest available interrupt priority, but
 * this is not a must. Chosing the highest possible priority would be driven by the concept
 * that the scheduler can force the execution of an application task in disfavor of any
 * other context. No difference is then made between interrupt service routines and regular
 * application tasks.\n
 *   However, most RTOSs distiguish between pure interrupt service routines and application
 * tasks and let the latter generally have a priority lower than all of the former. And
 * they would never preempt an interrupt in favor of an application task. If this behavior
 * is modelled then the priority demand would change to "the highest application task
 * priority plus one".\n
 *   An important aspect in this discussion is the availablity of the priority ceiling
 * protocol (PCP) for mutual exclusion of contexts. It is available only up to and
 * including priority level #RTOS_MAX_LOCKABLE_PRIORITY (The range till
 * #RTOS_KERNEL_PRIORITY-1 is reserved for non-suppressible safety tasks). An ISR running
 * above this level cannot have a critical section with user code and data exchange needs
 * to appy more complicated techniques, e.g. double-buffering.
 */
static void onOsTimerTick(void)
{
    /* Note, the scheduler function needs to run at highest priority, which means that no
       task or ISR can preempt this code. No mutual exclusion code is required. */

    /* Update the system time. */
    _tiOs += _tiOsStep;

    /* The scheduler is most simple; the only condition to make a task ready is the next
       periodic due time. The task activation is fully left to the hardware of the INTC and
       we don't have to bother with priority handling or task/context switching. */
    checkEventDue();

        /* Acknowledge the timer interrupt in the causing HW device. */
    PIT.TFLG0.B.TIF = 0x1;

} /* End of onOsTimerTick */



/**
 * Creation of an event. The event can be cyclically triggering or software triggerd.
 * An event is needed to activate a user task. Therefore, any reasonable application will
 * ceate at least one event.\n
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
 * The priority of the event in the range 1..(#RTOS_KERNEL_PRIORITY-1). Different events
 * can share the same priority or have different priorities. The priotrity of an event is
 * the priority of all associated tasks at the same time. The execution of tasks, which
 * share the priority will be serialized when they are activated at same time or with
 * overlap.\n
 *   Note the safety constraint that the highest permitted priorities,
 * #RTOS_MAX_LOCKABLE_PRIORITY+1 till #RTOS_KERNEL_PRIORITY-1, are available only to
 * events, which solely have tasks associated that belong to the process with highest
 * process ID in use.\n
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
    if(_noEvents >= RTOS_MAX_NO_EVENTS)
        return rtos_err_tooManyEventsCreated;

    /* The INTC permits priorities only in the range 0..15 and we exclude 0 since such a
       task would never become active. We furthermore exclude priorities equal to or
       greater then the one of the scheduler to avoid unwanted blocking states. */
    if(priority == 0  ||  priority >= RTOS_KERNEL_PRIORITY)
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

    /* Add the new event to the array and initialize the data structure. */
    _eventAry[_noEvents].tiCycleInMs = tiCycleInMs;
    _eventAry[_noEvents].tiDue = tiFirstActivationInMs;
    _eventAry[_noEvents].priority = priority;
    _eventAry[_noEvents].minPIDForTrigger = minPIDToTriggerThisEvent;
    _eventAry[_noEvents].noActivationLoss = 0;
    _eventAry[_noEvents].taskAry = NULL;
    _eventAry[_noEvents].noTasks = 0;

    /* Assign the next available event ID. */
    *pEventId = _noEvents++;

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
 * tasks is limited to #RTOS_MAX_NO_USER_TASKS (regardless, how they are distributed among
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
 * tasks is limited to #RTOS_MAX_NO_USER_TASKS (regardless, how they are distributed among
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
    _runTask_permissions |= mask;

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

    /* The user must have registered at minimum one task and must have associated it with a
       event. */
    if(_tiOsStep != 0)
        errCode = rtos_err_configurationOfRunningKernel;
    else if(_noEvents == 0  ||  _noTasks == 0)
        errCode = rtos_err_noEvOrTaskRegistered;

    /* Fill all process stacks with the empty-pattern, which is applied for computing the
       stack usage. */
    bool isProcessConfiguredAry[1+RTOS_NO_PROCESSES] = {[0 ... RTOS_NO_PROCESSES] = false};
    if(errCode == rtos_err_noError)
        errCode = rtos_initProcesses(isProcessConfiguredAry);

    unsigned int idxTask
               , maxPIDInUse = 0;
    /* Find the highest PID in use. */
    for(idxTask=0; idxTask<_noTasks; ++idxTask)
        if(_taskCfgAry[idxTask].PID > maxPIDInUse)
            maxPIDInUse = _taskCfgAry[idxTask].PID;

    /* A task must not belong to an invalid configured process. This holds for init and for
       run time tasks. */
    unsigned int idxP;
    if(errCode == rtos_err_noError)
    {
        for(idxTask=0; idxTask<_noTasks; ++idxTask)
        {
            assert(_taskCfgAry[idxTask].PID < sizeOfAry(isProcessConfiguredAry));
            if(!isProcessConfiguredAry[_taskCfgAry[idxTask].PID])
                errCode = rtos_err_taskBelongsToInvalidPrc;

        } /* For(All registered runtime tasks) */

        for(idxP=0; idxP<1+RTOS_NO_PROCESSES; ++idxP)
        {
            /* Note, the init task array is - different to the runtime task array
               _taskCfgAry - ordered by PID. The field PID in the array entries are
               redundant. A runtime check is not appropriate as this had happened at
               registration time. We can place a simple assertion here. */
            if(_initTaskCfgAry[idxP].addrTaskFct != 0)
            {
                assert(_initTaskCfgAry[idxP].PID == idxP);
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
        if((_runTask_permissions & mask) != 0)
            errCode = rtos_err_runTaskBadPermission;
    }

    /* We could check if a process, an init task is registered for, has a least one runtime
       task. However, it is not harmful if not and there might be pathologic applications,
       which solely consist of I/O driver callbacks. */

    if(errCode == rtos_err_noError)
    {
        unsigned int idxEv;
        for(idxEv=0; idxEv<_noEvents; ++idxEv)
        {
            /* Check task configuration: Events without an associated task are useless and
               point to a configuration error. */
            if(_eventAry[idxEv].noTasks == 0)
                errCode = rtos_err_eventWithoutTask;

            /* If an event has a priority above #RTOS_MAX_LOCKABLE_PRIORITY then only those
               tasks can be associated, which belong to the process with highest PID in use
               or OS tasks. This is a safety constraint. */
            if(_eventAry[idxEv].priority > RTOS_MAX_LOCKABLE_PRIORITY)
            {
                for(idxTask=0; idxTask<_eventAry[idxEv].noTasks; ++idxTask)
                {
                    const unsigned int PID = _eventAry[idxEv].taskAry[idxTask].PID;
                    if(PID > 0  &&  PID != maxPIDInUse)
                        errCode = rtos_err_highPrioTaskInLowPrivPrc;
                }
            } /* End if(Unblockable priority is in use by event) */
        } /* for(All registered events) */
    }

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
    _tiOs = (unsigned long)-1;
    _tiOsStep = 0;

    /* We can register all SW interrupt service routines. */
    if(errCode == rtos_err_noError)
    {
        /* Install all software interrupts, which implement the events. */
        #define INSTALL_SW_IRQ(idEv)                                                \
            if(idEv < _noEvents)                                                    \
            {                                                                       \
                /* Reset a possibly pending interrupt bit of the SW interrupt. */   \
                *((vuint8_t*)&INTC.SSCIR0_3.R + (idEv)) = 0x01;                     \
                rtos_osInstallInterruptHandler( &swInt##idEv                        \
                                              , /* vectorNum */ idEv                \
                                              , _eventAry[idEv].priority            \
                                              , /* isPreemptable */ true            \
                                              );                                    \
            }
        INSTALL_SW_IRQ(0)
        INSTALL_SW_IRQ(1)
        INSTALL_SW_IRQ(2)
        INSTALL_SW_IRQ(3)
        INSTALL_SW_IRQ(4)
        INSTALL_SW_IRQ(5)
        INSTALL_SW_IRQ(6)
        INSTALL_SW_IRQ(7)
        #undef INSTALL_SW_IRQ

        /* Disable all PIT timers during configuration. */
        PIT.PITMCR.R = 0x2;

        /* Install the interrupt service routine for cyclic timer PIT 0. It drives the OS
           scheduler for cyclic task activation. */
        rtos_osInstallInterruptHandler( &onOsTimerTick
                                      , /* vectorNum */ 59
                                      , RTOS_KERNEL_PRIORITY
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
    }

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
        if(_initTaskCfgAry[idxP].addrTaskFct != 0)
        {
            if(isProcessConfiguredAry[idxP])
            {
                /* Everything is alright. Run the initialization task. A negative return
                   value is defined to be an error. (This needs to be considered by the
                   implementation of the task.) */
                int32_t resultInit;
                if(_initTaskCfgAry[idxP].PID == 0)
                {
                    /* OS initialization function: It is a normal sub-function call; we are
                       here in the OS context. */
                    resultInit = ((int32_t (*)(void))_initTaskCfgAry[idxP].addrTaskFct)();
                }
                else
                {
                    /* The initialization function of a process is run as a task in that
                       process, which involves full exception handling and possible abort
                       causes. */
                    resultInit = rtos_osRunInitTask(&_initTaskCfgAry[idxP]);
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
        _tiOsStep = RTOS_CLOCK_TICK_IN_MS;

        rtos_osResumeAllInterrupts();
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
 *   This function can be called from any task or ISR. However, if the calling task belongs
 * to the set of tasks associated with \a idEvent, then it'll have no effect but an
 * accounted activation loss; an event can be re-triggered only after all associated
 * activations have been completed. There is no activation queueing. The function returns \a
 * false in this case.\n
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
    /* The events are related to the eight available software interrupts. Each SWIRQ is
       controlled by two bits in one out of two status registers of the INTC (RM, 28, p.
       911ff). Access to INTC_SSCIR is described at 28.4.2 and 28.4.7. */
    bool intFlagNotYetSet = true;
    vuint8_t * const pINTC_SSCIR = (vuint8_t*)&INTC.SSCIR0_3.R + idEvent;

    /* To make this function reentrant with respect to one and the same target SW IRQ, we
       need to encapsulate the flag-test-and-set operation in a critical section.
         As a side effect, we can use the same critical section for race condition free
       increment of the error counter in case. */
    const uint32_t msr = rtos_osEnterCriticalSection();
    {
        /* Read software interrupt bit of the task. If it is still set then we have an
           overrun of the associated tasks. Otherwise we activate the tasks by requesting
           the related software interrupt. */
        if(*pINTC_SSCIR == 0)
        {
            /* Put task into ready state (and leave the activation to the INTC). It
               is important to avoid a read-modify-write operation, don't simply
               set a single bit, the other three interrupts in the same register
               could be harmed.
                 The time gap between register read and write shapes a race condition with
               the scheduler for cyclic events and generally with other tasks using this
               function. An activation loss may be not recognized. */
            *pINTC_SSCIR = 0x3;

            /* Operation successful. */
        }
        else
        {
            /* CLRi is still set, interrupt has not completed yet, task has not terminated
               yet. We neither set nor clear the interrupt bit. */
            const unsigned int noActivationLoss = _eventAry[idEvent].noActivationLoss + 1;
            if(noActivationLoss > 0)
                _eventAry[idEvent].noActivationLoss = noActivationLoss;

            /* Activation failed. */
            intFlagNotYetSet = false;
        }
    }
    rtos_osLeaveCriticalSection(msr);

    return intFlagNotYetSet;

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
    /// @todo Do we really need a full handler here? Using the simple, non-preemptable
    // handler would delay the preemption by an activated task of higher priority until we
    // have entirely returned from the system call but we would burden the kernel stack
    // less. Which counts more?
    if(idEvent < _noEvents  &&  pidOfCallingTask >= _eventAry[idEvent].minPIDForTrigger)
        return (uint32_t)rtos_osTriggerEvent(idEvent);
    else
    {
        /* The user specified task ID is not in range. This is a severe user code error,
           which is handled with an exception, task abort and counted error.
             Note, this function does not return. */
        rtos_osSystemCallBadArgument();
    }
} /* End of rtos_scFlHdlr_triggerEvent */



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
    if((_runTask_permissions & mask) != 0)
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
    if(idEvent < _noEvents)
        return _eventAry[idEvent].noActivationLoss;
    else
    {
        assert(false);
        return UINT_MAX;
    }
} /* End of rtos_getNoActivationLoss */