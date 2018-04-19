/**
 * @file mai_main.c
 * The main entry point of the C code. In this sample it configures and runs the RTOS.
 * Some tasks are registered that implement blinking LEDs and more.\n
 *   A cyclic 1ms task controls one LED such that it blinks at 1 Hz. The task reads the
 * state of the buttons on the evaluation board. On button press an according event task,
 * taskOnButtonDown, is activated.\n
 *   The event task taskOnButtonDown reports each button event by printing a message to the
 * serial COM channel. At the same time it increments the amount of CPU load by 10%, load
 * which is (artificially) produced by task taskCpuLoad. This is a cyclic task with a busy
 * wait loop.\n
 *   A cyclic 1000ms task toggles the second LED at a rate of 0.5 Hz.\n
 *   An event task taskNonCyclic is activated by several other tasks under different
 * conditions. It can be observed that the activation sometime succeeds and sometime fails
 * - depending on these conditions.\n
 *   The regular 1s task is used to report the system state, CPU load, stack usage and task
 * overrun events.\n
 *   The idle task measures the CPU load.\n
 *   Three timer interrupts fire at high speed and on a time grid, which is asynchronous to
 * the normal application tasks. This leads to most variable preemption patterns. The
 * interrupts do nothing but producing system load and one of them participates the
 * software self-test (consistency check of shared data).
 *
 * The application should be run with a connected terminal. The terminal should be
 * configured for 115200 Bd, 8 Bits, no parity, 1 start and 1 stop bit.
 *
 * Some observations:\n
 *   Blinking LEDs: Note the slight phase shift due to the differing task start times.\n
 *   Reported CPU load: At nominal 100% artificial load it drops to about 50%. The
 * execution time of the cyclic task that produces the load exceeds the nominal cycle time
 * of the task and every second activation is lost. The activation loss counter in the
 * RTOS' task array constantly increases. (RTuinOS port: RTuinOS stays at 100% load as it
 * shifts the task activation if it is timely not possible.)\n
 *   Occasional activation losses can be reported for task taskNonCyclic. It can be
 * preempted by task task17ms and this task activates task taskNonCyclic. If it tries to
 * do, while it has preempted task taskNonCyclic, the activation is not possible.\n
 *   The code runs a permanent test of the different offered mechanisms of mutual exclusion
 * of tasks that access some shared data objects. A recognized failure is reported by
 * assertion, which will halt the code execution (in DEBUG compilation only). Everything is
 * fine as long as the LEDs continue blinking.
 *
 * Copyright (C) 2017-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   main
 * Local functions
 *   checkAndIncrementTaskCnts
 *   testPCP
 *   isrPid0
 *   isrPid1
 *   isrPid2
 *   task1ms
 *   task3ms
 *   task1s
 *   taskNonCyclic
 *   task17ms
 *   taskOnButtonDown
 *   taskCpuLoad
 *   installInterruptServiceRoutines
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <assert.h>

#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
#include "del_delay.h"
#include "gsl_systemLoad.h"
#include "rtos.h"
#include "tc16_mutualExclusion.h"
#include "tc16_applEvents.h"
#include "mai_main.h"

/*
 * Defines
 */

/** The demo can be compiled with a ground load. Most tasks produce some CPU load if this
    switch is set to 1. (Due to the high interrupt load, this will already cause task
    overruns.) */
#define TASKS_PRODUCE_GROUND_LOAD   0

/** The test of the priority ceiling protocol counts recognized errors, which are reproted
    by console output and to the debugger. Alternatively and by setting this macro to 1,
    the software execution can be halted on recognition of the first error. The no longer
    blinking LEDs would indicate the problem. */
#define HALT_ON_PCP_TEST_FAILURE    1

/** A wrapper around the API for the priority ceiling protocol (PCP), which lets the API
    for mutual exclusion of a task set look like the API calls from the OSEK/VDX standard.
      Here, for getting the resource, i.e. for entering a critical section of code.
      @remark Note, in the RTuinOS port of this test, the macro argument \a resource is not
    used. Always all competing kernel interrupts are excluded and thereby all other tasks.
      @remark In RTuinOS, the function pair rtos_enterCriticalSection() and
    rtos_leaveCriticalSection(), which is used to implement this macro, is not nestable. We
    could implement an invocation counter in this macro, but the test doesn't use it
    nested anyway. */
#define GetResource() { rtos_enterCriticalSection();

/** A wrapper around the API for the priority ceiling protocal (PCP), which lets the API
    for mutual exclusion of a task set look like the API calls from the OSEK/VDX standard.
      Here, for returning the resource, i.e. for leaving a critical section of code. */
#define ReleaseResource() rtos_leaveCriticalSection(); }

/** The stack size needs to be at minimum N times the size of the ISR stack frame, where N
    is the number of interrupt priorities in use, plus the size of the stack frame for the
    system call interrupt handler. In this sample we set N=4 (2 in serial and one for the
    RTuinOS system timer, one as reserve) and the stack frame size is S_I_StFr=168 Byte and
    S_SC_StFr=104 Byte.\n
      This does not yet include the stack consumption of the implementation of the task
    itself.\n
      Note, the number of uint32 words in the stack needs to be even, otherwise the
    implementation of the 8 Byte alignment for the initial stack pointer value is wrong
    (checked by assertion). */
#define NO_IRQ_LEVELS           4
#define STACK_USAGE_IN_BYTE     4000

/** The stack size in byte is derived from #STACK_USAGE_IN_BYTE and #NO_IRQ_LEVELS.
    Alignment constraints are considered in the computation. */
#define STACK_SIZE_TASK_IN_BYTE ((((NO_IRQ_LEVELS)*(S_I_StFr)+(S_SC_StFr)+STACK_USAGE_IN_BYTE)+7)&~7)


/*
 * Local type definitions
 */

/** The enumeration of all tasks; the values are the task IDs. Actually, the ID is provided
    by the RTOS at runtime, when registering the task. However, it is guaranteed that the
    IDs, which are dealt out by rtos_registerTask() form the series 0, 1, 2, ..., 7. So
    we don't need to have a dynamic storage of the IDs; we define them as constants and
    double-check by assertion that we got the correct, expected IDs. Note, this requires
    that the order of registering the tasks follows the order here in the enumeration. */
enum
{
    /** The ID of the registered application tasks. They occupy the index range starting
        from zero. */
    idxTask1ms = 0
    , idxTask3ms
    , idxTask1s
    , idxTaskNonCyclic
    , idxTask17ms
    , idxTaskOnButtonDown
    , idxTaskCpuLoad
    
    /** The number of tasks to register. */
    , noRegisteredTasks

    /** The idle task is not a task under control of the RTOS and it doesn't have an ID.
        We assign it a pseudo task ID that is used to store some task related data in the
        same array here in this sample application as we do by true task ID for all true
        tasks. */
    , idxTaskIdle = noRegisteredTasks
    
    /** The interrupts that are applied mainly to produce system load for testing, continue
        the sequence of ids, so that they can share the shared data container with test
        data. */
    , idISRPid0
    , idISRPid1
    , idISRPid2
    
    /** The number of all concurrent excution threads: The ISRs, the application tasks and
        the idle task. */
    , noExecutionContexts
    
    /** The number of ISRs. */
    , noISRs = noExecutionContexts - noRegisteredTasks - 1
};


/** The RTOS uses constant task priorities, which are defined here. (The concept and
    architecture of the RTOS allows dynamic changeing of a task's priority at runtime, but
    we didn't provide an API for that yet; where are the use cases?) */
enum
{
    prio_RTOS_Task1ms = 1
    , prio_RTOS_Task3ms = 2
    , prio_RTOS_Task1s = 0
    , prio_RTOS_TaskNonCyclic = 2
    , prio_RTOS_Task17ms = 3
    , prio_RTOS_TaskOnButtonDown = 0
    , prio_RTOS_TaskCpuLoad = 0
    
    , prio_INTC_ISRPid0 = 5
    , prio_INTC_ISRPid1 = 6
    , prio_INTC_ISRPid2 = 15
};



/*
 * Local prototypes
 */


/*
 * Data definitions
 */

/** A task invokation counter, which is incremented by all application tasks. */
volatile unsigned long long _cntAllTasks = 0;

/** A cycle counter for each task. The last entry is meant for the idle task. */
volatile unsigned long long _cntTaskAry[noExecutionContexts] =
                                                        {[0 ... noExecutionContexts-1] = 0};

volatile unsigned long mai_cntTaskIdle = 0  /** Counter of cycles of infinite main loop. */
                     , mai_cntTask1ms = 0   /** Counter of cyclic task. */
                     , mai_cntTask3ms = 0   /** Counter of cyclic task. */
                     , mai_cntTask1s = 0    /** Counter of cyclic task. */
                     , mai_cntTaskNonCyclic = 0  /** Counter of calls of software triggered
                                                     task */
                     , mai_cntTask17ms = 0  /** Counter of cyclic task. */
                     , mai_cntTaskOnButtonDown = 0  /** Counter of button event task. */
                     , mai_cntTaskCpuLoad = 0   /** Counter of cyclic task. */
                     , mai_cntISRPid0 = 0
                     , mai_cntISRPid1 = 0
                     , mai_cntISRPid2 = 0;

/** The color currently used by the interrupt handlers are controlled through selection of
    a pin. The selection is made by global variable. Here for D5. */
static volatile lbd_led_t _ledTask1s  = lbd_led_D5_grn;

/** The color currently used by the interrupt handlers are controlled through selection of
    a pin. The selection is made by global variable. Here for D4. */
static volatile lbd_led_t _ledTask1ms = lbd_led_D4_red;

/** The average CPU load produced by all tasks and interrupts in tens of percent. */
unsigned int mai_cpuLoad = 1000;

/** Test of CPU load estimation: This variable controls the production of some artificial
    CPU load. This is done in a task of low priority so that all higher prioritized task
    should not or barely be affected. (One LED is, the other isn't affected.) */
static unsigned int _cpuLoadInPercent = 0;

/** Test of priority ceiling protocol. A sub-set of tasks, whereof non of highest priority
    in use, share this data object. Is has redundant fields so that a sharing conflict can
    be recognized. Try compiling the code with bad resource definition and see if the
    problem is reported (in DEBUG compilation by assertion, too). */
static volatile struct sharedDataTasksIdleAnd1msAndCpuLoad_t
{
    /** Counter incremented on execution of task task1ms. */
    unsigned int cntTask1ms;
    
    /** Counter incremented on execution of task taskCpuLoad. */
    unsigned int cntTaskCpuLoad;
    
    /** Counter incremented on execution of the idle task. */
    unsigned int cntTaskIdle;
    
    /** Total count, sum of all others. */
    unsigned int cntTotal;
    
    /** The number of recognized data consistency errors. */
    unsigned int noErrors;
    
} _sharedDataTasksIdleAnd1msAndCpuLoad =
    { .cntTask1ms = 0
    , .cntTaskCpuLoad = 0
    , .cntTaskIdle = 0
    , .cntTotal = 0
    , .noErrors = 0
    };
    
/** The names of the tasks, where the arry index is the task ID. */
static const char *_taskNameAry[noRegisteredTasks+1] =
{
  "task1ms"
  , "task3ms"
  , "task1s"
  , "taskNonCyclic"
  , "task17ms"
  , "taskOnButtonDown"
  , "taskCpuLoad"
  , "taskIdle"
};



/*
 * Function implementation
 */

/**
 * Test function, to be called from any of the tasks: A task related counter is incremented
 * and in the same atomic operation is a task-shared counter incremented. The function then
 * validates that the sum of all task related counters is identical to the value of the
 * shared counter. The test result is validated by assertion, i.e. the application is
 * halted in case of an error.
 *   The test is aimed to prove the correct implementation of the offered mutual exclusion
 * mechanisms.
 *   @param idxTask
 * The ID (or index) of the calling task. Needed to identify the task related counter.
 */
static void checkAndIncrementTaskCnts(unsigned int idxTask)
{
    assert(idxTask < sizeOfAry(_cntTaskAry));
    
    /* Increment task related counter and shared counter in an atomic operation.
         RTuinOS port of code: Only the tasks but not the interrupts are mutually excluded
       by macro GetResource. We have cannot use the macro in this function, which shares
       data with the interrupts. */
    uint32_t msr = ihw_enterCriticalSection();
    {
        ++ _cntTaskAry[idxTask];
        ++ _cntAllTasks;
    }
    ihw_leaveCriticalSection(msr);

    /* Get all task counters and the common counter in an atomic operation. Now, we apply
       another offered mechanism for mutual exclusion of tasks.
         RTuinOS port of code: It's the same mechanism. See above: There we had to
       substitute the GetResource(). */
    unsigned long long cntTaskAryCpy[noExecutionContexts]
                     , cntAllTasksCpy;
    _Static_assert(sizeof(_cntTaskAry) == sizeof(cntTaskAryCpy), "Bad data definition");
    msr = ihw_enterCriticalSection();
    {
        memcpy(&cntTaskAryCpy[0], (void*)&_cntTaskAry[0], sizeof(cntTaskAryCpy));
        cntAllTasksCpy = _cntAllTasks;
    }
    ihw_leaveCriticalSection(msr);

    /* Check consistency of got data. */
    unsigned int u;
    for(u=0; u<noExecutionContexts; ++u)
        cntAllTasksCpy -= cntTaskAryCpy[u];
#ifdef DEBUG
    assert(cntAllTasksCpy == 0);
#else
    /* PRODUCTION compilation: Code execution is halted, LEDs stop blinking as indication
       of a severe problem. */
    if(cntAllTasksCpy != 0)
    {
        ihw_suspendAllInterrupts();
        while(true)
            ;
    }
#endif

    /* Get all task counters and the common counter again in an atomic operation. Now, we
       apply the third offered mechanism for mutual exclusion of tasks to include it into
       the test.
         Note, this code requires that we are not yet inside a critical section; it's a
       non-nestable call. */
    ihw_suspendAllInterrupts();
    {
        memcpy(&cntTaskAryCpy[0], (void*)&_cntTaskAry[0], sizeof(cntTaskAryCpy));
        cntAllTasksCpy = _cntAllTasks;
    }
    ihw_resumeAllInterrupts();

    /* Check consistency of got data. */
    for(u=0; u<noExecutionContexts; ++u)
        cntAllTasksCpy -= cntTaskAryCpy[u];
#ifdef DEBUG
    assert(cntAllTasksCpy == 0);
#else
    if(cntAllTasksCpy != 0)
    {
        ihw_suspendAllInterrupts();
        while(true)
            ;
    }
#endif
} /* End of checkAndIncrementTaskCnts. */




/**
 * Test function for priority ceiling protocol. To be called from a sub-set of tasks, idle
 * task, task1ms and taskCpuLoad.
 *   The test is aimed to prove the correct implementation of the offered mutual exclusion
 * mechanism for this sub-set of tasks.
 *   @param idxTask
 * The ID (or index) of the calling task. Needed to identify the task related counter.
 *   @remark
 * RTuinOS port of code: RTuinOS doesn't offer PCP for sub-sets of tasks but uses it to
 * selectively lock kernel interrupts and not all interrupts. The test is still useful and
 * justified although it no longer tests what it intentionally was made for.
 */
static void testPCP(unsigned int idxTask)
{
    /* Increment task related counter and shared counter in an atomic operation. */
    if(idxTask == idxTaskIdle)
    {
        GetResource();
        {
            ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTaskIdle;
            ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTotal;
        }
        ReleaseResource();
    }
    else if(idxTask == idxTaskCpuLoad)
    {
        GetResource();
        {
            ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTaskCpuLoad;
            ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTotal;
        }
        ReleaseResource();
    }
    else if(idxTask == idxTask1ms)
    {
        GetResource();
        {
            ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTask1ms;
            ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTotal;
        }
        ReleaseResource();
    }
    else
    {
        /* This function is intended only for a sub-set of tasks. */
#ifdef DEBUG
        assert(false);
# else
        ihw_suspendAllInterrupts();
        while(true)
            ;
#endif
    }

    /* Validate the consistency of the redundant data in an atomic operation. */
    GetResource();
    {
        const unsigned int sum = _sharedDataTasksIdleAnd1msAndCpuLoad.cntTaskIdle
                                 + _sharedDataTasksIdleAnd1msAndCpuLoad.cntTaskCpuLoad
                                 + _sharedDataTasksIdleAnd1msAndCpuLoad.cntTask1ms;
        if(sum != _sharedDataTasksIdleAnd1msAndCpuLoad.cntTotal)
        {
            /* Resynchronize to enable further error recognition. */
            _sharedDataTasksIdleAnd1msAndCpuLoad.cntTotal = sum;
            
            const unsigned int noErr = _sharedDataTasksIdleAnd1msAndCpuLoad.noErrors+1;
            if(noErr > 0)
                _sharedDataTasksIdleAnd1msAndCpuLoad.noErrors = noErr;
           
            /* On desire, the application is halted. This makes the error observable
               without connected terminal. */
#if HALT_ON_PCP_TEST_FAILURE == 1
# ifdef DEBUG
            assert(false);
# else
            ihw_suspendAllInterrupts();
            while(true)
                ;
# endif
#endif
        }
    }
    ReleaseResource();

} /* End of testPCP. */



/**
 * A regularly triggered interrupt handler for the timer PID0. The interrupt does nothing
 * but counting a variable. The ISR participates the test of safely sharing data with the
 * application tasks. It is triggered at medium frequency and asynchronously to the
 * kernel's clock tick to prove the system stability and properness of the context
 * switches.
 */
static void isrPid0()
{
    checkAndIncrementTaskCnts(idISRPid0);
    ++ mai_cntISRPid0;
    
    /* Acknowledge the interrupt in the causing HW device. */
    PIT.TFLG0.B.TIF = 0x1;

} /* End of isrPid0 */



/**
 * A regularly triggered interrupt handler for the timer PID1. The interrupt does nothing
 * but counting a variable. It is triggered at high frequency and asynchronously to the
 * kernel's clock tick to prove the system stability and properness of the context switches.
 */
static void isrPid1()
{
    ++ mai_cntISRPid1;
    
    /* Acknowledge the interrupt in the causing HW device. */
    PIT.TFLG1.B.TIF = 0x1;

} /* End of isrPid1 */



/**
 * A regularly triggered interrupt handler for the timer PID2. The interrupt does nothing
 * but counting a variable. It is triggered at high frequency and asynchronously to the
 * kernel's clock tick to prove the system stability and properness of the context switches.
 */
static void isrPid2()
{
    ++ mai_cntISRPid2;
    
    /* Acknowledge the interrupt in the causing HW device. */
    PIT.TFLG2.B.TIF = 0x1;

} /* End of isrPid2 */



/**
 * Task function, cyclically activated every Millisecond. The LED D4 is switched on and off
 * and the button SW3 is read and evaluated.\n
 *   In RTuinOS such a cyclic behavior is implemented by an infinite loop, always
 * waiting for the absolute timer event.
 *   @param initialResumeCondition
 * The set of events, which made this task initially ready.
 */
static _Noreturn void task1ms(uint32_t initialResumeCondition ATTRIB_DBG_ONLY)
{
    assert(initialResumeCondition == RTOS_EVT_DELAY_TIMER);
    while(true)
    {
        checkAndIncrementTaskCnts(idxTask1ms);
        testPCP(idxTask1ms);

        ++ mai_cntTask1ms;

        /* Activate the non cyclic task.
             Note, the non cyclic task is of higher priority than this task and it'll be
           executed immediately, preempting this task. The second activation below, on button
           down must not lead to an activation loss. */
        rtos_sendEvent(EVT_ACTIVATE_TASK_NON_CYCLIC);

#if TASKS_PRODUCE_GROUND_LOAD == 1
        /* Produce a bit of CPU load. This call simulates some true application software. */
        del_delayMicroseconds(/* fullLoadThisNoMicroseconds */ 50 /* approx. 5% load */);
#endif    

        /* Read the current button status to possibly toggle the LED colors. */
        static bool lastStateButton_ = false;
        if(lbd_getButton(lbd_bt_button_SW3))
        {
            if(!lastStateButton_)
            {
                /* Button down event: Toggle colors */
                static unsigned int cntButtonPress_ = 0;

                lbd_setLED(_ledTask1s, /* isOn */ false);
                lbd_setLED(_ledTask1ms, /* isOn */ false);
                _ledTask1s  = (cntButtonPress_ & 0x1) != 0? lbd_led_D5_red: lbd_led_D5_grn;
                _ledTask1ms = (cntButtonPress_ & 0x2) != 0? lbd_led_D4_red: lbd_led_D4_grn;

                /* Activate the non cyclic task a second time. The priority of the activated
                   task is higher than of this activating task so the first activation should
                   have been processed meanwhile and this one should be accepted, too. */
                rtos_sendEvent(EVT_ACTIVATE_TASK_NON_CYCLIC);

                /* Activate our button down event task. The activation will normally succeed
                   but at high load and very fast button press events it it theoretically
                   possible that not. */
                rtos_sendEvent(EVT_ACTIVATE_TASK_ON_BUTTON_DOWN);

                lastStateButton_ = true;
                ++ cntButtonPress_;
            }
        }
        else
            lastStateButton_ = false;

        static int cntIsOn_ = 0;
        if(++cntIsOn_ >= 500)
            cntIsOn_ = -500;
        lbd_setLED(_ledTask1ms, /* isOn */ cntIsOn_ >= 0);
    
        /* Wait for next task activation. */
#ifdef DEBUG
        uint32_t eventMask =
#endif
        rtos_waitForEvent( /* eventMask */ RTOS_EVT_ABSOLUTE_TIMER
                         , /* all */ false
                         , /* timeout */ 1 /* ms */
                         );
        assert(eventMask == RTOS_EVT_ABSOLUTE_TIMER);
    }
} /* End of task1ms */




/**
 * Task function, cyclically activated every 3ms.\n
 *   In RTuinOS such a cyclic behavior is implemented by an infinite loop, always
 * waiting for the absolute timer event.
 *   @param initialResumeCondition
 * The set of events, which made this task initially ready.
 */
static _Noreturn void task3ms(uint32_t initialResumeCondition ATTRIB_DBG_ONLY)
{
    assert(initialResumeCondition == RTOS_EVT_DELAY_TIMER);
    while(true)
    {
        checkAndIncrementTaskCnts(idxTask3ms);
        ++ mai_cntTask3ms;

#if TASKS_PRODUCE_GROUND_LOAD == 1
        /* Produce a bit of CPU load. This call simulates some true application software. */
        del_delayMicroseconds(/* fullLoadThisNoMicroseconds */ 150 /* approx. 5% load */);
#endif    
    
        /* Wait for next task activation. */
#ifdef DEBUG
        uint32_t eventMask =
#endif
        rtos_waitForEvent( /* eventMask */ RTOS_EVT_ABSOLUTE_TIMER
                         , /* all */ false
                         , /* timeout */ 3 /* ms */
                         );
        assert(eventMask == RTOS_EVT_ABSOLUTE_TIMER);
    }
} /* End of task3ms */



/**
 * Task function, cyclically activated every second.\n
 *   In RTuinOS such a cyclic behavior is implemented by an infinite loop, always
 * waiting for the absolute timer event.
 *   @param initialResumeCondition
 * The set of events, which made this task initially ready.
 */
static _Noreturn void task1s(uint32_t initialResumeCondition ATTRIB_DBG_ONLY)
{
    assert(initialResumeCondition == RTOS_EVT_DELAY_TIMER);
    while(true)
    {
        checkAndIncrementTaskCnts(idxTask1s);

        ++ mai_cntTask1s;

        static int cntIsOn_ = 0;
        if(++cntIsOn_ >= 1)
            cntIsOn_ = -1;
        lbd_setLED(_ledTask1s, /* isOn */ cntIsOn_ >= 0);

#if TASKS_PRODUCE_GROUND_LOAD == 1
        /* Produce a bit of CPU load. This call simulates some true application software.
             Note, the cyclic task taskCpuLoad has a period time of 23 ms and it has the
           same priority as this task. Because of the busy loop here and because the faster
           task itself has a non negligible execution time, there's a significant chance of
           loosing an activation of the faster task once a second. */
        del_delayMicroseconds(/* fullLoadThisNoMicroseconds */ 20000 /* approx. 2% load */);
#endif    
        /* Most critical for safe operation of the RTOS based software is the stack
           reserve. It can last a very, very, very long time until all interrupts preempt
           eachother and that this happens, when the task inside its deepest nested
           subroutine. No chance for testers... We display the worst stack reserve. */
        unsigned int idxTask
                   , idxWorstTask = 0
                   , idxSafestTask = 0
                   , minStackReserve = UINT_MAX
                   , maxStackReserve = 0;
        /* +1: Idle task */
        for(idxTask=0; idxTask<noRegisteredTasks+1; ++idxTask)
        {
            const unsigned int stackReserve = rtos_getStackReserve(idxTask);
            if(stackReserve < minStackReserve)
            {
                idxWorstTask = idxTask;
                minStackReserve = stackReserve;
            }
            if(stackReserve > maxStackReserve)
            {
                idxSafestTask = idxTask;
                maxStackReserve = stackReserve;
            }
        } /* End for(All tasks) */
        
        /* Simple code: First calculation of time to print is wrong. */
        static unsigned long tiPrintf = 0;
        unsigned long long tiFrom = GSL_PPC_GET_TIMEBASE();
        iprintf( "CPU load is %u.%u%%. Stack reserve min/max: %u Byte (%s)/%u Byte (%s).\r\n"
                 "Task activations (lost):\r\n"
                 "  task1ms: %lu (%u)\r\n"
                 "  task3ms: %lu (%u)\r\n"
                 "  task1s: %lu (%u)\r\n"
                 "  taskNonCyclic: %lu (%u)\r\n"
                 "  task17ms: %lu (%u)\r\n"
                 "  taskOnButtonDown: %lu (%u)\r\n"
                 "  taskCpuLoad: %lu (%u)\r\n"
                 "  taskIdle: %lu\r\n"
                 "  tiPrintf = %luus\r\n"
               , mai_cpuLoad/10, mai_cpuLoad%10
               , minStackReserve, _taskNameAry[idxWorstTask]
               , maxStackReserve, _taskNameAry[idxSafestTask]
               , mai_cntTask1ms, rtos_getTaskOverrunCounter(idxTask1ms, /* doReset */ false)
               , mai_cntTask3ms, rtos_getTaskOverrunCounter(idxTask3ms, /* doReset */ false)
               , mai_cntTask1s, rtos_getTaskOverrunCounter(idxTask1s, /* doReset */ false)
               , mai_cntTaskNonCyclic, rtos_getTaskOverrunCounter( idxTaskNonCyclic
                                                                 , /* doReset */ false
                                                                 )
               , mai_cntTask17ms, rtos_getTaskOverrunCounter(idxTask17ms, /* doReset */ false)
               , mai_cntTaskOnButtonDown, rtos_getTaskOverrunCounter( idxTaskOnButtonDown
                                                                    , /* doReset */ false
                                                                    )
               , mai_cntTaskCpuLoad, rtos_getTaskOverrunCounter( idxTaskCpuLoad
                                                               , /* doReset */ false
                                                               )
               , mai_cntTaskIdle
               , tiPrintf
               );
        tiPrintf = (unsigned long)(GSL_PPC_GET_TIMEBASE() - tiFrom) / 120;

    
        /* Wait for next task activation. */
#ifdef DEBUG
        uint32_t eventMask =
#endif
        rtos_waitForEvent( /* eventMask */ RTOS_EVT_ABSOLUTE_TIMER
                         , /* all */ false
                         , /* timeout */ 1000 /* ms */
                         );
        assert(eventMask == RTOS_EVT_ABSOLUTE_TIMER);
    }
} /* End of task1s */



/**
 * A non cyclic task, which is solely activated by software triggers from other tasks.\n
 *   In RTuinOS such a non cyclic behavior is implemented by an infinite loop, always
 * waiting for the trigger event, which is sent by the controlling task or ISR.
 *   @param initialResumeCondition
 * The set of events, which made this task initially ready.
 */
static _Noreturn void taskNonCyclic(uint32_t initialResumeCondition ATTRIB_DBG_ONLY)
{
    assert(initialResumeCondition == EVT_ACTIVATE_TASK_NON_CYCLIC);
    while(true)
    {
        checkAndIncrementTaskCnts(idxTaskNonCyclic);
        ++ mai_cntTaskNonCyclic;

        /* Wait for next activation. */
#ifdef DEBUG
        uint32_t eventMask =
#endif
        rtos_waitForEvent( /* eventMask */ EVT_ACTIVATE_TASK_NON_CYCLIC
                         , /* all */ false
                         , /* timeout */ 0
                         );
        assert(eventMask == EVT_ACTIVATE_TASK_NON_CYCLIC);
    }
} /* End of taskNonCyclic */



/**
 * Task function, cyclically activated every 17ms.\n
 *   In RTuinOS such a cyclic behavior is implemented by an infinite loop, always
 * waiting for the absolute timer event.
 *   @param initialResumeCondition
 * The set of events, which made this task initially ready.
 */
static _Noreturn void task17ms(uint32_t initialResumeCondition ATTRIB_DBG_ONLY)
{
    assert(initialResumeCondition == RTOS_EVT_DELAY_TIMER);
    while(true)
    {
        checkAndIncrementTaskCnts(idxTask17ms);
        ++ mai_cntTask17ms;

        /* This task has a higher priority than the software triggered, non cyclic task. Since
           the latter one is often active we have a significant likelihood of a failing
           activation from here -- always if we preempted the non cyclic task. */
        rtos_sendEvent(EVT_ACTIVATE_TASK_NON_CYCLIC);

#if TASKS_PRODUCE_GROUND_LOAD == 1
        /* Produce a bit of CPU load. This call simulates some true application software. */
        del_delayMicroseconds(/* fullLoadThisNoMicroseconds */ 17*40 /* approx. 4% load */);
#endif    

        /* A task can't activate itself, we do not queue activations and it's obviously
           active at the moment. Try it.
             RTuinOS port of test: RTuinOS doesn't give feedback about consumption of
           simple, broadcasted events. */
        rtos_sendEvent(EVT_ACTIVATE_TASK_17_MS);

        /* Wait for next activation. */
#ifdef DEBUG
        uint32_t eventMask =
#endif
        rtos_waitForEvent( /* eventMask */ RTOS_EVT_ABSOLUTE_TIMER | EVT_ACTIVATE_TASK_17_MS
                         , /* all */ false
                         , /* timeout */ 17 /* ms */
                         );
        assert((eventMask & (RTOS_EVT_ABSOLUTE_TIMER | EVT_ACTIVATE_TASK_17_MS)) != 0);
    }
} /* End of task17ms */



/**
 * A non cyclic task, which is activated by software trigger every time the button on the
 * evaluation board is pressed.\n
 *   In RTuinOS such a non cyclic behavior is implemented by an infinite loop, always
 * waiting for the trigger event, which is sent by the controlling task or ISR.
 *   @param initialResumeCondition
 * The set of events, which made this task initially ready.
 */
static _Noreturn void taskOnButtonDown(uint32_t initialResumeCondition ATTRIB_DBG_ONLY)
{
    assert(initialResumeCondition == EVT_ACTIVATE_TASK_ON_BUTTON_DOWN);
    while(true)
    {
        checkAndIncrementTaskCnts(idxTaskOnButtonDown);
        ++ mai_cntTaskOnButtonDown;
        iprintf("You pressed the button the %lu. time\r\n", mai_cntTaskOnButtonDown);

        /* Change the value of artificial CPU load on every click by 10%. */
        if(_cpuLoadInPercent < 100)
            _cpuLoadInPercent += 10;
        else
            _cpuLoadInPercent = 0;

        iprintf( "The additional, artificial CPU load has been set to %u%%\r\n"
               , _cpuLoadInPercent
               );
    
        /* Wait for next activation. */
#ifdef DEBUG
        uint32_t eventMask =
#endif
        rtos_waitForEvent( /* eventMask */ EVT_ACTIVATE_TASK_ON_BUTTON_DOWN
                         , /* all */ false
                         , /* timeout */ 0
                         );
        assert(eventMask == EVT_ACTIVATE_TASK_ON_BUTTON_DOWN);
    }
} /* End of taskOnButtonDown */



/**
 * A cyclic task of low priority, which is used to produce some artificial CPU load.\n
 *   In RTuinOS a cyclic task behavior is implemented by an infinite loop, always
 * waiting for the absolute timer event.
 *   @remark
 * We need to consider that in this sample, the measurement is inaccurate because the
 * idle loop is not empty (besides measuring the load) and so the observation window is
 * discontinuous. The task has a cycle time of much less than the CPU measurement
 * observation window, which compensates for the effect of the discontinuous observation
 * window.
 *   @param initialResumeCondition
 * The set of events, which made this task initially ready.
 */
static _Noreturn void taskCpuLoad(uint32_t initialResumeCondition ATTRIB_DBG_ONLY)
{
    assert(initialResumeCondition == RTOS_EVT_DELAY_TIMER);
    while(true)
    {
        checkAndIncrementTaskCnts(idxTaskCpuLoad);
        testPCP(idxTaskCpuLoad);

        ++ mai_cntTaskCpuLoad;

        /* Producing load is implemented as producing full load for a given span of world
           time. This is not the same as producing an additional load of according
           percentage to the system since the task may be preempted and time elapses while
           this task is not loading the CPU. The percent value is only approximately. */
        const unsigned int tiDelayInUs = 23 /* ms = cycle time of this task */
                                         * 1000 /* ms to us to improve resolution */
                                         * _cpuLoadInPercent
                                         / 100;
        del_delayMicroseconds(/* fullLoadThisNoMicroseconds */ tiDelayInUs);
    
        /* Wait for next task activation. */
#ifdef DEBUG
        uint32_t eventMask =
#endif
        rtos_waitForEvent( /* eventMask */ RTOS_EVT_ABSOLUTE_TIMER
                         , /* all */ false
                         , /* timeout */ 23 /* ms */
                         );
        assert(eventMask == RTOS_EVT_ABSOLUTE_TIMER);
    }
} /* End of taskCpuLoad */



/**
 * This demonstration software uses a number of fast interrupts to produce system load and
 * prove stability. The interrupts are timer controlled (for simplicity) but the
 * activations are chosen as asynchronous to the operating system clock as possible to
 * provoke a most variable preemption pattern.
 */
static void installInterruptServiceRoutines(void)
{
    /* 0x2: Disable all PIT timers during configuration. Note, this is a
       global setting for all four timers. Accessing the bits makes this rountine have race
       conditions with the RTOS initialization that uses timer PID0. Both routines must not
       be called in concurrency. */
    PIT.PITMCR.R |= 0x2u;
    
    /* Install the ISRs now that all timers are stopped.
         Vector numbers: See MCU reference manual, section 28.7, table 28-4. */
    
    ihw_installINTCInterruptHandler( (int_externalInterruptHandler_t){.simpleIsr = isrPid0}
                                   , /* vectorNum */ 59 /* Timer PIT 0 */
                                   , /* psrPriority */ prio_INTC_ISRPid0
                                   , /* isPreemptable */ true
                                   , /* isKernelInterrupt */ false
                                   );
    ihw_installINTCInterruptHandler( (int_externalInterruptHandler_t){.simpleIsr = isrPid1}
                                   , /* vectorNum */ 60 /* Timer PIT 1 */
                                   , /* psrPriority */ prio_INTC_ISRPid1
                                   , /* isPreemptable */ true
                                   , /* isKernelInterrupt */ false
                                   );
    ihw_installINTCInterruptHandler( (int_externalInterruptHandler_t){.simpleIsr = isrPid2}
                                   , /* vectorNum */ 61 /* Timer PIT 2 */
                                   , /* psrPriority */ prio_INTC_ISRPid2
                                   , /* isPreemptable */ true
                                   , /* isKernelInterrupt */ false
                                   );

    /* Peripheral clock has been initialized to 120 MHz. The timer counts at this rate. The
       RTOS operates in ticks of 1ms we use prime numbers to get good asynchronity with the
       RTOS clock.
         Note, one interrupt is much slower than the two others. The reason is it does much
       more, it takes part at the test of safely accessing data shared with the application
       tasks.
         -1: See MCU reference manual, 36.5.1, p. 1157. */
    PIT.LDVAL0.R = 11987-1;/* Interrupt rate approx. 10kHz */
    PIT.LDVAL1.R = 4001-1; /* Interrupt rate approx. 30kHz */
    PIT.LDVAL2.R = 3989-1; /* Interrupt rate approx. 30kHz */

    /* Enable interrupts by the timers and start them. */
    PIT.TCTRL0.R = 0x3;
    PIT.TCTRL1.R = 0x3;
    PIT.TCTRL2.R = 0x3;
    
    /* Enable timer operation, all four timers are affected. Interrupt processing should
       start. */
    PIT.PITMCR.R &= ~0x2u;
    
} /* End of installInterruptServiceRoutines */



/**
 * The application owned part of the idle task. This routine is repeatedly called whenever
 * there's some execution time left. It's interrupted by any other task when it becomes
 * due.
 *   @remark
 * Different to all other tasks, the idle task routine may and should return. (The task as
 * such doesn't terminate). This has been designed in accordance with the meaning of the
 * original Arduino loop function.
 */ 
void loop(void)
{
    /* Installing more interrupts should be possible while the system is already running.
       We place the PID timer initialization here to prove this statement. */
    static bool isFirstTime_ = true;
    if(isFirstTime_)
    {
        del_delayMicroseconds(500000);
        installInterruptServiceRoutines();
        isFirstTime_ = false;
    }
       
    checkAndIncrementTaskCnts(idxTaskIdle);
    testPCP(idxTaskIdle);
    ++ mai_cntTaskIdle;

    /* Activate the non cyclic task. Note, the execution time of this task activation
       will by principle not be considered by the CPU load measurement started from the
       same task (the idle task). */
    rtos_sendEvent(EVT_ACTIVATE_TASK_NON_CYCLIC);

    /* Compute the average CPU load. Note, this operation lasts about 1s and has a
       significant impact on the cycling speed of this infinite loop. Furthermore, it
       measures only the load produced by the tasks and system interrupts but not that
       of the rest of the code in the idle loop. */
    mai_cpuLoad = gsl_getSystemLoad();

    /* In PRODUCTION compilation we print the inconsistencies found in the PCP test. */
    if(_sharedDataTasksIdleAnd1msAndCpuLoad.noErrors != 0)
    {
        iprintf( "CAUTION: %u errors found in PCP self-test!\r\n"
               , _sharedDataTasksIdleAnd1msAndCpuLoad.noErrors
               );
    }
} /* End of loop */



/**
 * The initalization of the RTOS tasks and general board initialization.
 */
void setup(void)
{
    /* Print standard greeting of RTuinOS applications. */
    iprintf(RTOS_EOL RTOS_RTUINOS_STARTUP_MSG RTOS_EOL);

    unsigned int idxTask = 0;
    
    /* Configure task 1 of priority class 0 */
    {
        static _Alignas(uint64_t) uint8_t stack[STACK_SIZE_TASK_IN_BYTE];
        assert(idxTask == idxTask1ms);
        rtos_initializeTask( idxTask++
                           , /* taskFunction */     task1ms
                           , /* priority */         prio_RTOS_Task1ms
                           , /* pStackArea */       &stack[0]
                           , /* stackSize */        sizeof(stack)
                           , /* startEventMask */   RTOS_EVT_DELAY_TIMER
                           , /* startByAllEvents */ false
                           , /* startTimeout */     10
                           );
    }

    {
        static _Alignas(uint64_t) uint8_t stack[STACK_SIZE_TASK_IN_BYTE];
        assert(idxTask == idxTask3ms);
        rtos_initializeTask( idxTask++
                           , /* taskFunction */     task3ms
                           , /* priority */         prio_RTOS_Task3ms
                           , /* pStackArea */       &stack[0]
                           , /* stackSize */        sizeof(stack)
                           , /* startEventMask */   RTOS_EVT_DELAY_TIMER
                           , /* startByAllEvents */ false
                           , /* startTimeout */     17
                           );
    }

    {
        static _Alignas(uint64_t) uint8_t stack[STACK_SIZE_TASK_IN_BYTE];
        assert(idxTask == idxTask1s);
        rtos_initializeTask( idxTask++
                           , /* taskFunction */     task1s
                           , /* priority */         prio_RTOS_Task1s
                           , /* pStackArea */       &stack[0]
                           , /* stackSize */        sizeof(stack)
                           , /* startEventMask */   RTOS_EVT_DELAY_TIMER
                           , /* startByAllEvents */ false
                           , /* startTimeout */     100
                           );
    }

    {
        static _Alignas(uint64_t) uint8_t stack[STACK_SIZE_TASK_IN_BYTE];
        assert(idxTask == idxTaskNonCyclic);
        rtos_initializeTask( idxTask++
                           , /* taskFunction */     taskNonCyclic
                           , /* priority */         prio_RTOS_TaskNonCyclic
                           , /* pStackArea */       &stack[0]
                           , /* stackSize */        sizeof(stack)
                           , /* startEventMask */   EVT_ACTIVATE_TASK_NON_CYCLIC
                           , /* startByAllEvents */ false
                           , /* startTimeout */     0
                           );
    }

    {
        static _Alignas(uint64_t) uint8_t stack[STACK_SIZE_TASK_IN_BYTE];
        assert(idxTask == idxTask17ms);
        rtos_initializeTask( idxTask++
                           , /* taskFunction */     task17ms
                           , /* priority */         prio_RTOS_Task17ms
                           , /* pStackArea */       &stack[0]
                           , /* stackSize */        sizeof(stack)
                           , /* startEventMask */   RTOS_EVT_DELAY_TIMER
                           , /* startByAllEvents */ false
                           , /* startTimeout */     0
                           );
    }

    {
        static _Alignas(uint64_t) uint8_t stack[STACK_SIZE_TASK_IN_BYTE];
        assert(idxTask == idxTaskOnButtonDown);
        rtos_initializeTask( idxTask++
                           , /* taskFunction */     taskOnButtonDown
                           , /* priority */         prio_RTOS_TaskOnButtonDown
                           , /* pStackArea */       &stack[0]
                           , /* stackSize */        sizeof(stack)
                           , /* startEventMask */   EVT_ACTIVATE_TASK_ON_BUTTON_DOWN
                           , /* startByAllEvents */ false
                           , /* startTimeout */     0
                           );
    }

    {
        static _Alignas(uint64_t) uint8_t stack[STACK_SIZE_TASK_IN_BYTE];
        assert(idxTask == idxTaskCpuLoad);
        rtos_initializeTask( idxTask++
                           , /* taskFunction */     taskCpuLoad
                           , /* priority */         prio_RTOS_TaskCpuLoad
                           , /* pStackArea */       &stack[0]
                           , /* stackSize */        sizeof(stack)
                           , /* startEventMask */   RTOS_EVT_DELAY_TIMER
                           , /* startByAllEvents */ false
                           , /* startTimeout */     3
                           );
    }

    /* The last check ensures that we didn't forget to register a task. */
    assert(idxTask == noRegisteredTasks);

} /* End of setup */
