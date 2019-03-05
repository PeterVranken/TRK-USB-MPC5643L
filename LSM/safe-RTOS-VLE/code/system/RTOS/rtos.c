/**
 * @file rtos.c
 * This file implements a simple yet "safe" Real Time Operating System (RTOS) for the
 * MPC5643L.\n
 *   The RTOS offers a strictly priority controlled scheduler. The user code is organized
 * in processes and tasks. Any task belongs to one of the processes. Different processes
 * have different privileges. The concept is to use the process with highest privileges for
 * the safety tasks.\n
 *   A task is activated by an event; an application will repeatedly use the API
 * rtos_createEvent() to define the conditions or points in time, when the tasks have to
 * become due.\n
 *   Prior to the start of the scheduler (and thus prior to the beginning of the
 * pseudo-parallel, concurrent execution of the tasks) all later used task are registered
 * at the scheduler; an application will repeatedly use the API rtos_registerTask().\n
 *   After all needed tasks are registered the application will start the RTOS' kernel
 * by calling the API rtos_initKernel() and task scheduling begins.\n
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
 * the call of API rtos_initKernel() is called the "idle task".)\n
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
 * registered with prc_installINTCInterruptHandler().\n
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
 * needs to start with a definition of "safe" in the given context of an RTOS:\n
 *   TODOC
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
 *   rtos_createEvent
 *   rtos_registerTask
 *   rtos_initKernel
 *   rtos_OS_triggerEvent
 *   rtos_scFlHdlr_triggerEvent
 *   rtos_getNoActivationLoss
 *   rtos_getStackReserve
 * Module inline interface
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
 * Local functions
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
#include "ihw_initMcuCoreHW.h"
#include "prc_process.h"
#include "pcp_sysCallPCP.h"
#include "rtos_defSysCalls.h"


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

/* The assembler code doen't have access to all defines found in the sphere of C code. This
   makes it essential to have a cross check here, were we can access both spheres. */
#if PCP_KERNEL_PRIORITY != RTOS_KERNEL_PRIORITY
# error Inconsistent definitions made in C and assembler code
#endif


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
          Note, if the event has the highest priority #RTOS_KERNEL_PRIORITY-1 then only
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
          The range of \a minPIDToTriggerThisEvent is 0 ... (#PRC_NO_PROCESSES+1). 0 and 1
        both mean, all processes may trigger the event, #PRC_NO_PROCESSES+1 means only OS code
        can trigger the event. */
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
    const prc_userTaskConfig_t * taskAry;

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
static prc_userTaskConfig_t _taskCfgAry[RTOS_MAX_NO_USER_TASKS]
                                                        SECTION(.data.OS._taskCfgAry) =
    { [0 ... (RTOS_MAX_NO_USER_TASKS-1)] = { .taskFct = NULL
                                             , .tiTaskMax = 0
                                             , .PID = 0
                                           }
    };

/** The list of all process initialization tasks. */
static prc_userTaskConfig_t _initTaskCfgAry[1+PRC_NO_PROCESSES]
                                                        SECTION(.data.OS._initTaskCfgAry) =
    { [0 ... PRC_NO_PROCESSES] = { .taskFct = NULL
                                   , .tiTaskMax = 0
                                   , .PID = 0
                                 }
    };

/** The number of registered tasks. The range is 0..#RTOS_MAX_NO_USER_TASKS. */
static unsigned int _noTasks SECTION(.sdata.OS._noTasks) = 0;

/** The list of task activating events. */
static eventDesc_t _eventAry[RTOS_MAX_NO_EVENTS] SECTION(.data.OS._eventAry) =
    { DEFAULT_EVENT(0), DEFAULT_EVENT(1), DEFAULT_EVENT(2), DEFAULT_EVENT(3)
      , DEFAULT_EVENT(4), DEFAULT_EVENT(5), DEFAULT_EVENT(6), DEFAULT_EVENT(7)
    };

/** The number of created events. The range is 0..8. The events are implemented by
    software interrupts and these are limited by hardware to a number of eight. */
static unsigned int _noEvents SECTION(.sdata.OS._noEvents) = 0;

/** Time increment of one tick of the RTOS system clock. It is set at kernel initialization
    time to the configured period time of the system clock in Milliseconds
    (#RTOS_CLOCK_TICK_IN_MS). This way the unit of all time designations in the RTOS API always
    stay Milliseconds despite of the actually chosen clock rate. (An application of the
    RTOS can reduce the clock rate to the lowest possible value in order to save overhead.)
    The normal settings are a clock rate of 1 kHz and #RTOS_CLOCK_TICK_IN_MS=1.\n
      The variable is initially set to zero to hold the scheduler during RTOS
    initialization. */
static unsigned long _tiOsStep SECTION(.sdata.OS._tiStepOs) = 0;

/** RTOS sytem time in Milliseconds since start of kernel. */
static unsigned long _tiOs SECTION(.sdata.OS._tiOs) = (unsigned long)-1;


/*
 * Function implementation
 */

/*
 * The SW interrupt service routines are implemented by macro and created multiple times.
 * Each function implements one event. It activates all associated tasks and acknowledges
 * the software interrupt flag.
 */
#define swInt(idEv)                                                                 \
    static void swInt##idEv(void)                                                   \
    {                                                                               \
        const prc_userTaskConfig_t *pTaskConfig = &_eventAry[idEv].taskAry[0];      \
        unsigned int u = _eventAry[idEv].noTasks;                                   \
        while(u-- > 0)                                                              \
        {                                                                           \
            if(pTaskConfig->PID > 0)                                                \
                rtos_OS_runTask(pTaskConfig, /* taskParam */ idEv);                 \
            else                                                                    \
                ((void (*)(void))pTaskConfig->taskFct)();                           \
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
       rtos_OS_triggerEvent() and if this function and the ISR try to trigger one and the
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
       priority using rtos_OS_triggerEvent() then it will most likely trigger a dedicated,
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

                /* Adjust the due time. */
                /// @todo We can queue task activations for cyclic tasks by not adjusting
                // the due time. Some limitation code is required to make this safe.
                // Alternatively, we can implement a queue length of one by acknowledging
                // the IRQ flag on entry into the SW IRQ handler: The next SW IRQ can be
                // requested while the proveious one is still being handled.
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
 * The OS default timer handler. In function rtos_initKernel(), it is associated with the
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
 * including priority level #RTOS_KERNEL_PRIORITY-2 (#RTOS_KERNEL_PRIORITY-1 is reserved
 * for non-suppressible safety tasks). An ISR running above this level cannot have a
 * critical section with user code and data exchange needs to appy more complicated
 * techniques, e.g. double-buffering.
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
 * rtos_initKernel().\n
 *   @return
 * All events are identified by a positive integer. Normally this ID is returned. The
 * maximum number of events is limited to #RTOS_MAX_NO_EVENTS by hardware constraints. If
 * the event cannot be created due to this constraint or if the event descriptor contains
 * invalid data then the function returns (unsigned int)-1. An assertion in the calling
 * code is considered appropriate to handle the error because it'll always be a static
 * configuration error.
 *   Note, it is guaranteed to the caller that the returned ID is not any meaningless
 * number. Instead, the ID is counted from zero in order of creating events. The first
 * call of this function will return 0, the second 1, and so on. This simplifies ID
 * handling in the application code, constants can mostly be applied as the IDs are
 * effectively known at compile time.
 *   @param pEventDesc
 * The properties of the requested event, mainly timing related.\n
 *   The object needs to be alive only during the call of this function, the proerties are
 * copied into the kernel owned runtime data structures.\n
 *   \a *pEventDesc contains the priority of the event (and the associated user tasks it'll
 * activate). Note, the order in which events are created can affect the priority in a
 * certain sense. If two events are created with same priority and when they at runtime
 * become due at the same OS time tick then the earlier created event will trigger its
 * associated user tasks before the later ceated event.
 *   @remark
 * Never call this function after the call of rtos_initKernel()!
 *   @remark
 * This function must be called by trusted code in supervisor mode only.
 */
unsigned int rtos_createEvent(const rtos_eventDesc_t *pEventDesc)
{
    /* The number of events is constraint by hardware (INTC) */
    if(_noEvents >= RTOS_MAX_NO_EVENTS)
        return (unsigned int)-1;

    /* The INTC permits priorities only in the range 0..15 and we exclude 0 since such a
       task would never become active. We furthermore exclude priorities equal to or
       greater then the one of the scheduler to avoid unwanted blocking states. */
    if(pEventDesc->priority == 0  ||  pEventDesc->priority >= RTOS_KERNEL_PRIORITY)
        return (unsigned int)-1;

    /* Check settings for non regularly activated tasks. */
    if(pEventDesc->tiCycleInMs == 0)
    {
        /* Avoid a useless and misleading setting. */
        if(pEventDesc->tiFirstActivationInMs != 0)
            return (unsigned int)-1;
    }

    /* The full 32 Bit range is avoided for time designations in order to have safe and
       unambiguous before and after decisions in a cyclic time model.
         Furthermore, no task must have the initial due time of 0xffffffff in order to not
       invalidate the startup logic of the scheduler (see rtos_initKernel()). */
    else if(((pEventDesc->tiCycleInMs | pEventDesc->tiFirstActivationInMs) & 0xc0000000) != 0)
        return (unsigned int)-1;

    /* Is the PID constraint plausible? */
    if(pEventDesc->minPIDToTriggerThisEvent > PRC_NO_PROCESSES+1)
        return (unsigned int)-1;

    /* Add the new event to the array and initialize the data structure. */
    _eventAry[_noEvents].tiCycleInMs = pEventDesc->tiCycleInMs;
    _eventAry[_noEvents].tiDue = pEventDesc->tiFirstActivationInMs;
    _eventAry[_noEvents].priority = pEventDesc->priority;
    _eventAry[_noEvents].minPIDForTrigger = pEventDesc->minPIDToTriggerThisEvent;
    _eventAry[_noEvents].noActivationLoss = 0;
    _eventAry[_noEvents].taskAry = NULL;
    _eventAry[_noEvents].noTasks = 0;

    return _noEvents++;

} /* End of rtos_createEvent */



/**
 * Registration of a user task. Normal, event activated tasks and process initialization
 * tasks can be registered for later execution. This function is repeatedly called by the
 * application code as often as tasks are required.\n
 *   All calls of this function need to be done prior to the start of the kernel using
 * rtos_initKernel().
 *   @return
 * \a true if the task could be registered. The maximum number of normal tasks is limited
 * to #RTOS_MAX_NO_USER_TASKS (regardless, how they are distributed among processes). The
 * maximum number of initialization tasks is one per process or for the OS. If the limit is
 * exceeded or if the task descriptor is invalid then the function returns \a false. An
 * assertion in the calling code is considered appropriate to handle the error because
 * it'll always be a static configuration error.
 *   @param pTaskDescAPI
 * The details of the registered task: The process it belongs to, the task function and the
 * time budget for execution.
 *   @param idEvent
 * Any (normal) task is activated by an event and a task without related event is useless.
 * This call associates the registered task with an already created event. See
 * rtos_createEvent().\n
 *   The association of the task with the process is made through field \a
 * pTaskDescAPI->PID.\n
 *   The order of registration of several tasks with one and the same event matters. The
 * tasks will be acivated in order of registration whenever the event becomes due or is
 * triggered by software.\n
 *   If a process or OS initialization task is registered, then \a idEvent is set to
 * #RTOS_EVENT_ID_INIT_TASK. It is allowed not to register an init task for a process or
 * for the OS but it is not possible to register more than one (or to re-register an) init
 * task for a given process or the OS.\n
 *   The order of registration doesn't matter for initialization tasks! The OS
 * initialization task is served first. Processes are always initialized in order of rising
 * process ID. The most privileged process is served last and can thus override descisions
 * of its less privileged predecessors.
 *   @remark
 * Never call this function after the call of rtos_initKernel()!
 *   @remark
 * This function must be called by trusted code in supervisor mode only.
 *   @todo
 * We should consider an enumeration of different error return codes. The function
 * implementation anyway makes the differentiation. Same for rtos_createEvent() and
 * rtos_initKernel().
 */
bool rtos_registerTask(const rtos_taskDesc_t *pTaskDescAPI, unsigned int idEvent)
{
    /* The scheduler should be in halted state. */
    if(_tiOsStep != 0)
        return (unsigned int)-1;

    /* The event need to be created before the task can be registered. */
    if(idEvent >= _noEvents  &&  idEvent != RTOS_EVENT_ID_INIT_TASK)
        return (unsigned int)-1;

    /* The process ID needs to be in the fixed and limited range. */
    if(pTaskDescAPI->PID > PRC_NO_PROCESSES)
        return (unsigned int)-1;

    /* The number of runtime tasks is constraint by compile time configuration. */
    if(_noTasks >= RTOS_MAX_NO_USER_TASKS  &&  idEvent != RTOS_EVENT_ID_INIT_TASK)
        return (unsigned int)-1;

    /* Task function not set. */
    if(pTaskDescAPI->userTaskFct == NULL)
        return (unsigned int)-1;

    /* Check execution budget: There's an upper boundary for user tasks and OS task cannot
       have deadline monitoring at all. */
    if(pTaskDescAPI->tiTaskMaxInUS > RTOS_TI_DEADLINE_MAX_IN_US
       ||  pTaskDescAPI->PID == 0  && pTaskDescAPI->tiTaskMaxInUS > 0
      )
    {
        return (unsigned int)-1;
    }

    /* We make the distinction between normal runtime tasks and initialization tasks. */
    if(idEvent == RTOS_EVENT_ID_INIT_TASK)
    {
        /* An initialization task must not be configured repeatedly for one and the same
           process. */
        const unsigned int idxP = pTaskDescAPI->PID;
        assert(idxP < sizeOfAry(_initTaskCfgAry));
        if(_initTaskCfgAry[idxP].taskFct != NULL)
            return (unsigned int)-1;

        /* Typecast: The assembler interface offers an extended function signature, which
           we cannot exploit in this context. */
        _initTaskCfgAry[idxP].taskFct = idxP == 0? (int32_t (*)(uint32_t, uint32_t))
                                                   pTaskDescAPI->osTaskFct
                                                 : (int32_t (*)(uint32_t, uint32_t))
                                                   pTaskDescAPI->userTaskFct;

        _initTaskCfgAry[idxP].tiTaskMax = pTaskDescAPI->tiTaskMaxInUS * 120;
        _initTaskCfgAry[idxP].PID = pTaskDescAPI->PID;
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
        for(idxTask=noTasksBefore; idxTask<_noTasks; ++idxTask)
            _taskCfgAry[idxTask+1] = _taskCfgAry[idxTask];

        /* Typecast: The assembler interface offers an extended function signature, which
           we cannot exploit in this context. */
        _taskCfgAry[noTasksBefore].taskFct = pTaskDescAPI->PID == 0
                                             ? (int32_t (*)(uint32_t, uint32_t))
                                               pTaskDescAPI->osTaskFct
                                             : (int32_t (*)(uint32_t, uint32_t))
                                               pTaskDescAPI->userTaskFct;
        _taskCfgAry[noTasksBefore].tiTaskMax = pTaskDescAPI->tiTaskMaxInUS * 120;
        _taskCfgAry[noTasksBefore].PID = pTaskDescAPI->PID;
        ++ _noTasks;

        /* Associate the task with the specified event. */
        if(_eventAry[idEvent].taskAry == NULL)
        {
            _eventAry[idEvent].taskAry = &_taskCfgAry[noTasksBefore];
            assert(_eventAry[idEvent].noTasks == 0);
        }
        ++ _eventAry[idEvent].noTasks;

        for(; idxEv<_noEvents; ++idxEv)
            if(_eventAry[idxEv].taskAry != NULL)
                ++ _eventAry[idxEv].taskAry;
    }

    return true;

} /* End of rtos_registerTask */



/**
 * Initialization and start of the RTOS kernel.\n
 *   The function initializes a hardware device to produce a regular clock tick and
 * connects the OS schedule function onOsTimerTick() with the interrupt raised by this
 * timer device. After return, the RTOS is running with a regular clock tick for scheduling
 * the tasks. Period time is #RTOS_CLOCK_TICK_IN_MS Milliseconds.\n
 *   The function can be called before or after the External Interrupts are enabled at the
 * CPU (see ihw_resumeAllInterrupts()). Normal behavior is however, no to resume the
 * interrupt processing before and let this be done by rtos_initKernel().
 *   @return
 * The function returns \a false is a configuration error is detected. The software must
 * not start up in this case. Normally, you will always receive a \a true. Since it is only
 * about static configuration, the returned error may be appropriately handled by an
 * assertion.
 *   @remark
 * The RTOS kernel applies the Periodic Interrupt Timer 0 (PIT0) as clock source. This
 * timer is reserved to the RTOS and must not be used at all by some other code.
 *   @param
 * All application tasks need to be registered before invoking this function, see
 * rtos_registerTask().
 */
bool rtos_initKernel(void)
{
    /* The user must have registered at minimum one task and must have associated it with a
       event. */
    bool isConfigOk = _noEvents > 0  &&  _noTasks > 0  &&  _tiOsStep == 0;

    /* Fill all process stacks with the empty-pattern, which is applied for computing the
       stack usage. */
    bool isProcessConfiguredAry[1+PRC_NO_PROCESSES] = {[0 ... PRC_NO_PROCESSES] = false};
    if(isConfigOk)
        isConfigOk = prc_initProcesses(isProcessConfiguredAry);

    unsigned int idxTask
               , maxPIDInUse = 0;
    /* Find the highest PID in use. */
    for(idxTask=0; idxTask<_noTasks; ++idxTask)
        if(_taskCfgAry[idxTask].PID > maxPIDInUse)
            maxPIDInUse = _taskCfgAry[idxTask].PID;

    /* A task must not belong to an invalid configured process. This holds for init and for
       run time tasks. */
    unsigned int idxP;
    if(isConfigOk)
    {
        for(idxTask=0; idxTask<_noTasks; ++idxTask)
        {
            assert(_taskCfgAry[idxTask].PID < sizeOfAry(isProcessConfiguredAry));
            if(!isProcessConfiguredAry[_taskCfgAry[idxTask].PID])
                isConfigOk = false;

        } /* For(All registered runtime tasks) */

        for(idxP=0; idxP<1+PRC_NO_PROCESSES; ++idxP)
        {
            /* Note, the init task array is - different to the runtime task array
               _taskCfgAry - ordered by PID. The field PID in the array entries are
               redundant. A runtime check is not appropriate as this had happened at
               registration time. We can place a simple assertion here. */
            if(_initTaskCfgAry[idxP].taskFct != NULL)
            {
                assert(_initTaskCfgAry[idxP].PID == idxP);
                if(!isProcessConfiguredAry[idxP])
                    isConfigOk = false;
            }
        } /* for(All possibly used processes) */
    }

    /* We could check if a process, an init task is registered for, has a least one runtime
       task. However, it is not harmful if not and there might be pathologic applications,
       which solely consist of I/O driver callbacks. */

    if(isConfigOk)
    {
        unsigned int idxEv;
        for(idxEv=0; idxEv<_noEvents; ++idxEv)
        {
            /* Check task configuration: Events without an associated task are useless and
               point to a configuration error. */
            if(_eventAry[idxEv].noTasks == 0)
                isConfigOk = false;

            /* If an event has the highest possible priority #RTOS_KERNEL_PRIORITY-1 then
               only those tasks can be associated, which belong to the process with highest
               PID in use or OS tasks. This is a safety constraint. */
            if(_eventAry[idxEv].priority == RTOS_KERNEL_PRIORITY-1)
            {
                for(idxTask=0; idxTask<_eventAry[idxEv].noTasks; ++idxTask)
                {
                    const unsigned int PID = _eventAry[idxEv].taskAry[idxTask].PID;
                    if(PID > 0  &&  PID != maxPIDInUse)
                        isConfigOk = false;
                }
            } /* End if(Unblockable priority is in use by event) */
        } /* for(All registered events) */
    }

    /* After checking the static configuration, we can enable the dynamic processes.
       Outline:
       - Disable all processes (which is their initial state). Once we enable the
         interrupts, the I/O drivers start working and they may invoke callbacks into the
         processes. The execution of these callbacks will be inhibited
       - Disable the scheduler to trigger any events (which is its initial state).
         Triggering events would not cause any user tasks to execute (processes still
         disabled) but their due counters would already run and the configured startup
         conditions would not be met later when enabling the processes
       - Globally enable interrupt processing. I/O drivers and OS clock tick are running.
         This is a prerequisite for the deadline monitoring, which we want to have in place
         already for the init tasks
       - Sequentially execute all configured process initialization tasks. There are no
         crosswise race condition and nor with user tasks or I/O driver callbacks
       - Enable the processes and release the scheduler; scheduler, user tasks and I/O
         driver's callbacks start running */

    /* Stop the scheduler. It won't run although the RTOS clock starts spinning. We don't
       want to see a running user task during execution of the init tasks. */
    _tiOs = (unsigned long)-1;
    _tiOsStep = 0;

    /* We can register all SW interrupt service routines. */
    if(isConfigOk)
    {
        /* Install all software interrupts, which implement the events. */
        #define INSTALL_SW_IRQ(idEv)                                            \
            if(idEv < _noEvents)                                                \
            {                                                                   \
                prc_installINTCInterruptHandler( &swInt##idEv                   \
                                               , /* vectorNum */ idEv           \
                                               , _eventAry[idEv].priority       \
                                               , /* isPreemptable */ true       \
                                               );                               \
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

        /* Install the interrupt servive routine for cyclic timer PIT 0. It drives the OS
           scheduler for cyclic task activation. */
        prc_installINTCInterruptHandler( &onOsTimerTick
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
           RTOS. */
        PIT.PITMCR.R = 0x1;
    }

    /* All processes are initialized by prc_initProcesses() in stopped state: We don't want
       to see a callback from an I/O driver after resuming interrupt processing and while
       an init task is executed. Note, it doesn't really matter if interrupt processing was
       resumed already before. */
    if(isConfigOk)
        ihw_resumeAllInterrupts();

    /* When we get here (and if we didn't see a configuration error) then all kernel
       interrupts are configured, interrupts occur and are processed, but no user tasks are
       activated nor will an I/O driver run a callback. We can safely start our process
       initialization tasks. */

    /* Run all process initialization in order of increasing PID. A process with higher
       privileges is initialized after another with less privileges. The higher
       privileged could override settings made by its predecessor. */
    for(idxP=0; isConfigOk && idxP<1+PRC_NO_PROCESSES; ++idxP)
    {
        /* The specification of an initialization task is an option only. Check for NULL
           pointer. */
        if(_initTaskCfgAry[idxP].taskFct != NULL)
        {
            if(isProcessConfiguredAry[idxP])
            {
                /* Everything is alright. Run the initialization task. A negative return
                   value is defined to be an error. (This needs to be considered by the
                   implementation of the task.) */
                int32_t resultInit;
                if(_initTaskCfgAry[idxP].PID == 0)
                {
                    /* OS initailization function: It is a normal sub-function code; we are
                       here in the OS context. */
                    resultInit = ((int32_t (*)(void))_initTaskCfgAry[idxP].taskFct)();
                }
                else
                    resultInit = ivr_runInitTask(&_initTaskCfgAry[idxP]);

                if(resultInit < 0)
                    isConfigOk = false;
            }
            else
            {
                /* An initialization task must not be registered for a process, which is not
                   configurated. */
                /// @todo Didn't we check this above? Use assert instead
                isConfigOk = false;
            }
        } /* End if(Init task configured for process?) */
    } /* End for(All possible processes) */

    /* After successfully completing all the initialization tasks, we can release the
       scheduler and the processes. We do this in a critical section in order to not
       endanger the specified relationship of initial task activations (specified in terms
       of task priority, period time and initial due time). */
    if(isConfigOk)
    {
        ihw_suspendAllInterrupts();

        /* Process state: Set to running (not zero) only if configuration generally okay. */
        unsigned int idxP;
        for(idxP=1; idxP<1+PRC_NO_PROCESSES; ++idxP)
            if(isProcessConfiguredAry[idxP])
                prc_processAry[idxP-1].state = 1;
        
        /* Release scheduler. */
        _tiOsStep = RTOS_CLOCK_TICK_IN_MS;

        ihw_resumeAllInterrupts();
    }

    /* @todo Shall we offer idle tasks per process? If so, we cannot leave the routine but
       would need to enter an infinite loop - and had to offer such a function for OS, too.
       For consistency reason this would require an init function for OS, too. */

    return isConfigOk;

} /* End of rtos_initKernel */




/**
 * Trigger an event to activate all associated tasks. A event, which had been registered
 * with cycle time zero is normally not executed. It needs to be triggered with this
 * function in order to make its associated tasks run once, i.e. to make its task functions
 * exceuted once as result of this call.\n
 *   This function can be called from any task or ISR. However, if the calling task belongs
 * to the set of tasks associated with \a idEvent, then it'll have no effect but an
 * accounted activation loss; an event can be re-triggered only after all associated
 * activations have been completed. There is no activation queuing. The function returns \a
 * false in this case.\n
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
 * would be invoked once addtionally. Note, that an event activation loss is not unlikely in
 * this case; the cyclic task may currently be busy.
 *   @remark
 * It is not forbidden but useless to let a task activate itself by triggering the event it
 * is associated with. This will have no effect besides incrementing the activation loss
 * counter for that event.
 *   @remark
 * This function must be called from the OS context only. It may be called from an ISR to
 * implement delegation to a user task.
 */
bool rtos_OS_triggerEvent(unsigned int idEvent)
{
    /* The events are related to the eight available software interrupts. Each SWIRQ is
       controlled by two bits in one out of two status registers of the INTC (RM, 28, p.
       911ff). Access to INTC_SSCIR is described at 28.4.2 and 28.4.7. */
    bool intFlagNotYetSet = true;
    vuint8_t * const pINTC_SSCIR = (vuint8_t*)&INTC.SSCIR0_3.R + idEvent;

    /* To make this function reentrant with respect to one and the same target SW IRQ, we
       need to encapsulate the flag-test-and-test operation in a critical section.
         As a side effect, we can use the same critical section for race condition free
       increment of the error counter in case. */
    const uint32_t msr = ihw_enterCriticalSection();
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
    ihw_leaveCriticalSection(msr);

    return intFlagNotYetSet;

} /* End of rtos_OS_triggerEvent */



/**
 * System call handler implementation to trigger an event (and to activate the associated
 * tasks). Find all details in rtos_OS_triggerEvent().
 *   @return
 * \a true if activation was possible, \a false otherwise.
 *   @param pidOfCallingTask
 * Process ID of calling user task. The operation is permitted only for tasks belonging to
 * those processes, which
 * have an ID that is greater of equal to the minimum specified for the event in question.
 * Otherwise an exception is raised, which aborts the calling task.
 *   @param idEvent
 * The ID of the event to trigger. This is the ID got from rtos_createEvent().
 *   @remark
 * This function must never be called directly. The function is only made for placing it in
 * the global system call table.
 */
uint32_t rtos_scFlHdlr_triggerEvent(unsigned int pidOfCallingTask, unsigned int idEvent)
{
    if(idEvent < _noEvents  &&  pidOfCallingTask >= _eventAry[idEvent].minPIDForTrigger)
        return (uint32_t)rtos_OS_triggerEvent(idEvent);
    else
    {
        /* The user specified task ID is not in range. This is a severe user code error,
           which is handled with an exception, task abort and counted error.
             Note, this function does not return. */
        ivr_systemCallBadArgument();
    }
} /* End of rtos_scFlHdlr_triggerEvent */




/**
 * System call handler implementation to create and run a task in another process. Find
 * more details in rtos_OS_runTask().\n
 *   Start a user task. A user task is a C function, which is executed in user mode and in
 * a given process context. The call is synchronous; the calling user context is
 * immediately preempted and superseded by the started task. The calling user context is
 * resumed when the task function ends - be it gracefully or by exception/abortion.\n
 *   The started task inherits the priority of the calling user task. It can be preempted
 * only by contexts of higher priority.\n
 *   The function requires sufficient privileges. The invoking task needs to belong to a
 * process with an ID greater then the target process. The task cannot be started in the OS
 * context.\n
 *   The function cannot be used recursively. The created task cannot in turn make use of
 * rtos_runTask().
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
 * This function must never be called directly. The function is only made for placing it in
 * the global system call table.
 */
uint32_t rtos_scFlHdlr_runTask( unsigned int pidOfCallingTask
                              , const prc_userTaskConfig_t *pUserTaskConfig
                              , uintptr_t taskParam
                              )
{
    if(sc_checkUserCodeReadPtr(pUserTaskConfig, sizeof(prc_userTaskConfig_t))
       &&  pidOfCallingTask > pUserTaskConfig->PID
      )
    {
        /* We forbid recursive use of this system call not because it would be is
           technically not possible but to avoid an overflow of the supervisor stack. Each
           creation of a user task puts a stack frame on the SV stack. We cannnot detect a
           recursion but we can hinder the SV stack overflow by making the current
           context's priority a gate for further use of this function: The next invokation
           needs to appear at higher level. This will limit the number of stack frames
           similar as this is generally the case for interrupts.
             Note, a user task can circumvent the no-recursion rule by abusing the priority
           ceiling protocol to increment the level by one in each recursion. This is
           technically alright and doesn't impose a risk. The number of available PCP
           levels is strictly limited and so is then the number of possible recursions. The
           SV stack is protected. */
        static uint32_t minPriorityLevel_ SECTION(.sdata.OS.minPriorityLevel_) = 0;
        
        uint32_t currentLevel = INTC.CPR_PRC0.R
               , minPriorityLevelOnEntry;

        ihw_suspendAllInterrupts();
        const bool isEnabled = currentLevel >= minPriorityLevel_;
        if(isEnabled)
        {
            minPriorityLevelOnEntry = minPriorityLevel_;
            minPriorityLevel_ = currentLevel+1;
        }
        ihw_resumeAllInterrupts();
        
        if(isEnabled)
        {
            /* All preconitions fulfilled, lock is set, run the task. */
            const int32_t taskResult = ivr_runUserTask(pUserTaskConfig, taskParam);
            
            /* Restore the pre-requisite for future use of this system call. */
            ihw_suspendAllInterrupts();
            
            /* The warning "'minPriorityLevelOnEntry' may be used uninitialized" is locally
               switched off. Justification: Variable is only used if(isEnabled) and then it
               is initialized, too. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
            minPriorityLevel_ = minPriorityLevelOnEntry;
#pragma GCC diagnostic pop

            ihw_resumeAllInterrupts();
            
            return (uint32_t)taskResult;
        }            
        else
        {
            /* Bad use of function, penalty is task abortion.
                 Note, this function does not return. */
            ivr_systemCallBadArgument();
        }
    }
    else
    {
        /* The user doesn't have the privileges to run the aimed task. This is a severe
           user code error, which is handled with an exception, task abort and counted
           error.
             Note, this function does not return. */
        ivr_systemCallBadArgument();
    }
} /* End of rtos_scFlHdlr_runTask */




/**
 * An event, which becomes due may not be able to activate all its associated tasks because
 * they didn't terminate yet after their previous activation. It doesn't matter if this
 * happens because a cyclic task becomes due or because an event task has been triggered by
 * software (using rtos_triggerEvent()). The scheduler counts the failing activations on a
 * per event base. The current value can be queried with this function.
 *   @return
 * Get the current number of failed task activations since start of the RTOS scheduler. The
 * counter is saturated and will not wrap around.\n
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
    if(PID <= PRC_NO_PROCESSES)
    {
        /* The stack area is defined by the linker script. We can access the information by
           declaring the linker defined symbols. */
        extern uint32_t ld_stackStartOS[], ld_stackStartP1[], ld_stackStartP2[]
                      , ld_stackStartP3[], ld_stackStartP4[]
                      , ld_stackEndOS[], ld_stackEndP1[], ld_stackEndP2[]
                      , ld_stackEndP3[], ld_stackEndP4[];
        static const uint32_t const *stackStartAry_[PRC_NO_PROCESSES+1] =
                                                        { [0] = ld_stackStartOS
                                                        , [1] = ld_stackStartP1
                                                        , [2] = ld_stackStartP2
                                                        , [3] = ld_stackStartP3
                                                        , [4] = ld_stackStartP4
                                                        }
                                  , *stackEndAry_[PRC_NO_PROCESSES+1] =
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



