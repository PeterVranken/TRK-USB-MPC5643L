/**
 * @file rtos.c
 *   Implementation of a Real Time Operating System for the Arduino Mega board in the
 * Arduino environment 1.0.6.\n
 *   The implementation is dependent on the board (the controller) and the GNU C++ compiler
 * (thus the release of the Arduino environment) but should be easily portable to other
 * boards and Arduino releases. See manual for details.\n
 *   This is a port for the NXP PowerPC core e200z4. The dependencies on the cpompiler has
 * been significantly lowered by separating all machine code from the C code. This file
 * implements the scheduler in widely machine independent C code. The differences to the
 * original Arduino implementation are little.
 *
 * Copyright (C) 2012-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   rtos_initializeTask
 *   rtos_initRTOS
 *   setupAfterKernelInit (default implementation of callback)
 *   rtos_enableIRQUser00 (callback without default implementation)
 *   rtos_enableIRQUser01 (callback without default implementation)
 *   rtos_sc_sendEvent (system call, invoked through macro #rtos_sendEvent)
 *   setupAfterSystemTimerInit (default implementation of callback)
 *   loop (default implementation of callback)
 *   isrUser00
 *   isrUser01
 *   rtos_sc_waitForEvent (system call, invoked through macro #rtos_waitForEvent)
 *   rtos_getTaskOverrunCounter
 *   rtos_getStackReserve
 * Local functions
 *   isrSystemTimerTick
 *   enableIRQTimerTick
 *   prepareTaskStack
 *   checkTaskForActivation
 *   lookForActiveTask
 *   onTimerTick
 *   sendEvent
 *   acquireFreeSyncObjs
 *   storeResumeCondition
 *   waitForEvent
 */


/*
 * Include files
 */

#ifdef DEBUG
# include <stdio.h>
#endif
#include <string.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "int_defStackFrame.h"
#include "int_interruptHandler.h"
#include "sc_systemCalls.h"
#include "del_delay.h"
#include "ccx_createContextSaveDesc.h"
#include "rtos_systemCalls.h"
#include "rtos.h"


/*
 * Defines
 */

/** The ID of the idle task. The ID of a task is identical with the index into the task
    array. */
#define IDLE_TASK_ID    (RTOS_NO_TASKS)

/** A bit mask, which selects all the mutex events in an event vector. */
#define MASK_EVT_IS_SEMAPHORE (((uint32_t)1<<(RTOS_NO_SEMAPHORE_EVENTS))-1u)

/** A bit mask, which selects all the mutex events in an event vector. */
#define MASK_EVT_IS_MUTEX                                                       \
        ((((uint32_t)1<<(RTOS_NO_MUTEX_EVENTS+RTOS_NO_SEMAPHORE_EVENTS))-1u)    \
         - MASK_EVT_IS_SEMAPHORE                                                \
        )

/** A bit mask, which selects all timer events in a vector of events. */
#define MASK_EVT_IS_TIMER (RTOS_EVT_ABSOLUTE_TIMER | RTOS_EVT_DELAY_TIMER)

/** A pattern byte, which is used as prefill byte of any task stack area. A simple and
    inexpensive stack usage check at runtime can be implemented by looking for up to where
    this pattern has been destroyed. Any value which is not the null and which is
    improbable to be a true stack contents byte can be used - whatever this value might
    be.\n
      Note, you must not change this definition. The pattern needs to be identical to the
    one that is used by the startup code for initialization of the main stack, which is
    inherited by the idle task. */
#define UNUSED_STACK_PATTERN 0xa5a5a5a5ul

/** For stability testing only, we can realize the kernel clock by three cyclic timers,
    which yield in sum the same (average) clock rate but which have mutual prime cycle
    times so that all possible phase shifts are seen, in particular critical short
    distances and coincidence.\n
      It depends on the application code if this test mode can be safely enabled. It must
    never be used besides testing.\n
      If the test mode is enabled then the kernel requires PIT1 and PIT2 as additional
    clock sources (normally only PIT3). */
#define TEST_USE_IRREGULAR_SYS_CLOCK    0


/*
 * Local type definitions
 */

/** The descriptor of any task. Contains static information like task priority class and
    dynamic information like received events, timer values etc. This type is invisible to
    the RTuinOS application code.
      @remark Some members of the struct are not used by the kernel at runtime, e.g. the
    pointer to the task function or the definition of the stack area. These elements or
    parts of them could be placed into a second array such that the remaining data
    structure had a size which is a power of 2. (Maybe with a few padding bytes.) This
    would speed-up - and maybe significantly - the address computations the kernel often has
    to do when accessing the task information. */
typedef struct
{
    /** The execution context information (mainly the stack pointer) of this task whenever
        it is not active. */
    int_contextSaveDesc_t contextSaveDesc;

    /** The priority class this task belongs to. Priority class 255 has the highest
        possible priority and the lower the value the lower the priority. */
    unsigned int prioClass;

    /** The task function as a function pointer. It is used once and only once: The task
        function is invoked the first time the task becomes active and must never end. A
        return statement would cause an immediate reset of the controller. */
    rtos_taskFunction_t taskFunction;

    /** The timer value triggering the task local absolute-timer event. */
    unsigned int timeDueAt;

#if RTOS_ROUND_ROBIN_MODE_SUPPORTED == RTOS_FEATURE_ON
    /** The maximum time a task may be activated if it is operated in round-robin mode. The
        range is 1..max_value(unsigned int).\n
          Specify a maximum time of 0 to switch round robin mode off for this task.
          @remark Round robin like behavior is given only if there are several tasks in the
        same priority class and all tasks of this class have the round-robin mode
        activated. Otherwise it's just the limitation of execution time for an individual
        task. */
    unsigned int timeRoundRobin;
#endif

    /** The pointer to the preallocated stack area of the task. The area needs to be
        available all the RTOS runtime long. Therefore dynamic allocation won't pay off.
        Consider to use the address of any statically defined array. */
    uint32_t *pStackArea;

    /** The size in Byte of the memory area \a *pStackArea, which is reserved as stack for
        the task. Each task may have an individual stack size. */
    unsigned int stackSize;

    /** The timer tick decremented counter triggering the task local delay-timer event.\n
          The initial value determines at which system timer tick the task becomes due the
        very first time. This may always by 1 (task becomes due immediately). In the use
        case of regular tasks it may however pay off to distribute the tasks on the time
        grid in order to avoid too many due tasks regularly at specific points in time. See
        documentation for more. */
    unsigned int cntDelay;

#if RTOS_ROUND_ROBIN_MODE_SUPPORTED == RTOS_FEATURE_ON
    /** The timer tick decremented counter triggering a task switch in round-robin mode. */
    unsigned int cntRoundRobin;
#endif

    /** The events posted to this task. */
    uint32_t postedEventVec;

    /** The mask of events which will make this task due. */
    uint32_t eventMask;

    /** Do we need to wait for the first posted event or for all events? */
    bool waitForAnyEvent;

    /** All recognized overruns of the timing of this task are recorded in this variable.
          Task overruns are defined only in the (typical) use case of regular real time
        tasks. In all other applications of a task this value is useless and undefined.\n
           @remark The implementation requires atomic load and store access to this
        variable. */
    unsigned int cntOverrun;

} task_t;



/*
 * Local prototypes
 */

#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON  ||  RTOS_USE_MUTEX == RTOS_FEATURE_ON
static bool waitForEvent( uint32_t eventMask
                        , bool all
                        , unsigned int timeout
                        );
#else
static void waitForEvent(uint32_t eventMask, bool all, unsigned int timeout);
#endif


/*
 * Data definitions
 */

#if RTOS_NO_TASKS > RTOS_NO_PRIO_CLASSES*RTOS_MAX_NO_TASKS_IN_PRIO_CLASS
# error Bad configuration of number of tasks and priority classes
#endif

/** The RTuinOS startup message. All applications, which make use of the serial connection
    with the host computer should print this message at startup. */
const char rtos_rtuinosStartupMsg[] = RTOS_EOL RTOS_RTUINOS_STARTUP_MSG;

/** The system time. A cyclic counter of the timer ticks. The counter is interrupt driven.
    The unit of the time is defined only by the it triggering source and doesn't matter at
    all for the kernel. The time even don't need to be regular.\n
      The initial value is such that the time is 0 during the execution of the very first
    system timer interrupt service. This is important for getting a task startup behavior,
    which is transparent and predictable for the application. */
static unsigned int _time = (unsigned int)-1;

/** Array of all the task objects. The array has one additional element to store the
    information about the implicitly defined idle task. (Although most fields of the task
    object are irrelevant for the idle task. Here is potential to save memory space.)\n
      The array is partly initialized by the application repeatedly calling \a
    rtos_initializeTask. The rest of the initialization and the initialization of the idle
    task element is done in rtos_initRTOS(). */
static task_t _taskAry[RTOS_NO_TASKS+1];

/** Pointer to the idle task object. */
static task_t * const _pIdleTask = &_taskAry[IDLE_TASK_ID];

/** The one and only active task. This may be the only internally seen idle task which does
    nothing. Implemented as pointer to the task object. */
static task_t *_pActiveTask = &_taskAry[IDLE_TASK_ID];

/** The task which is to be suspended because of a newly activated one. Only temporarily
    used in the instance of a task switch. */
static task_t *_pSuspendedTask = NULL;

/** Array holding all due (but not active) tasks. Ordered according to their priority
    class. */
static task_t *_pDueTaskAryAry[RTOS_NO_PRIO_CLASSES][RTOS_MAX_NO_TASKS_IN_PRIO_CLASS];

/** Number of due tasks in the different priority classes. */
static unsigned int _noDueTasksAry[RTOS_NO_PRIO_CLASSES];

/** Array holding all currently suspended tasks. */
static task_t *_pSuspendedTaskAry[RTOS_NO_TASKS];

/** Number of currently suspended tasks. */
static unsigned int _noSuspendedTasks;

#if RTOS_USE_MUTEX == RTOS_FEATURE_ON
/** All of the mutex events are combined in a bit vector. The mutexes are initially
    released, all according bits are set. All remaining bits are don't care bits. */
static uint32_t _mutexVec = MASK_EVT_IS_MUTEX;
#endif


/*
 * Function implementation
 */


/**
 * The still unused stack area of a new task is filled with a specific pattern, which will
 * permit to run a (a bit guessing) stack usage routine later on: We can look up to where
 * the pattern has been destroyed.
 *   @param pEmptyTaskStack
 * A pointer to a RAM area which is reserved for the stack data of the new task. The EABI
 * requires a 64 Bit alignment of stack pointer. The easiest way to get such an area is to
 * define a uint64_t array on module scope. Or use the type decoration _Alignas(uint64_t)
 * otherwise.
 *   @param stackSize
 * The number of bytes of the stack area. Should be a multiple of eight - all stack frames'
 * sizes are a multiple of eight Byte.
 *   @remark
 * Note, the alignment and size constraints are double-checked by assertion.
 */
static void prepareTaskStack(uint32_t * const pEmptyTaskStack, unsigned int stackSize)
{
    /* We need a minimum of stack size to ensure context switches. The alignment matters. */
    assert(stackSize >= 200  &&  (stackSize & 0x7) == 0
           &&  ((uint32_t)pEmptyTaskStack & 0x7) == 0
          );

    uint32_t *sp = pEmptyTaskStack + stackSize/sizeof(uint32_t);

    /* We fill the stack area with a specific pattern, which will permit to run a (a bit
       guessing) stack usage routine later on: We can look up to where the pattern has been
       destroyed. */
    while(--sp >= (uint32_t*)pEmptyTaskStack)
        *sp = UNUSED_STACK_PATTERN;

} /* End of prepareTaskStack. */





/**
 * When an event has been posted to a currently suspended task, it might easily be that
 * this task is resumed and becomes due. This routine checks a suspended task for resume
 * and moves it into the due task lists if it is resumed.
 *   @return
 * The Boolean information whether the task is resumed and becomes due is returned.
 *   @param idxSuspTask
 * The index of the investigated task. It is the index in the global list
 * \a _pSuspendedTaskAry of suspended tasks.
 */
static inline bool checkTaskForActivation(unsigned int idxSuspTask)
{
    task_t * const pT = _pSuspendedTaskAry[idxSuspTask];
    uint32_t eventVec;
    bool taskBecomesDue;

    /* Check if the task becomes due because of the events posted prior to calling this
       function. The optimally supported case is the more probable OR combination of
       events.
         The AND operation is less straight forward as the timeout character of the
       timer events needs to be retained: AND only refers to the postable events but
       does not include the timer events. All postable events need to be set in both
       the mask and the vector of posted events OR any of the timer events in the mask
       are set in the vector of posted events. */
    eventVec = pT->postedEventVec;
    if((pT->waitForAnyEvent &&  eventVec != 0)
       ||  (!pT->waitForAnyEvent
            &&  (((eventVec ^ pT->eventMask) & ~MASK_EVT_IS_TIMER) == 0
                 ||  (eventVec & pT->eventMask & MASK_EVT_IS_TIMER) != 0
                )
           )
      )
    {
        unsigned int u
                   , prio = pT->prioClass;

        /* This task becomes due. */

#if RTOS_ROUND_ROBIN_MODE_SUPPORTED == RTOS_FEATURE_ON
        /* If a round robin task voluntarily suspends it gets the right for a complete
           new time slice. Reload the counter. */
        pT->cntRoundRobin = pT->timeRoundRobin;
#endif
        /* Move the task from the list of suspended tasks to the list of due tasks of
           its priority class. */
        _pDueTaskAryAry[prio][_noDueTasksAry[prio]++] = pT;
        -- _noSuspendedTasks;
        for(u=idxSuspTask; u<_noSuspendedTasks; ++u)
            _pSuspendedTaskAry[u] = _pSuspendedTaskAry[u+1];

        /* Since a task became due there might be a change of the active task. */
        taskBecomesDue = true;
    }
    else
        taskBecomesDue = false;

    return taskBecomesDue;

} /* End of checkTaskForActivation */




/**
 * After posting an event to one or more currently suspended tasks, it might easily be that
 * one such task is resumed and ready and becomes active because of its higher priority. To
 * find out, this function needs to be called after posting the events. It determines which
 * task is now the active task.
 *   @return
 * The Boolean information wheather the active task now is another task is returned.\n
 *   If the function returns true, references to the old and the new active task are
 * reported by side effect: They are written into the global variables \a _pSuspendedTask and
 * \a _pActiveTask.
 */
static inline bool lookForActiveTask()
{
    /* The calling interrupt service routine will do a context switch only if we return
       true. Otherwise it'll simply do a "reti" to the interrupted context and continue
       it. */

    signed int idxPrio;

    /* Look for the task we will return to. It's the first entry in the highest
       non-empty priority class. The loop requires a signed index. */
    for(idxPrio=RTOS_NO_PRIO_CLASSES-1; idxPrio>=0; --idxPrio)
    {
        if(_noDueTasksAry[idxPrio] > 0)
        {
            _pSuspendedTask = _pActiveTask;
            _pActiveTask    = _pDueTaskAryAry[idxPrio][0];

            /* If we only entered the outermost if clause we made at least one task
               due; these statements are thus surely reached. As the due becoming task
               might however be of lower priority it can easily be that we nonetheless
               don't have a task switch. */
            return _pActiveTask != _pSuspendedTask;
        }
    }

    /* We never get here. This function is called under the precondition that a task was
       put into a due list, so the search above will surely have found one. */
    assert(false);
    return false;

} /* End of lookForActiveTask */




/**
 * This function is called from the system interrupt triggered by the main clock. The
 * timers of all suspended tasks are served and - in case they elapse - timer events are
 * generated. These events may then resume some of the tasks. If so, they are placed in
 * the appropriate list of due tasks. Finally, the longest due task in the highest none
 * empty priority class is activated.
 *   @return
 * The Boolean information is returned whether we have or not have a task switch. In most
 * invocations we won't have and therefore it's worth to optimize the code for this case:
 * Don't do the expensive switch of the stack pointers.\n
 *   The most important result of the function, the reference to the active task after
 * leaving the function, is returned by side effect: The global variable _pActiveTask is
 * updated.
 */
static bool onTimerTick(void)
{
    /* Clock the system time. Cyclic overrun is intended. */
    ++ _time;

    bool activeTaskMayChange = false;

    /* Check for all suspended tasks if a timer event has to be posted. */
    unsigned int idxSuspTask = 0;
    while(idxSuspTask<_noSuspendedTasks)
    {
        task_t * const pT = _pSuspendedTaskAry[idxSuspTask];

        /* Remember the received events before (possibly) getting some more in this
           timer tick: Only if this set changes it is necessary to check for a state
           transition of the task. */
        const uint32_t postedEventVecBefore = pT->postedEventVec;

        /* Check for absolute timer event. */
        if(_time == pT->timeDueAt)
        {
            /* Setting the absolute timer event when it already is set looks like a task
               overrun indication. It isn't for the following reason. The absolute timer
               event can't be AND combined with other events, so the event will immediately
               change the status to due (see checkForTaskActivation), so that setting it a
               second time will never occur. */
            pT->postedEventVec |= (RTOS_EVT_ABSOLUTE_TIMER & pT->eventMask);
        }

        /* Check for delay timer event. The code here should optimally support the standard
           situation that the counter is constantly 0. */
        if(pT->cntDelay != 0)
        {
            if(-- pT->cntDelay == 0)
                pT->postedEventVec |= (RTOS_EVT_DELAY_TIMER & pT->eventMask);
        }

        /* Check if this suspended task becomes due because of a timer event, which was
           posted to it. */
        if(postedEventVecBefore != pT->postedEventVec  && checkTaskForActivation(idxSuspTask))
        {
            /* The task becomes due, which may cause a task switch. */
            activeTaskMayChange = true;

            /* Check next suspended task, which is in this case found in the array
               element with same index: The task which became due has been removed into the
               due list of its priority class. */
        }
        else
        {
            /* The task remains suspended. */

            /* Check next suspended task, which is in this case found in the next array
               element. */
            ++ idxSuspTask;

        } /* End if(Did this suspended task become due?) */

    } /* End while(All suspended tasks) */


#if RTOS_ROUND_ROBIN_MODE_SUPPORTED == RTOS_FEATURE_ON
    /* Round-robin: Applies only to the active task. It can become inactive, however not
       undue, or suspended respectively. If its time slice is elapsed it is put at the end
       of the due list in its priority class. */
    if(_pActiveTask->cntRoundRobin != 0)
    {
        if(--_pActiveTask->cntRoundRobin == 0)
        {
            /* Time slice of active task has elapsed. Reload the counter. */
            _pActiveTask->cntRoundRobin = _pActiveTask->timeRoundRobin;

            unsigned int prio = _pActiveTask->prioClass
                       , noTasks = _noDueTasksAry[prio];

            /* If there are more due tasks in the same class the next one will be made the
               active one by a cyclic move of the positions in the list. */
            if(noTasks-- > 1)
            {
                unsigned int idxTask;

                /* The list of due tasks in the active priority class is rolled by one task.
                   The next due task will become active.
                     Note: noTasks has been decremented before in the if. */
/// @todo Try using void *__memcpy(void *, const void *, size_t);
                for(idxTask=0; idxTask<noTasks; ++idxTask)
                    _pDueTaskAryAry[prio][idxTask] = _pDueTaskAryAry[prio][idxTask+1];
                _pDueTaskAryAry[prio][idxTask] = _pActiveTask;

                /* Force check for new active task - even if no suspended task should have been
                   resumed. */
                activeTaskMayChange = true;

            } /* if(Did the task loose against another due one?) */

        } /* if(Did the round robin time of the task elapse?) */

    } /* if(Do we have a round robin task?) */
#endif

    /* Check if another task becomes active because of the possibly occurred timer events.
         activeTaskMayChange: We do the search for the new active task only if at least one
       suspended task was resumed. If round robin is compiled it depends. Always look for a
       new active task if the round robin time has elapsed.
         The function has side effects: If there's a task which was suspended before and
       which is resumed because of the timer events and which is of higher priority than
       the one being active so far then the references to the old and newly active task are
       written into global variables _pSuspendedTask and _pActiveTask. */
    return activeTaskMayChange && lookForActiveTask();

} /* End of onTimerTick. */






/**
 * Each call of this function cyclically increments the system time of the kernel by one.\n
 *   Incrementing the system timer is an important system event. The routine will always
 * include an inspection of all suspended tasks, whether they could become due again.\n
 *   The unit of the time is defined only by the it triggering source and doesn't matter at
 * all for the kernel. The time even don't need to be regular.\n
 *   @remark
 * The function needs to be called by an interrupt and can easily end with a context change,
 * i.e. the interrupt will return to another task as the one it had interrupted.
 *   @remark
 * Servicing of this interrupt needs to be disabled/enabled by the implementation of
 * rtos_enterCriticalSection() and rtos_leaveCriticalSection().
 *   @see bool onTimerTick(void)
 */
#if TEST_USE_IRREGULAR_SYS_CLOCK != 1
static uint32_t isrSystemTimerTick(int_cmdContextSwitch_t *pCmdContextSwitch)
{
    /// @todo Try using eTimer_2, TC5IR, on level #227: We should use the lowest possible
    // interrupt prio for the RTOS tick in order to give any other real time event
    // precedence (Offer both by #define)

    /* Acknowledge the timer interrupt in the causing HW device. */
    assert(PIT.TFLG3.B.TIF == 0x1);
    PIT.TFLG3.B.TIF = 0x1;

    /* Check for all suspended tasks if this change in time is an event for them. */
    if(onTimerTick())
    {
        /* Yes, another task becomes active with this timer tick. Command the calling
           system call handler on return to switch to the other task. The vector of
           received events is the value returned to the activated task. */
        pCmdContextSwitch->signalToResumedContext = _pActiveTask->postedEventVec;
        _pActiveTask->postedEventVec = 0;
        pCmdContextSwitch->pSuspendedContextSaveDesc = &_pSuspendedTask->contextSaveDesc;
        pCmdContextSwitch->pResumedContextSaveDesc = &_pActiveTask->contextSaveDesc;

        return int_rcIsr_switchContext;
    }
    else
    {
        /* No task switch is wanted in this tick. Continue the same, preempted context. */
        return int_rcIsr_doNotSwitchContext;
    }
} /* End of ISR to increment the system time by one tick. */

#else /* if TEST_USE_IRREGULAR_SYS_CLOCK == 1 */

static uint32_t onSystemTimerTick(int_cmdContextSwitch_t *pCmdContextSwitch)
{
    /* Check for all suspended tasks if this change in time is an event for them. */
    if(onTimerTick())
    {
        /* Yes, another task becomes active with this timer tick. Command the calling
           system call handler on return to switch to the other task. The vector of
           received events is the value returned to the activated task. */
        pCmdContextSwitch->signalToResumedContext = _pActiveTask->postedEventVec;
        _pActiveTask->postedEventVec = 0;
        pCmdContextSwitch->pSuspendedContextSaveDesc = &_pSuspendedTask->contextSaveDesc;
        pCmdContextSwitch->pResumedContextSaveDesc = &_pActiveTask->contextSaveDesc;

        return int_rcIsr_switchContext;
    }
    else
    {
        /* No task switch is wanted in this tick. Continue the same, preempted context. */
        return int_rcIsr_doNotSwitchContext;
    }
} /* End of onSystemTimerTick. */

static uint32_t isrSystemTimerTick(int_cmdContextSwitch_t *pCmdContextSwitch)
{
    /* Acknowledge the timer interrupt in the causing HW device. */
    assert(PIT.TFLG3.B.TIF == 0x1);
    PIT.TFLG3.B.TIF = 0x1;
    return onSystemTimerTick(pCmdContextSwitch);
}
static uint32_t isrSystemTimerTick_testPid1(int_cmdContextSwitch_t *pCmdContextSwitch)
{
    /* Acknowledge the timer interrupt in the causing HW device. */
    assert(PIT.TFLG1.B.TIF == 0x1);
    PIT.TFLG1.B.TIF = 0x1;
    return onSystemTimerTick(pCmdContextSwitch);
}
static uint32_t isrSystemTimerTick_testPid2(int_cmdContextSwitch_t *pCmdContextSwitch)
{
    /* Acknowledge the timer interrupt in the causing HW device. */
    assert(PIT.TFLG2.B.TIF == 0x1);
    PIT.TFLG2.B.TIF = 0x1;
    return onSystemTimerTick(pCmdContextSwitch);
}
#endif



/**
 * Start the interrupt which clocks the system time. We use PIT 3 and let it generate a
 * regular clock tick of period #RTOS_TICK.
 *   @remark
 * The system clock interrupt should have a high interrupt vector number. Therefore we
 * chose PIT 3 rather than PIT 0..2. All kernel interrupts share the same INTC priority 1;
 * and therefore the vector number is the inverted, effective priority of a kernel
 * interrupt. For most applications, it's will be desired that real hardware events -
 * implemented as interrupts - get a higher priority than the system timer.
 */
static void enableIRQTimerTick(void)
{
    /* Disable all PIT timers during configuration. */
    PIT.PITMCR.R = 0x2;

    /* Install the interrupt handler for cyclic timer PIT 3. It drives the OS scheduler for
       cyclic task activation. We choose PIT 3 since it has a significant lower priority
       than the other three. (This matters because all kernel interrupts need to share the
       same INTC priority level 1.) */ 
    ihw_installINTCInterruptHandler
                        ( (int_externalInterruptHandler_t){.kernelIsr = isrSystemTimerTick}
                        , /* vectorNum */ 127 /* Timer PIT 3 */
                        , /* psrPriority */ 1
                        , /* isPreemptable */ true
                        , /* isOsInterrupt */ true
                        );

#if TEST_USE_IRREGULAR_SYS_CLOCK == 1
    /* For stability testing only, we can realize the kernel clock by three cyclic timers,
       which yield in sum the same (average) clock rate but which have mutual prime cycle
       times so that all possible phase shifts are seen, in particular critical short
       distances. */
    ihw_installINTCInterruptHandler
                        ( (int_externalInterruptHandler_t)
                            {.kernelIsr = isrSystemTimerTick_testPid1}
                        , /* vectorNum */ 60 /* Timer PIT 1 */
                        , /* psrPriority */ 1
                        , /* isPreemptable */ true
                        , /* isOsInterrupt */ true
                        );
    ihw_installINTCInterruptHandler
                        ( (int_externalInterruptHandler_t)
                            {.kernelIsr = isrSystemTimerTick_testPid2}
                        , /* vectorNum */ 61 /* Timer PIT 2 */
                        , /* psrPriority */ 1
                        , /* isPreemptable */ true
                        , /* isOsInterrupt */ true
                        );
#endif

    /* Peripheral clock has been initialized to 120 MHz. To get a 1ms interrupt tick we
       need to count till 120000.
         -1: See MCU reference manual, 36.5.1, p. 1157. */
#if TEST_USE_IRREGULAR_SYS_CLOCK != 1
    _Static_assert( (RTOS_TICK) >= 1e-5  &&  (RTOS_TICK) <= 30.0
                  , "System clock period RTOS_TICK configured out of range"
                  );
    PIT.LDVAL3.R = (uint32_t)((RTOS_TICK) * 120e6)-1; /* Interrupt rate configurable */
#else
    _Static_assert( (RTOS_TICK) == 1e-3  ||  (RTOS_TICK) == 1e-4
                  , "Test TEST_USE_IRREGULAR_SYS_CLOCK is hard-coded for a system clock"
                    " tick of either 1ms or 100us"
                  );
    if((RTOS_TICK) == 1e-3)
    {
        PIT.LDVAL1.R = 359981-1; /* Interrupt rate 3*1ms */
        PIT.LDVAL2.R = 359987-1; /* Interrupt rate 3*1ms */
        PIT.LDVAL3.R = 360007-1; /* Interrupt rate 3*1ms */
    }
    else
    {
        PIT.LDVAL1.R = 35993-1; /* Interrupt rate 3*100us */
        PIT.LDVAL2.R = 35999-1; /* Interrupt rate 3*100us */
        PIT.LDVAL3.R = 36007-1; /* Interrupt rate 3*100us */
    }
#endif

    /* Enable interrupts by this timer and start it. */
    PIT.TCTRL3.R = 0x3;
#if TEST_USE_IRREGULAR_SYS_CLOCK == 1
    PIT.TCTRL1.R = 0x3;
    PIT.TCTRL2.R = 0x3;
#endif

    /* Enable timer operation and let them be stopped on debugger entry. Note, this is a
       global setting for all four timers, even if we use and reserve only one for the
       RTOS. */
    PIT.PITMCR.R = 0x1;

} /* End of enableIRQTimerTick */



/**
 * Actual implementation of routine \a rtos_sendEvent. The task posts a set of events and the
 * scheduler is asked which task is the one to be activated now.\n
 *   The action of this SW interrupt is placed into an own function in order to let the
 * compiler generate the stack frame required for all local data. (The stack frame
 * generation of the SW interrupt entry point needs to be inhibited in order to permit the
 * implementation of saving/restoring the task context).
 *   @return
 * The function determines which task is to be activated and records which task is left
 * (i.e. the task calling this routine) in the global variables \a _pActiveTask and \a
 * _pSuspendedTask.\n
 *   If there is a task switch the function reports this by a return value true. If there
 * is no task switch it returns false and the global variables \a _pActiveTask and \a
 * _pSuspendedTask are not touched.
 *   @param postedEventVec
 * See software interrupt #rtos_sendEvent().
 *   @remark
 * This function and particularly passing the return codes via a global variable will
 * operate only if all interrupts are disabled.
 */
static bool sendEvent(uint32_t postedEventVec)
{
    bool activeTaskMayChange = false;

    /* The timer events must not be set manually. */
    assert((postedEventVec & MASK_EVT_IS_TIMER) == 0);

    /* We keep track of all semaphores and mutexes, which have to be posted (released)
       exactly once - to the first task, which is waiting for them. This task is done,
       when the related mask, semaphoreToReleaseVec or mutexToReleaseVec becomes null. */
#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON
    uint32_t semaphoreToReleaseVec = postedEventVec & MASK_EVT_IS_SEMAPHORE;
#endif
#if RTOS_USE_MUTEX == RTOS_FEATURE_ON
    uint32_t mutexToReleaseVec = postedEventVec & MASK_EVT_IS_MUTEX;
# ifdef DEBUG
    uint32_t dbg_allMutexesToReleaseVec = mutexToReleaseVec;
# endif
#endif
#if RTOS_USE_MUTEX == RTOS_FEATURE_ON  ||  RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON
    /* The implementation makes a distinction between synchronization objects (mutex and
       semaphores) and ordinary, broadcasted events. */
    postedEventVec &= ~(MASK_EVT_IS_MUTEX | MASK_EVT_IS_SEMAPHORE);
#endif

    /* Post ordinary events to all suspended tasks which are waiting for it.
         Pass mutexes and semaphores to a single task each, those task, which is of highest
       priority and waits the longest for it. This loop is the reason, why we need to have
       the list of suspended tasks always sorted, when at least one semaphore or mutex is in
       use. */
    unsigned int idxSuspTask = 0;
    while(idxSuspTask<_noSuspendedTasks)
    {
        task_t * const pT = _pSuspendedTaskAry[idxSuspTask];

        /* Remember the received events before (possibly) getting some more by this
           sendEvent: Only if this set changes it is necessary to check for a state
           transition of the task. */
        const uint32_t postedEventVecBefore = pT->postedEventVec;

#if RTOS_USE_MUTEX == RTOS_FEATURE_ON
        /* Mutexes are Boolean and can't be posted twice to a task. This is easily possible
           but an application error. This assertion fires if the application doesn't
           properly keep track of who owns which mutex. */
        assert((pT->postedEventVec & dbg_allMutexesToReleaseVec) == 0);

        /* The vector of all events this task will receive. */
        uint32_t gotEvtVec = (postedEventVec | mutexToReleaseVec) & pT->eventMask;

        /* Collect the events in the task object. */
        pT->postedEventVec |= gotEvtVec;

        /* Subtract the given mutexes from the vector of all, which are to release in this
           call. It doesn't matter that the mask also contains ordinary events - all these
           bits can do is to reset already reset bits. */
        mutexToReleaseVec &= ~gotEvtVec;
#else
        pT->postedEventVec |= (postedEventVec & pT->eventMask);

#endif /* RTOS_USE_MUTEX == RTOS_FEATURE_ON */

#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON
        /* Now pass all released semaphores to tasks waiting for them. This can't be done
           all at once as semaphores are not simply a bit. We need a second loop to do so. */
        uint32_t semMask = 0x01;
        while(semaphoreToReleaseVec != 0  &&  (semMask & MASK_EVT_IS_SEMAPHORE) != 0)
        {
            /* Check if this task is waiting for this semaphore but didn't get it yet (i.e.
               in an earlier call of this routine). */
            if((semaphoreToReleaseVec & semMask & pT->eventMask & ~pT->postedEventVec) != 0)
            {
                /* The semaphore release operation is handled by passing the semaphore to
                   the task. */
                pT->postedEventVec    |= semMask;
                semaphoreToReleaseVec &= ~semMask;
            }

            /* Shift mask one Bit to the left to test next semaphore in next cycle. */
            semMask <<= 1;
        }
#endif

        /* Check if this suspended task becomes due because of an event, which was posted
           to it. */
        if(postedEventVecBefore != pT->postedEventVec  && checkTaskForActivation(idxSuspTask))
        {
            /* The task becomes due. */
            activeTaskMayChange = true;

            /* Check next suspended task, which is in this case found in the array
               element with same index: The task which became due has been removed into the
               due list of its priority class. */
        }
        else
        {
            /* The task remains suspended. */

            /* Check next suspended task, which is in this case found in the next array
               element. */
            ++ idxSuspTask;

        } /* End if(Did this suspended task become due?) */

    } /* End while(All suspended tasks) */

#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON
    /* The remaining semaphores (more precise: semaphore counter values) are accumulated in
       the global semaphore storage for later acquisition. */
    unsigned int idxSem = 0;
    while(semaphoreToReleaseVec)
    {
        if((semaphoreToReleaseVec & 0x01) != 0)
        {
            ++ rtos_semaphoreAry[idxSem];

            /* The assertion validates the application code. It fires if the application
               releases/produces more semaphore counter values as fit into the data type of
               the semphore. This is harmless with respect to the stability of the RTOS but
               probably a design error in the application. */
            assert(rtos_semaphoreAry[idxSem] != 0);
        }

        /* Test next semaphore in next cycle. */
        semaphoreToReleaseVec >>= 1;

        /* The next event/semaphore bit is related to the next array entry. */
        ++ idxSem;
    }
#endif
#if RTOS_USE_MUTEX == RTOS_FEATURE_ON
    /* The remaining mutexes are returned to the global storage for later acquisition.
         The assertion validates application code. It fires if the application releases
       a mutex, which was not acquired. This is harmless with respect to the stability of
       the RTOS but probably a design error in the application. Maybe, you have to
       consider using a semaphore. */
    assert((_mutexVec & dbg_allMutexesToReleaseVec) == 0);
    _mutexVec |= mutexToReleaseVec;
#endif /* RTOS_USE_MUTEX == RTOS_FEATURE_ON */

    /* Check if another task becomes active because of the posted events.
         activeTaskMayChange: We do the search for the new active task only if at least one
       suspended task was resumed.
         The function has side effects: If there's a task which was suspended before and
       which is resumed because of an event and which is of higher priority than the one
       being active so far then the references to the old and newly active task are written
       into the global variables _pSuspendedTask and _pActiveTask. */
    return activeTaskMayChange && lookForActiveTask();

} /* End of sendEvent */




/**
 * A task (including the idle task) may post an event. The event is broadcasted to all
 * suspended tasks which are waiting for it. An event is not saved beyond that. If a task
 * suspends and starts waiting for an event which has been posted by another task just
 * before, it'll wait forever and never be resumed.\n
 *   The posted event may resume another task, which may be of higher priority as the event
 * posting task. In this case #rtos_sendEvent() will cause a task switch. The calling task
 * stays due but stops to be the active task. It does not become suspended (this is why
 * even the idle task may call this function). The activated task will resume by coming out
 * of the suspend command it had been invoked to wait for this event. The return value of
 * this suspend command will then tell about the event sent here.\n
 *   If no task of higher priority is resumed by the posted event the calling task will be
 * continued immediately after execution of this method. In this case #rtos_sendEvent()
 * behaves like any ordinary sub-routine.
 *   @return
 * The function returns \a true if the sent events cause the activation of another task
 * than the currently active.
 *   @param pCmdContextSwitch
 * If the function return \a true and decides to activate another task then it states in *
 * \a pCmdContextSwitch where to save the context information of the de-activated task and
 * where to find the context description of the activated task. Furthermore, here it states
 * the return value to be passed on to the activated task.
 *   @param eventVec
 * A bit vector of posted events. Known events are defined in rtos.h. The timer events
 * #RTOS_EVT_ABSOLUTE_TIMER and #RTOS_EVT_DELAY_TIMER cannot be posted.
 *   @see
 * #rtos_waitForEvent()
 *   @remark
 * This is the implementation of system call index #SC_IDX_SYS_CALL_SEND_EVENT. The
 * function must never be called from task code but it may be invoked from kernel relevant
 * ISRs, which may interact with the scheduler by sending events and by provoking a context
 * switch in this way, as a consequence of the sent events.
 */
uint32_t rtos_sc_sendEvent(int_cmdContextSwitch_t *pCmdContextSwitch, uint32_t eventVec)
{
    /* Check for all suspended tasks if the posted events will resume them.
         The actual implementation of the function's logic is placed into a sub-routine in
       order to benefit from the compiler generated stack frame for local variables (in
       this naked function we must not have declared any). */
    if(sendEvent(eventVec))
    {
        /* Yes, another task becomes active with this timer tick. Command the calling
           system call handler on return to switch to the other task. The vector of
           received events is the value returned to the activated task. */
        pCmdContextSwitch->signalToResumedContext = _pActiveTask->postedEventVec;
        _pActiveTask->postedEventVec = 0;
        pCmdContextSwitch->pSuspendedContextSaveDesc = &_pSuspendedTask->contextSaveDesc;
        pCmdContextSwitch->pResumedContextSaveDesc = &_pActiveTask->contextSaveDesc;

        return int_rcIsr_switchContext;
    }
    else
    {
        /* No other task has been resumed by the posted events and we return to the same
           calling task. There is no function result. */
        return int_rcIsr_doNotSwitchContext;
    }
} /* End of rtos_sc_sendEvent */



#if RTOS_USE_APPL_INTERRUPT_00 == RTOS_FEATURE_ON
/**
 * A conditionally compiled interrupt service routine. If #RTOS_USE_APPL_INTERRUPT_00 is
 * set to #RTOS_FEATURE_ON, this function implements an ISR, which posts event
 * #RTOS_EVT_ISR_USER_00 every time the interrupt occurs.\n
 *   The use case is to have a task (of high priority) which implements an infinite loop.
 * At the beginning of each loop cycle the task will suspend itself waiting for this event
 * (and maybe for a timeout). The body of the loop will then handle the interrupt.\n
 *   The implementation of the ISR is generic to a certain extend only. Two elements require
 * application dependent configuration:\n
 *   For the e200z4 core, the interrupt source is selected by index. The index is
 * configured as #RTOS_ISR_USER_00. A table of all interrupt sources with their indexes is
 * found in the MCU reference manual, section 28.7, table 28-4.\n
 *   An interrupt normally needs to reset the reqeusting interrupt bit in the hardware
 * device, which is configured as interrupt source. The C code to do do - usually a simple
 * assignment to a device register - is configured as #RTOS_ISR_USER_00_ACKNOWLEDGE_IRQ.
 *   @return
 * Posting the event will easily yield a context switch to a task, which waits for this
 * event. Therefore, this ISR is implemented as kernel relevant interrupt, which has the
 * option to demand a context switch on return. A context switch is requested by return
 * value \a true. If the ISR returns \a false then the interrupted task is continued.
 *   @param pCmdContextSwitch
 * If the function requests a context switch then the ISR provides the context save areas
 * of suspended and resumed task through * \a pCmdContextSwitch.
 */
static uint32_t isrUser00(int_cmdContextSwitch_t *pCmdContextSwitch)
{
    /* Acknowledge the timer interrupt in the causing HW device. */
    RTOS_ISR_USER_00_ACKNOWLEDGE_IRQ
    
    /* The action and the decision whether the posted event yields a task switch is
       delegated to the normal implementation of system call sendEvent. Note, an ISR cannot
       use the system call mechanism. */
    return rtos_sc_sendEvent(pCmdContextSwitch, /* eventVec */ RTOS_EVT_ISR_USER_00);

} /* End of isrUser00 */
#endif /* RTOS_USE_APPL_INTERRUPT_00 == RTOS_FEATURE_ON */




#if RTOS_USE_APPL_INTERRUPT_01 == RTOS_FEATURE_ON
/**
 * A conditionally compiled interrupt service routine. If #RTOS_USE_APPL_INTERRUPT_01 is
 * set to #RTOS_FEATURE_ON, this function implements an ISR, which posts event
 * #RTOS_EVT_ISR_USER_01 every time the interrupt occurs.\n
 *   The use case is to have a task (of high priority) which implements an infinite loop.
 * At the beginning of each loop cycle the task will suspend itself waiting for this event
 * (and maybe for a timeout). The body of the loop will then handle the interrupt.\n
 *   The implementation of the ISR is generic to a certain extend only. Two elements require
 * application dependent configuration:\n
 *   For the e200z4 core, the interrupt source is selected by index. The index is
 * configured as #RTOS_ISR_USER_01. A table of all interrupt sources with their indexes is
 * found in the MCU reference manual, section 28.7, table 28-4.\n
 *   An interrupt normally needs to reset the requesting interrupt bit in the hardware
 * device, which is configured as interrupt source. The C code to do do - usually a simple
 * assignment to a device register - is configured as #RTOS_ISR_USER_01_ACKNOWLEDGE_IRQ.
 *   @return
 * Posting the event will easily yield a context switch to a task, which waits for this
 * event. Therefore, this ISR is implemented as kernel relevant interrupt, which has the
 * option to demand a context switch on return. A context switch is requested by return
 * value \a true. If the ISR returns \a false then the interrupted task is continued.
 *   @param pCmdContextSwitch
 * If the function requests a context switch then the ISR provides the context save areas
 * of suspended and resumed task through * \a pCmdContextSwitch.
 */
static uint32_t isrUser01(int_cmdContextSwitch_t *pCmdContextSwitch)
{
    /* Acknowledge the timer interrupt in the causing HW device. */
    RTOS_ISR_USER_01_ACKNOWLEDGE_IRQ

    return rtos_sc_sendEvent(pCmdContextSwitch, /* eventVec */ RTOS_EVT_ISR_USER_01);

} /* End of isrUser01 */
#endif /* RTOS_USE_APPL_INTERRUPT_01 == RTOS_FEATURE_ON */



/**
 * This is a code pattern (inline function) which saves the resume condition of a task,
 * which is going to be suspended into its task object. This pattern is mainly used in the
 * suspend function rtos_waitForEvent but also at task initialization time, when the
 * initial task start condition is specified by the application code.
 *   @param pT
 * Pointer to the task object of the suspend task.
 *   @param eventMask
 * See function \a rtos_waitForEvent for details.
 *   @param all
 * See function \a rtos_waitForEvent for details.
 *   @param timeout
 * See function \a rtos_waitForEvent for details.
 *   @see
 * void rtos_waitForEvent(uint32_t, bool, unsigned int)
 *   @remark
 * For performance reasons this function needs to be inlined. A macro would be an
 * alternative.
 */

static inline void storeResumeCondition( task_t * const pT
                                       , uint32_t eventMask
                                       , bool all
                                       , unsigned int timeout
                                       )
{
    /* Check event condition: It must not be empty. The two timers can't be used at the
       same time (which is a rather harmless application design error) and at least one
       other event needs to be required for a resume in case of the AND condition.
         It leads to a crash if the AND condition is not combined with a required event. If
       "all" is set then RTuinOS interprets the empty event mask as the always fulfilled
       resume condition. The task became due without any bit set in the task's
       postedEventVec - which leads to a wrong state transition in the kernel. (Please
       refer to the manual for an in-depth explanation of the task state meaning of
       postedEventVec.) */
    assert(eventMask != 0
           &&  (eventMask & MASK_EVT_IS_TIMER) != MASK_EVT_IS_TIMER
           &&  (!all || (eventMask & ~MASK_EVT_IS_TIMER) != 0)
          );

    /* The timing parameter may refer to different timers. Depending on the event mask we
       either load the one or the other timer. Default is the less expensive delay
       counter. */
    if((eventMask & RTOS_EVT_ABSOLUTE_TIMER) != 0)
    {
        /* This suspend command wants a reactivation at a certain time. The new time is
           assigned by the += in the conditional expression.
             Task overrun detection: The new time must be no more than half a cycle in the
           future. The test uses a signed comparison. */
        if((signed int)((pT->timeDueAt+=timeout) - _time) <= 0)
        {
            const unsigned int newVal = pT->cntOverrun + 1;
            if(newVal != 0)
                pT->cntOverrun = newVal;

            /* The wanted point in time is over. We do the best recovery which is possible:
               Let the task become due in the very next timer tick. */
            pT->timeDueAt = _time + 1;
        }
    }
    else
    {
        /* The timeout counter is reloaded. We could do this conditionally if the bit is
           set, but in all normal use cases it'll and if not it doesn't harm. We will
           just decrement the delay counter in onTimerTick once but not set the event.
             ++ timeout: The call of the suspend function is in no way synchronized with the
           system clock. We define the delay to be a minimum and implement the resolution
           caused uncertainty as an additional delay. */
        if(timeout+1 != 0)
            ++ timeout;
        pT->cntDelay = timeout;

    } /* if(Which timer is addressed for the timeout condition?) */

    pT->eventMask = eventMask;
    pT->waitForAnyEvent = !all;

} /* End of storeResumeCondition */



#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON  ||  RTOS_USE_MUTEX == RTOS_FEATURE_ON
/**
 * Waiting for events doesn't necessarily mean waiting: If the wait condition includes
 * mutexes and semaphores these may already be available on entry in the wait function.
 * This routine checks all demanded sync objects and locks and allocates all those objects
 * to the calling task, which are currently free.
 *   @return
 * Finally the routine checks if the wait condition of the suspend command is already
 * fulfilled with the found sync objects. If and only if so it returns true and the suspend
 * command won't actually block the calling task.
 *   @param eventMask
 * See function \a rtos_waitForEvent for details.
 *   @param all
 * See function \a rtos_waitForEvent for details.
 *   @see
 * void rtos_waitForEvent(uint32_t, bool, unsigned int)
 *   @remark
 * This function is inlined for performance reasons.
 */
static inline bool acquireFreeSyncObjs(uint32_t eventMask, bool all)
{
#if RTOS_USE_MUTEX == RTOS_FEATURE_ON
    /* Check for immediate availability of all/any mutex. These mutexes are locked now and
       entered in the calling task's postedEventVec. */
    _pActiveTask->postedEventVec = eventMask & _mutexVec;

    /* Lock the demanded mutexes. Bits in eventMask which do not correspond to mutex events
       don't need to be masked. They may reset some anyway reset don't-care bits in
       _mutexVec. */
    _mutexVec &= ~eventMask;
#endif

#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON
    /* Check for immediate availability of all/any semaphores. The counters of none zero
       semaphores are decremented now and the related acquired-bit is set in the calling
       task's postedEventVec. */
    unsigned int idxSem = 0;
    uint32_t maskSem = 0x01
           , semaphoreToAcquireVec = eventMask & MASK_EVT_IS_SEMAPHORE;
    while(semaphoreToAcquireVec)
    {
        if((semaphoreToAcquireVec & 0x01) != 0)
        {
            if(rtos_semaphoreAry[idxSem] > 0)
            {
                -- rtos_semaphoreAry[idxSem];
                _pActiveTask->postedEventVec |= maskSem;
            }
        }

        /* Test next event bit and increment related index of semaphore in array. */
        ++ idxSem;
        maskSem <<= 1;
        semaphoreToAcquireVec >>= 1;
    }
#endif

    /* postedEventVec now contains all requested mutexes, which were currently available.
       If these were all demanded events, we don't need to suspend the task but can
       immediately and successfully return. The timer bits (events 14 and 15) don't matter
       as they are always OR terms. We definitely don't need to wait for them. */
    return (!all &&  _pActiveTask->postedEventVec != 0)
           || (all &&  ((_pActiveTask->postedEventVec ^ eventMask) & ~MASK_EVT_IS_TIMER) == 0);

} /* End of acquireFreeSyncObjs */

#endif /* RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON  ||  RTOS_USE_MUTEX == RTOS_FEATURE_ON */


/**
 * Actual implementation of task suspension routine \a rtos_waitForEvent. The task is
 * suspended until a specified event occurs.\n
 *   The action of this SW interrupt is placed into an own function in order to let the
 * compiler generate the stack frame required for all local data. (The stack frame
 * generation of the SW interrupt entry point needs to be inhibited in order to permit the
 * implementation of saving/restoring the task context).
 *   @return
 * If mutex or semaphores are in use the function returns the Boolean information whether
 * the calling task has to be suspended. This is not the case if all demanded sync objects
 * are currently available. If RTuinOS is compiled without support of mutexes and
 * sempahores than the calling task is always suspended.\n
 *   By means of side effects, the function returns which task is to be activated and records
 * which task is left (i.e. the task calling this routine) in the global variables \a
 * _pActiveTask and \a _pSuspendedTask.
 *   @param eventMask
 * See software interrupt \a rtos_waitForEvent.
 *   @param all
 * See software interrupt \a rtos_waitForEvent.
 *   @param timeout
 * See software interrupt \a rtos_waitForEvent.
 *   @see
 * uint32_t rtos_waitForEvent(uint32_t, bool, unsigned int)
 *   @remark
 * This function and particularly passing the return codes via a global variable will
 * operate only if all interrupts are disabled.
 */
#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON  ||  RTOS_USE_MUTEX == RTOS_FEATURE_ON
static bool waitForEvent( uint32_t eventMask
                        , bool all
                        , unsigned int timeout
                        )
#else
static void waitForEvent(uint32_t eventMask, bool all, unsigned int timeout)
#endif
{
    /* Here, we could double-check _pActiveTask for the idle task and return without
       context switch if it is active. (The idle task has illicitly called a suspend
       command.) However, all implementation rates performance higher than failure
       tolerance, and so do we here. */
    assert(_pActiveTask != _pIdleTask);

#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON  ||  RTOS_USE_MUTEX == RTOS_FEATURE_ON
    if(acquireFreeSyncObjs(eventMask, all))
        return false;
#endif

    signed int idxPrio;
    signed int idxTask;

    /* Take the active task out of the list of due tasks. */
    task_t * const pT = _pActiveTask;
    unsigned int prio = pT->prioClass;
    signed int noDueNow = (int)(-- _noDueTasksAry[prio]);
    for(idxTask=0; idxTask<noDueNow; ++idxTask)
        _pDueTaskAryAry[prio][idxTask] = _pDueTaskAryAry[prio][idxTask+1];

    /* This suspend command wants a reactivation by a combination of events (which may
       include the timeout event). Save the resume condition in the task object. The
       operation is implemented as inline function to be able to reuse the code in the
       task initialization routine. */
    storeResumeCondition(pT, eventMask, all, timeout);

    /* Put the task in the list of suspended tasks. If mutexes or semaphores are in use
       this list is sorted with decreasing priority. */
#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON  ||  RTOS_USE_MUTEX == RTOS_FEATURE_ON
    /* < rather than <=: The now suspended task becomes the last one in its prio class. The
       others of same priority are waiting longer and will receive a later posted event
       with priority. */
    signed int idxPos;
    for(idxPos=0; idxPos<(int)_noSuspendedTasks; ++idxPos)
        if(_pSuspendedTaskAry[idxPos]->prioClass < prio)
            break;

    /* Shift rest of the list one to the end. This loop requires a signed index. */
    for(idxTask=(int)_noSuspendedTasks++ - 1; idxTask>=idxPos; --idxTask)
        _pSuspendedTaskAry[idxTask+1] = _pSuspendedTaskAry[idxTask];

    /* Insert the suspended task at the priority determined right position. */
    _pSuspendedTaskAry[idxPos] = pT;
#else
    _pSuspendedTaskAry[_noSuspendedTasks++] = pT;
#endif

    /* Record which task suspends itself for the assembly code in the calling function
       which actually switches the context. */
    _pSuspendedTask = _pActiveTask;

    /* Look for the task we will return to. It's the first entry in the highest non-empty
       priority class. The loop requires a signed index.
         It's not guaranteed that there is any due task. Idle is the fallback. */
    _pActiveTask = _pIdleTask;
    for(idxPrio=RTOS_NO_PRIO_CLASSES-1; idxPrio>=0; --idxPrio)
    {
        if(_noDueTasksAry[idxPrio] > 0)
        {
            _pActiveTask = _pDueTaskAryAry[idxPrio][0];
            break;
        }
    }

#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON  ||  RTOS_USE_MUTEX == RTOS_FEATURE_ON
    return true;
#endif
} /* End of waitForEvent */




/**
 * Suspend the current task (i.e. the one which invokes this method) until a specified
 * combination of events occur. In the special case, that the specified combination of
 * events can be satisfied with currently available synchronization objects (mutexes and
 * semaphores) the function will return immediately without suspending the calling task.\n
 *   A task is suspended in the instance of calling this method. It specifies a list of
 * events. The task becomes due again, when either the first one or all of the specified
 * events have been posted by other tasks.\n
 *   The idle task can't be suspended. If it calls this function a crash would be the
 * immediate result.
 *   @return
 * The function returns \a true if the sent events cause the activation of another task
 * than the currently active. Otherwise it'll return \a false and control returns to the
 * calling task like for an ordinary function call.
 *   @param pCmdContextSwitch
 * If the function return \a true and decides to activate another task then it states in *
 * \a pCmdContextSwitch where to save the context information of the de-activated task and
 * where to find the context description of the activated task.\n
 *   Furthermore, here it states the result value to be passed on to the activated task or
 * to return to the continued task. The result value is in the set of actually resuming or
 * acquired events, represented by a bit vector, which corresponds bitwise to \a eventMask.
 * See \a rtos.h for a list of known events.
 *   @param eventMask
 * The bit vector of events to wait for. Needs to include either the delay timer event
 * #RTOS_EVT_DELAY_TIMER or #RTOS_EVT_ABSOLUTE_TIMER, if a timeout is required, but not both
 * of them.\n
 *   The normal use case will probably be the delay timer. However, a regular task may also
 * specify the absolute timer with the next regular task time as parameter so that it
 * surely becomes due outermost at its usual task time.
 *   @param all
 *   If false, the task is made due as soon as the first event mentioned in \a eventMask is
 * seen.\n
 *   If true, the task is made due only when all events are posted to the suspending task -
 * except for the timer events, which are still OR combined. If you say "all" but the event
 * mask contains either #RTOS_EVT_DELAY_TIMER or #RTOS_EVT_ABSOLUTE_TIMER, the task will
 * resume when either the timer elapsed or when all other events in the mask were seen.\n
 *   Caution: If all is true, there need to be at least one event bit set in \a eventMask
 * besides a timer bit; RTuinOS crashes otherwise. Saying: "No particular event, just a
 * timer condition" needs to be implemented by \a all set to false and \a eventMask set to
 * only a timer event bit.
 *   @param timeout
 * If \a eventMask contains #RTOS_EVT_DELAY_TIMER:\n
 *   The number of system timer ticks from now on until the timeout elapses. One should be
 * aware of the resolution of any timing being the tick of the system timer. A timeout of \a
 * n may actually mean any delay in the range \a n .. \a n+1 ticks.\n
 *   Even specifying 0 will suspend the task a short time and give others the chance to
 * become active - particularly other tasks belonging to the same priority class.\n
 *   If \a eventMask contains #RTOS_EVT_ABSOLUTE_TIMER:\n
 *   The absolute time the task becomes due again at latest. The time designation is
 * relative; it refers to the last recent absolute time at which this task had been
 * resumed. See #rtos_suspendTaskTillTime for details.\n
 *   Now the range of the parameter is 1 till the half the range of the data type which is
 * configured for the system time, e.g. 127 in case of uint8_t. Otherwise proper task
 * timing can't be guaranteed.\n
 *   If neither #RTOS_EVT_DELAY_TIMER nor #RTOS_EVT_ABSOLUTE_TIMER is set in the event mask,
 * this parameter should be zero.
 *   @remark
 * This is the implementation of a system call. You must never invoke the function
 * directly. It is called only through the system call interrupt handler. You API to this
 * function is macro #rtos_waitForEvent().
 */
uint32_t rtos_sc_waitForEvent( int_cmdContextSwitch_t *pCmdContextSwitch
                             , uint32_t eventMask
                             , bool all
                             , unsigned int timeout
                             )
{
#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON  ||  RTOS_USE_MUTEX == RTOS_FEATURE_ON
    if(waitForEvent(eventMask, all, timeout))
#else
    waitForEvent(eventMask, all, timeout);
#endif
    {
        /* Yes, another task becomes active with this timer tick. Command the calling
           system call handler on return to switch to the other task. The vector of
           received events is the value returned to the activated task. */
        pCmdContextSwitch->signalToResumedContext = _pActiveTask->postedEventVec;
        _pActiveTask->postedEventVec = 0;
        pCmdContextSwitch->pSuspendedContextSaveDesc = &_pSuspendedTask->contextSaveDesc;
        pCmdContextSwitch->pResumedContextSaveDesc = &_pActiveTask->contextSaveDesc;

        return int_rcIsr_switchContext;
    }
#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON  ||  RTOS_USE_MUTEX == RTOS_FEATURE_ON
    else
    {
        /* We return to the same calling task. All or at least a satisfying sub-set of the
           requested mutexes and semaphores has been acquired and there's no need to
           suspend the active task. The acquired objects are returned as system call
           result. */
        pCmdContextSwitch->signalToResumedContext = _pActiveTask->postedEventVec;
        _pActiveTask->postedEventVec = 0;

        return int_rcIsr_doNotSwitchContext;
    }
#endif
} /* End of rtos_sc_waitForEvent */





/**
 * This is the common guard function of the context entry functions: When a function, which
 * had been specified as context entry function is left with return then program flow
 * goes into this guard function.\n
 *   In RTuinOS it used to cause a reset if a forbidden leave of a task function is
 * attempted. The implementation of the guard emulates this behavior.
 *   @param retValOfContext
 * The guard function receives the return value of the left context entry function as
 * parameter.\n
 *   An RTuinOS task function doesn't return anything and the guard will receive a
 * meaningless, arbitrary value as function argument. Never use \a retValOfContext.
 *   @remark
 * Note, the guard function has no calling parent function. Any attempt to return from
 * this function will surely lead to a crash. The normal use case is to have a system call
 * implemented here, which notifies the situation about the terminating context to the
 * scheduler. On return, the system call implementation will surely not use the option \a
 * int_rcIsr_doNotSwitchContext and control will never return back to the guard.
 *   @remark
 * This is an RTuinOS specific, overloaded function implementation. The default
 * implementation is owned by package \a kernelBuilder, see file int_interruptHandler.S.
 */
_Noreturn void int_fctOnContextEnd(uint32_t retValOfContext ATTRIB_UNUSED)
{
#ifdef DEBUG
    iprintf( "int_fctOnContextEnd: Caught attempt to return from an RTuinOS task function,"
             " which is not allowed. System will try to reset\r\n"
           );
           
    /* Leave time to flush serial buffers, even at low Baud rate. */
    del_delayMicroseconds(/* tiCpuInUs */ 1000000);
    
    /* In DEBUG compilation it's most beneficial to be informated by assertion about the
       forbidden situation. */
    assert(false);
#endif    

    /* Emulation of original RTuinOS behavior: Reset the system.
         Note, for an MPC5643L MCU the startup code needs to be prepared for a warm start.
       The required mode transitions differ from cold start. */
    ((void (*)(void))(0x00000010))();

} /* End of int_fctOnContextEnd */



/**
 * Get the current value of the overrun counter of a given task.\n
 *   The value is a limited 8 Bit counter (i.e. it won't cycle around). This is considered
 * satisfying as any task overrun is a kind of error and should not happen in a real
 * application (with other words: even a Boolean information could be enough). Furthermore,
 * if a larger range is required, one can regularly ask for this information, accumulate it
 * and reset the value here to zero at the same time.\n
 *   The function may be called from a task or from the idle task.
 *   @return
 * Get the current value of the overrun counter.
 *   @param idxTask
 * The index of the task the overrun counter of which is to be returned. The index is the
 * same as used when initializing the tasks (see rtos_initializeTask).
 *   @param doReset
 * Boolean flag, which tells whether to reset the value.\n
 *   Caution, when setting this to true, reading and resetting the value needs to become an
 * atomic operation, which requires a critical section. This is significantly more
 * expensive than just reading the value and it globally enables the interrupts finally.
 * Therefore this call may destroy a surrounding critical section.
 *   @remark
 * This function has some limitations and weaknesses: First of all, the concept of task
 * overruns is defined only for regular tasks. Secondary, and particularly when using the 8
 * Bit system timer, there's a significant probability of not recognizing huge task
 * overruns. Finally there's a significant probability of false recognitions of (not
 * happening) task overruns if the task period time is greater than half the system timer's
 * cycle time (i.e. greater than 127 for the 8 Bit system timer). Please, refer to the
 * RTuinOS manual for details.
 *   @see
 * void rtos_initializeTask()
 */
unsigned int rtos_getTaskOverrunCounter(unsigned int idxTask, bool doReset)
{
    if(doReset)
    {
        unsigned int retCode
                   , * const pCntOverrun = &_taskAry[idxTask].cntOverrun;

        /* Read and reset should be atomic for data consistency if the application wants to
           accumulate the counter values in order to extend the counter's range. */
        rtos_enterCriticalSection();
        {
            retCode = *pCntOverrun;
            *pCntOverrun = 0;
        }
        rtos_leaveCriticalSection();

        return retCode;
    }
    else
    {
        /* Reading an 8 Bit word is an atomic operation as such, no additional lock
           operation needed. */
        return _taskAry[idxTask].cntOverrun;
    }
} /* End of rtos_getTaskOverrunCounter */




/**
 * Compute how many bytes of the stack area of a task are still unused. If the value is
 * requested after an application has been run a long while and has been forced to run
 * through all its paths many times, it may be used to optimize the static stack allocation
 * of the task. The function is useful only for diagnosis purpose as there's no chance to
 * dynamically increase or decrease the stack area at runtime.\n
 *   The function may be called from a task or from the idle task.\n
 *   The algorithm is as follows: The unused part of the stack is initialized with a
 * specific pattern byte. This routine counts the number of subsequent pattern bytes down
 * from the top of the stack area. This number is returned.\n
 *   The returned result must not be trusted too much: It could of course be that a pattern
 * byte is found not because of the initialization but because it has been pushed onto the
 * stack - in which case the return value is too great (too optimistic) by one. The
 * probability that this happens is significantly greater than zero. The chance that two
 * pattern bytes had been pushed is however much less and the probability of three, four,
 * five such bytes in sequence is negligible. (Except the irrelevant case you initialize
 * an automatic array variable with all pattern bytes.) Any stack size optimization based
 * on this routine should therefore subtract e.g. five bytes from the returned reserve and
 * diminish the stack outermost by this modified value.\n
 *   Be careful with stack size optimization based on this routine: Even if the application
 * ran a long time there's a non-zero probability that there has not yet been a system
 * timer interrupt in the very instance that the code of the task of interest was busy in
 * the deepest nested sub-routine, i.e. when having the largest stack consumption. A good
 * suggestion is to have another 36 Byte of reserve - this is the stack consumption if an
 * interrupt occurs.\n
 *   Recipe: Run your application a long time, ensure that it ran through all paths, get
 * the stack reserve from this routine, subtract 5+36 Byte and diminish the stack by this
 * value.
 *   @return
 * The number of still unused stack bytes. See function description for details.
 *   @param idxTask
 * The index of the task the stack usage has to be investigated for. The index is the
 * same as used when initializing the tasks, see rtos_initializeTask().\n
 *   Note, this function may be called for the idle task, too. To query the stack reserve
 * of the idle task pass the pseudo task index #RTOS_NO_TASKS.
 *   @remark
 * The computation is a linear search for the first non-pattern byte and thus relatively
 * expensive. It's suggested to call it only in some specific diagnosis compilation or
 * occasionally from the idle task.
 */
unsigned int rtos_getStackReserve(unsigned int idxTask)
{
    assert(idxTask < RTOS_NO_TASKS+1);

    uint32_t *sp = _taskAry[idxTask].pStackArea;

    /* The bottom of the stack is always initialized with 0, which must not be the pattern
       byte. Therefore we don't need a limitation of the search loop - it'll always find a
       non-pattern byte in the stack area. */
    while(*sp == UNUSED_STACK_PATTERN)
        ++ sp;

    return sizeof(uint32_t *) * (unsigned int)(sp - _taskAry[idxTask].pStackArea);

} /* End of rtos_getStackReserve */




/**
 * Initialize the contents of a single task object.\n
 *   This routine needs to be called from within setup() once for each task. The number of
 * tasks has been defined by the application using #RTOS_NO_TASKS but the array of this
 * number of task objects is still empty. The system will crash if this routine is not
 * called properly for each of the tasks before the RTOS actually starts.\n
 *   This function must never be called outside of setup(). A crash would result otherwise.
 *   @param idxTask
 * The index of the task in the range 0..RTOS_NO_TASKS-1. The order of tasks barely
 * matters.
 *   @param taskFunction
 * The task function as a function pointer. It is used once and only once: The task
 * function is invoked the first time the task becomes active and must never end. A
 * return statement would cause an immediate reset of the controller.
 *   @param prioClass
 * The priority class this task belongs to. Priority class 255 has the highest
 * possible priority and the lower the value the lower the priority.
 *   @param timeRoundRobin
 * The maximum time a task may be active if it is operated in round-robin mode. The
 * range is 1..max_value(\a unsigned int). The effective length of the time slice is less
 * than the nominal value. The time begins when the task becomes active and ends at the \a
 * timeRoundRobin-th system timer tick. Normally, a task becomes due at a system timer tick
 * but not immediately active. The effective time slice duration is (\a timeRoundRobin-1
 * ... \a timeRoundRobin] * #RTOS_TICK.\n
 *   Specify a maximum time of 0 to switch round robin mode off for this task.\n
 *   Remark: Round robin like behaviour is given only if there are several tasks in the
 * same priority class and all tasks of this class have the round-robin mode
 * activated. Otherwise it's just the limitation of execution time for an individual
 * task.\n
 *   This parameter is available only if #RTOS_ROUND_ROBIN_MODE_SUPPORTED is set to
 * #RTOS_FEATURE_ON.
 *   @param pStackArea
 * The pointer to the preallocated stack area of the task. The area needs to be
 * available all the RTOS runtime. Therefore dynamic allocation won't pay off. Consider
 * to use the address of any statically defined array.\n
 *   Note, each pre-emption of a context by an asynchronous External Interrupt requires
 * about 200 Bytes of stack space. If your application makes use of all interrupt
 * priorities then you need to have 15*200 Byte as a minimum of stack space for safe
 * operation, not yet counted the stack consumption of your application itself.\n
 *   Note, this lower bounds even holds if you apply the implementation of the priority
 * ceiling protocol from the startup code to mutually exclude sets of interrupts from
 * pre-empting one another, see https://community.nxp.com/message/993795 for details.\n
 *   The passed address needs to be 8 Byte aligned; this is double-checked by assertion.
 *   @param stackSize
 * The size in Byte of the memory area \a *pStackArea, which is reserved as stack for the
 * task. The size of the stack area needs to be a multiple of 8, which is checked by
 * assertion. Each task may have an individual stack size.
 *   @param startEventMask
 * The condition under which the task becomes due the very first time is specified nearly
 * in the same way as at runtime when using the suspend command \a rtos_waitForEvent: A set
 * of events to wait for is specified, the Boolean information if any event will activate
 * the task or if all are required and finally a timeout in case no such events would be
 * posted.\n
 *   This parameter specifies the set of events as a bit vector. Only ordinary events are
 * supports, i.e. broadcasted events. Events of kind mutex or semaphore must not be used
 * here. If a task needs to own such events right from beginning its implementation needs to
 * place an explicit suspend command \a rtos_waitForEvent as very first command.
 *   @param startByAllEvents
 * If true, all specified events (except for a timer event) must be posted before the task
 * is activated. Otherwise the first event belonging to the specified set will activate the
 * task. See rtos_waitForEvent for details.
 *   @param startTimeout
 * The task will be started at latest after \a startTimeout system timer ticks if only the
 * event #RTOS_EVT_DELAY_TIMER is in the set of specified events. If none of the timer
 * events #RTOS_EVT_DELAY_TIMER and #RTOS_EVT_ABSOLUTE_TIMER are set in \a startEventMask,
 * the task will not be activated by a time condition. Do not set both timer events at
 * once! See rtos_waitForEvent for details.
 *   @see void rtos_initRTOS(void)
 *   @see uint32_t rtos_waitForEvent(uint32_t, bool, unsigned int)
 *   @remark
 * The restriction that the initial resume condition must not comprise the request for mutex
 * or semaphore kind of events has been made just for simplicity. No additional code is
 * needed for broadcasted events but would be needed to allow mutexes and semaphores, too.\n
 *   The main use case for the start condition is to delay the take off of different tasks
 * by individual time spans to avoid having too many regular tasks becoming due at the same
 * timer tick. If there should be a use case for initial waiting for a mutex or semaphore
 * this can be easily implemented by an according suspend command at the beginning of the
 * actual task code.
 */
void rtos_initializeTask( unsigned int idxTask
                        , rtos_taskFunction_t taskFunction
                        , unsigned int prioClass
#if RTOS_ROUND_ROBIN_MODE_SUPPORTED == RTOS_FEATURE_ON
                        , unsigned int timeRoundRobin
#endif
                        , uint8_t * const pStackArea
                        , unsigned int stackSize
                        , uint32_t startEventMask
                        , bool startByAllEvents
                        , unsigned int startTimeout
                        )
{
    assert(idxTask < RTOS_NO_TASKS
           &&  ((uint32_t)pStackArea & 0x7) == 0   &&  (stackSize & 0x7) == 0
          );

    task_t * const pT = &_taskAry[idxTask];

    /* Remember task function and stack allocation. */
    pT->taskFunction = taskFunction;
    pT->pStackArea   = (uint32_t*)pStackArea;
    pT->stackSize    = stackSize;

    /* To which priority class does the task belong? */
    pT->prioClass = prioClass;

    /* Set the start condition. */
    assert(startEventMask != 0);

    /* Start condition "wait for sync object" is not implemented. */
#if RTOS_USE_MUTEX == RTOS_FEATURE_ON  ||  RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON
    assert((startEventMask & (MASK_EVT_IS_MUTEX | MASK_EVT_IS_SEMAPHORE)) == 0);
#endif

    pT->cntDelay = 0;
    pT->timeDueAt = 0;
    pT->cntOverrun = 0;
    storeResumeCondition(pT, startEventMask, startByAllEvents, startTimeout);

#if RTOS_ROUND_ROBIN_MODE_SUPPORTED == RTOS_FEATURE_ON
    /* The maximum execution time in round robin mode. */
    pT->timeRoundRobin = timeRoundRobin;
#endif

} /* End of rtos_initializeTask */




/**
 * The e200z4 port offers a more open and flexible way to deal with kernel interrupts.
 * These interrupts must not be initialized prior to the initialization of the kernel;
 * they can cause task switches and the kernel must already be in the state to accept
 * this. In the original Arduino programming model, the only possible code location for
 * doing so would be the function loop(), which is awkward, since it is repeatedly
 * invoked. We offer another hook: It is called after the kernel initialization but still
 * prior to the start of the system timer interrupts.
 *   @remark The use of this function is optional.
 */
RTOS_DEFAULT_FCT void setupAfterKernelInit(void)
{
} /* End of setupAfterKernelInit */


/**
 * To complete the set of initialization hooks, the e200z4 port of RTuinOS offers a
 * callback into the application code after having started the system timer and application
 * defined interrupts. The system is now fully up and running. This callback is called from
 * the context in which rtos_initRTOS() is invoked (normally the context of C's main(),
 * which now becomes the idle task context). The call of this function can also be
 * considered the first action of the idle task. After return from this function, RTuinOS
 * enters the infinite loop calling idle hook loop().
 *   @remark An application may decide not to return from this function. The implementation
 * of this function becomes the idle task and the Arduino loop() function will never be
 * called.
 *   @remark The use of this function is optional.
 */
RTOS_DEFAULT_FCT void setupAfterSystemTimerInit(void)
{
} /* End of setupAfterSystemTimerInit */



/**
 * This is the empty default implementation of the Ardunio like idle function loop(), which
 * is cyclically invoked as implementation of the idle task. In the e200z4 port of RTuinOS
 * this function become optional. An application doesn't need to implement it.
 *   @remark The use of this function is optional.
 */
RTOS_DEFAULT_FCT void loop(void)
{
} /* End of loop */



/**
 * Application called initialization of RTOS.\n
 *   Most important is the application handled task information. A task is characterized by
 * several static settings which need to be preset by the application. To save resources,
 * a standard situation will be the specification of all relevant settings at compile time
 * in the initializer expression of the array definition. The array is declared extern to
 * enable this mode.\n
 *   The application however also has the chance to provide this information at runtime.
 * Early in the execution of this function a callback setup() into the application is
 * invoked. If the application has setup everything as a compile time expression, the
 * callback may simply contain a return.\n
 *   The callback setup() is invoked before any RTOS related interrupts have been
 * initialized, the routine is executed in the same context as and as a substitute of the
 * normal Arduino setup routine. The implementation can be made without bothering with
 * interrupt inhibition or data synchronization considerations. Furthermore it can be used
 * for all the sort of things you've ever done in Arduino's setup routine.\n
 *   After returning from setup() all tasks defined in the task array are made due. The
 * main interrupt, which clocks the RTOS system time is started and will immediately make
 * the very task the active task which belongs to the highest priority class and which was
 * found first (i.e. in the order of rising indexes) in the task array. The system is
 * running.\n
 *   No idle task is specified in the task array. The idle task is implicitly defined and
 * implemented as the external function loop(). To stick to Arduino's convention (and to
 * give the RTOS the chance to benefit from idle as well) loop() is still implemented as
 * such: You are encouraged to return from loop() after doing things. RTOS will call \a
 * loop again as soon as it has some time left.\n
 *   As for the original Arduino code, setup() and loop() are mandatory, global
 * functions.\n
 *   This routine is not called by the application but in C's main function. Your code
 * seems to start with setup and seems then to branch into either loop() (the idle task)
 * or any other of the tasks defined by your application.\n
 *   This function never returns. No task must ever return, a reset will be the immediate
 * consequence. Your part of the idle task, function loop(), may and should return, but
 * the actual idle task as a whole won't terminate neither. Instead it'll repeat to call \a
 * loop.
 */
_Noreturn void rtos_initRTOS(void)
{
    unsigned int idxTask, idxClass;
    task_t *pT;

#ifdef DEBUG
    /* We add some code to double-check that rtos_initializeTask has been invoked for each
       of the tasks. */
    memset(/* dest */ _taskAry, /* val */ 0x00, /* len */ sizeof(_taskAry));
#endif

    /* Give the application the chance to do all its initialization - regardless of RTOS
       related or whatever else. After return, the task array needs to be properly
       filled. */
    setup();

    /* Handle all tasks. */
    for(idxTask=0; idxTask<RTOS_NO_TASKS; ++idxTask)
    {
        pT = &_taskAry[idxTask];

        /* Anticipate typical application errors with respect to the initialization of task
           objects. */
        assert(pT->taskFunction != NULL  &&  pT->pStackArea != NULL  &&  pT->stackSize >= 200);

        /* Prepare the stack of the task and store the initial stack pointer value. The
           task is initialized as if it had suspended by system call waitForEvent.
             @todo privilegedMode: RTuinOS is not prepared to run tasks in user mode. The
           infrastructure is missing; in particular the critical sections are implemented
           such that they require privileged instructions. */
        prepareTaskStack(pT->pStackArea, pT->stackSize);
        ccx_createContextSaveDesc
                        ( /* pNewContextSaveDesc */ &pT->contextSaveDesc
                        , /* stackPointer */ (uint8_t*)pT->pStackArea + pT->stackSize
                        , /* fctEntryIntoContext */(int_fctEntryIntoContext_t)pT->taskFunction
                        , /* privilegedMode */ true
                        );
#ifdef DEBUG
# if RTOS_NO_TASKS <= 3
        {
            iprintf( "Task %u:\r\n"
                     "Stack pointer: 0x%p\r\n"
                   , idxTask
                   , pT->contextSaveDesc.pStack
                   );

            unsigned int u;
            const unsigned int noWords = pT->stackSize / sizeof(uint32_t);
            for(u=0; u<noWords; ++u)
            {
                if(u%4 == 0)
                {
                    iprintf( "\r\n%4u, %p:\t"
                           , u
                           , (uint32_t*)(pT->pStackArea) + u
                           );

                    /* Leave printf the time to flush the buffer. */
                    del_delayMicroseconds(5000 /* us */);
                }
                iprintf("%08lx\t", ((uint32_t*)(pT->pStackArea))[u]);
            }
            iprintf("\r\n");
        }
# endif
#endif

#if RTOS_ROUND_ROBIN_MODE_SUPPORTED == RTOS_FEATURE_ON
        /* The round robin counter is loaded to its maximum when the tasks becomes due.
           Now, the value doesn't matter. */
        pT->cntRoundRobin = 0;
#endif
        /* No events have been posted to this task yet. */
        pT->postedEventVec = 0;

        /* Initialize overrun counter. */
        pT->cntOverrun = 0;

        /* Any task is suspended at the beginning. No task is active, see before. If
           mutexes or semaphores are in use this list is sorted with decreasing priority.
             Note Daijie Zhang <zdaijie@gmail.com> (see
           http://forum.arduino.cc/index.php?topic=138643 as of Jan 8, 2016): The
           preprocessor condition was wrong in
           https://vranken@svn.code.sf.net/p/rtuinos/code/trunk/code/RTOS/rtos.c?&r=1. This
           let to wrong first assignment of free semphores to tasks of different priority
           and has been fixed in r2. */
#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_OFF  &&   RTOS_USE_MUTEX == RTOS_FEATURE_OFF
        _pSuspendedTaskAry[idxTask] = pT;
#else
        signed int idxPos;
        for(idxPos=0; idxPos<(int)idxTask; ++idxPos)
            if(_pSuspendedTaskAry[idxPos]->prioClass < pT->prioClass)
                break;

        /* Shift rest of the list one to the end. This loop requires a signed index. */
        signed int i;
        for(i=(int)idxTask-1; i>=idxPos; --i)
            _pSuspendedTaskAry[i+1] = _pSuspendedTaskAry[i];

        /* Insert the suspended task at the priority determined right position. */
        _pSuspendedTaskAry[idxPos] = pT;
#endif
    } /* for(All tasks to initialize) */

    /* Number of currently suspended tasks: All. */
    _noSuspendedTasks = RTOS_NO_TASKS;

    /* The idle task is stored in the last array entry. It differs, there's e.g. no task
       function defined. We mainly need the storage location for the stack pointer.
         The information of the stack area is retrieved from the linker; the according
       symbols are declared in the linker command file. Having this information doesn't
       cost anything but enables to apply the stack reserve computation for the idle task,
       too. */
    extern uint32_t ld_memStackStart[0], ld_memStackSize[0];

    pT = _pIdleTask;
    
    /* The context save descriptor of the idle task needs to be initialized, too. It is
       used when we leave and later resume the idle context.
         Note, the parameters besides the reference to the initzialized object don't care
       in this situation; the startup context is already created and started and all of
       this has already been decided. */
    ccx_createContextSaveDescOnTheFly( &pT->contextSaveDesc
                                     , /* stackPointer */ NULL
                                     , /* fctEntryIntoOnTheFlyStartedContext */ NULL
                                     , /* privilegedMode */ true
                                     );

    pT->timeDueAt = 0;              /* Not used at all. */
#if RTOS_ROUND_ROBIN_MODE_SUPPORTED == RTOS_FEATURE_ON
    pT->cntRoundRobin = 0;          /* Not used at all. */
#endif
    pT->pStackArea = ld_memStackStart;          /* Used for stack reserve computation. */
    pT->stackSize = (uint32_t)ld_memStackSize;  /* Not used at all. */
    pT->cntDelay = 0;               /* Not used at all. */

    /* The next element always needs to be 0. Otherwise any interrupt or a call of
       sendEvent would corrupt the stack assuming that a suspend command would require a
       return code. */
    pT->postedEventVec = 0;

    pT->eventMask = 0;              /* Not used at all. */
    pT->waitForAnyEvent = true;     /* Not used at all. */
    pT->cntOverrun = 0;             /* Not used at all. */

    /* Any task is suspended at the beginning. No task is active, see before. */
    for(idxClass=0; idxClass<RTOS_NO_PRIO_CLASSES; ++idxClass)
        _noDueTasksAry[idxClass] = 0;
    _pActiveTask    = _pIdleTask;
    _pSuspendedTask = _pIdleTask;

    /* All data is prepared and the kernel is ready to react on task switch demands. Being
       in this state, we can let the application install kernel interrupts. */
    setupAfterKernelInit();
    
    /* All data is prepared. Let's start the IRQ which clocks the system time. */
    enableIRQTimerTick();

#if RTOS_USE_APPL_INTERRUPT_00 == RTOS_FEATURE_ON
    /* Install the interrupt handler for application defined interrupt 0. */
    ihw_installINTCInterruptHandler
                        ( (int_externalInterruptHandler_t){.kernelIsr = isrUser00}
                        , /* vectorNum */ RTOS_ISR_USER_00
                        , /* psrPriority */ 1
                        , /* isPreemptable */ true
                        , /* isOsInterrupt */ true
                        );
    /* Call the application to let it configure the interrupt source. */
    rtos_enableIRQUser00();
#endif

#if RTOS_USE_APPL_INTERRUPT_01 == RTOS_FEATURE_ON
    /* Install the interrupt handler for application defined interrupt 1. */
    ihw_installINTCInterruptHandler
                        ( (int_externalInterruptHandler_t){.kernelIsr = isrUser01}
                        , /* vectorNum */ RTOS_ISR_USER_01
                        , /* psrPriority */ 1
                        , /* isPreemptable */ true
                        , /* isOsInterrupt */ true
                        );
    /* Call the application to let it configure the interrupt source. */
    rtos_enableIRQUser01();
#endif

    /* From here, all further code implicitly becomes the idle task. */
    setupAfterSystemTimerInit();
    while(true)
        loop();

} /* End of rtos_initRTOS */




/// @todo Remove test code
#if defined(RTOS_ISR_SYSTEM_TIMER_TIC) || defined(RTOS_ISR_SYSTEM_TIMER_TICK)
# error Remove Arduino relict in rtos.config.h
#endif
