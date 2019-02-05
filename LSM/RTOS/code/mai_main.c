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
 * overrun events (more precise: failed activations).\n
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
 * RTOS' task array constantly increases.\n
 *   Occasional activation losses can be reported for task taskNonCyclic. It can be
 * preempted by task task17ms and this task activates task taskNonCyclic. If it tries to
 * do, while it has preempted task taskNonCyclic, the activation is not possible.\n
 *   The code runs a permanent test of the different offered mechanisms of mutual exclusion
 * of tasks that access some shared data objects. A recognized failure is reported by
 * assertion, which will halt the code execution (in DEBUG compilation only). Everything is
 * fine as long as the LEDs continue blinking.
 *
 * Copyright (C) 2017 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   isrPit1
 *   isrPit2
 *   isrPit3
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

#include "MPC5643L.h"
#include "typ_types.h"
#include "sup_settings.h"
#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
#include "sio_serialIO.h"
#include "RTOS.h"
#include "del_delay.h"
#include "gsl_systemLoad.h"
#include "mai_main.h"


/*
 * Defines
 */

/** The demo can be compiled with a ground load. Most tasks produce some CPU load if this
    switch is set to 1. */
#define TASKS_PRODUCE_GROUND_LOAD   0

/** The test of the priority ceiling protocol counts recognized errors, which are reproted
    by console output and to the debugger. Alternatively and by setting this macro to 1,
    the software execution can be halted on recognition of the first error. The no longer
    blinking LEDs would indicate the problem. */
#define HALT_ON_PCP_TEST_FAILURE    1

/** A wrapper around the API for the priority ceiling protocal (PCP), which lets the API
    for mutual exclusion of a task set look like the API calls from the OSEK/VDX standard.
      Here, for getting the resource, i.e. for entering a critical section of code. */
#define GetResource(resource)                                                               \
            { uint32_t priorityLevelSoFar = rtos_suspendAllInterruptsByPriority             \
                                                (/* suspendUpToThisPriority */ (resource));

/** A wrapper around the API for the priority ceiling protocal (PCP), which lets the API
    for mutual exclusion of a task set look like the API calls from the OSEK/VDX standard.
      Here, for returning the resource, i.e. for leaving a critical section of code. */
#define ReleaseResource() rtos_resumeAllInterruptsByPriority(priorityLevelSoFar); }

/** The task counter array is accessed by all tasks. Here it is modelled as an OSEK/VDX
    like resource to be used with #GetResource and #ReleaseResource. */
#define RESOURCE_CNT_TASK_ARY   (MAXP(RESOURCE_ALL_TASKS,prioISRPit1))

/** The priority level to set if all tasks should be mutually excluded from accessing a
    shared resource. The macro is named such that the code resembles the resource API from
    the OSEK/VDX standard. */
#define RESOURCE_ALL_TASKS  (MAXP(prioTask1ms,MAXP(prioTask3ms, \
                             MAXP(prioTask1s,MAXP(prioTaskNonCyclic, \
                             MAXP(prioTask17ms,MAXP(prioTaskOnButtonDown, \
                             prioTaskCpuLoad)))))))

/** The priority level to set for the activation of task taskNonCyclic. The call of
    rtos_activateTask(task) is not reentrant; if several tasks want to activate one and the
    same task then this call needs to be placed into a critical section. The macro is named
    such that the code resembles the resource API from the OSEK/VDX standard. */
#define RESOURCE_ACTIVATE_TASK_NON_CYCLIC  (MAXP(prioTask1ms,MAXP(prioTask17ms,prioTaskIdle)))

/** The priority level to set for the atomic operations done in function testPCP(). The
    macro is named such that the code resembles the resource API from the OSEK/VDX
    standard. */
#define RESOURCE_TEST_PCP  (MAXP(prioTask1ms,MAXP(prioTaskCpuLoad,prioTaskIdle)))

/** Some helper code to get the maximum task priority as a constant for the priority ceiling
    protocol. */
#define MAXP(p1,p2)  ((p2)>(p1)?(p2):(p1))


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
    idTask1ms = 0
    , idTask3ms
    , idTask1s
    , idTaskNonCyclic
    , idTask17ms
    , idTaskOnButtonDown
    , idTaskCpuLoad
    
    /** The number of tasks to register. */
    , noRegisteredTasks

    /** The interrupts that are applied mainly to produce system load for testing, continue
        the sequence of ids, so that they can share the shared data container with test
        data. */
    , idISRPit1 = noRegisteredTasks    
    , idISRPit2
    , idISRPit3
    
    /** The number of registered tasks and ISRs. */
    , noRegisteredTasksAndISRs
    
    /** The number of ISRs. */
    , noISRs = noRegisteredTasksAndISRs - noRegisteredTasks
    
    /** The idle task is not a task under control of the RTOS and it doesn't have an ID.
        We assign it a pseudo task ID that is used to store some task related data in the
        same array here in this sample application as we do by true task ID for all true
        tasks. */
    , idTaskIdle = noRegisteredTasksAndISRs
    
    /** The number of all concurrent excution threads: The ISRs, the application tasks and
        the idle task. */
    , noExecutionContexts
};


/** The RTOS uses constant task priorities, which are defined here. (The concept and
    architecture of the RTOS allows dynamic changeing of a task's priority at runtime, but
    we didn't provide an API for that yet; where are the use cases?) */
enum
{
    prioTask1ms = 2
    , prioTask3ms = 2
    , prioTask1s = 1
    , prioTaskNonCyclic = 3
    , prioTask17ms = 4
    , prioTaskOnButtonDown = 1
    , prioTaskCpuLoad = 1
    , prioTaskIdle = 0
    
    , prioISRPit1 = 5
    , prioISRPit2 = 6
    , prioISRPit3 = 15
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
                     , mai_cntActivationLossTaskNonCyclic = 0  /* Lost activations of non
                                                                  cyclic task by 17ms
                                                                  cyclic task. */
                     , mai_cntISRPit1 = 0
                     , mai_cntISRPit2 = 0
                     , mai_cntISRPit3 = 0;

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
 *   @param idTask
 * The ID (or index) of the calling task. Needed to identify the task related counter.
 */
static void checkAndIncrementTaskCnts(unsigned int idTask)
{
    /* Increment task related counter and shared counter in an atomic operation. */
    assert(idTask < sizeOfAry(_cntTaskAry));
    GetResource(RESOURCE_CNT_TASK_ARY);
    {
        ++ _cntTaskAry[idTask];
        ++ _cntAllTasks;
    }
    ReleaseResource();

    /* Get all task counters and the common counter in an atomic operation. Now, we apply
       another offered mechanism for mutual exclusion of tasks. */
    unsigned long long cntTaskAryCpy[noExecutionContexts]
                     , cntAllTasksCpy;
    _Static_assert(sizeof(_cntTaskAry) == sizeof(cntTaskAryCpy), "Bad data definition");
    const uint32_t msr = ihw_enterCriticalSection();
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
 * Test function for priority ceiling portocol. To be called from a sub-set of tasks, idle
 * task, task1ms and taskCpuLoad.
 *   The test is aimed to prove the correct implementation of the offered mutual exclusion
 * mechanism for this sub-set of tasks.
 *   @param idTask
 * The ID (or index) of the calling task. Needed to identify the task related counter.
 */
static void testPCP(unsigned int idTask)
{
    /* Increment task related counter and shared counter in an atomic operation. */
    if(idTask == idTaskIdle)
    {
        GetResource(RESOURCE_TEST_PCP);
        {
            ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTaskIdle;
            ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTotal;
        }
        ReleaseResource();
    }
    else if(idTask == idTaskCpuLoad)
    {
        GetResource(RESOURCE_TEST_PCP);
        {
            ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTaskCpuLoad;
            ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTotal;
        }
        ReleaseResource();
    }
    else if(idTask == idTask1ms)
    {
        /* Here, we want to prove that the resource doesn't need to be acquired by a task,
           which has the highest priority in the sub-set.
             Omitting the critical section code invalidates the code if the task priorities
           are redefined in the heading part of the file and we need to put an assertion to
           double check this. */
        _Static_assert( prioTask1ms >= prioTaskIdle  &&  prioTask1ms >= prioTaskCpuLoad
                      , "Task priorities do not meet the requirements of function testPCP"
                      );
        ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTask1ms;
        ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTotal;
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
    GetResource(RESOURCE_TEST_PCP);
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
 * A regularly triggered interrupt handler for the timer PIT1. The interrupt does nothing
 * but counting a variable. The ISR participates the test of safely sharing data with the
 * application tasks. It is triggered at medium frequency and asynchronously to the
 * kernel's clock tick to prove the system stability and properness of the context
 * switches.
 */
static void isrPit1()
{
    checkAndIncrementTaskCnts(idISRPit1);
    ++ mai_cntISRPit1;
    
    /* Acknowledge the interrupt in the causing HW device. */
    PIT.TFLG1.B.TIF = 0x1;

} /* End of isrPit1 */



/**
 * A regularly triggered interrupt handler for the timer PIT2. The interrupt does nothing
 * but counting a variable. It is triggered at high frequency and asynchronously to the
 * kernel's clock tick to prove the system stability and properness of the context switches.
 */
static void isrPit2()
{
    ++ mai_cntISRPit2;
    
    /* Acknowledge the interrupt in the causing HW device. */
    PIT.TFLG2.B.TIF = 0x1;

} /* End of isrPit2 */



/**
 * A regularly triggered interrupt handler for the timer PIT3. The interrupt does nothing
 * but counting a variable. It is triggered at high frequency and asynchronously to the
 * kernel's clock tick to prove the system stability and properness of the context switches.
 */
static void isrPit3()
{
    ++ mai_cntISRPit3;
    
    /* Acknowledge the interrupt in the causing HW device. */
    PIT.TFLG3.B.TIF = 0x1;

} /* End of isrPit3 */



/**
 * Task function, cyclically activated every Millisecond. The LED D4 is switched on and off
 * and the button SW3 is read and evaluated.
 */
static void task1ms(void)
{
    checkAndIncrementTaskCnts(idTask1ms);
    testPCP(idTask1ms);

    ++ mai_cntTask1ms;

    /* Activate the non cyclic task.
         Note, the non cyclic task is of higher priority than this task and it'll be
       executed immediately, preempting this task. The second activation below, on button
       down must not lead to an activation loss.
         Note, activating one and the same task from different contexts requires a critical
       section. */
    GetResource(RESOURCE_ACTIVATE_TASK_NON_CYCLIC);
    {
        rtos_activateTask(idTaskNonCyclic);
    }
    ReleaseResource();

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
               have been processed meanwhile and this one should be accepted, too.
                 Note, activating one and the same task from different contexts requires a
               critical section. */
#ifdef DEBUG
            bool bActivationAccepted;
#endif
            GetResource(RESOURCE_ACTIVATE_TASK_NON_CYCLIC);
            {
#ifdef DEBUG
                bActivationAccepted =
#endif
                rtos_activateTask(idTaskNonCyclic);
            }
            ReleaseResource();
            assert(bActivationAccepted);

            /* Activate our button down event task. The activation will normally succeed
               but at high load and very fast button press events it it theoretically
               possible that not. We don't place an assertion. */
#ifdef DEBUG
            bActivationAccepted =
#endif
            rtos_activateTask(idTaskOnButtonDown);
            //assert(bActivationAccepted);

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

} /* End of task1ms */




/**
 * Task function, cyclically activated every 3ms.
 */
static void task3ms(void)
{
    checkAndIncrementTaskCnts(idTask3ms);
    ++ mai_cntTask3ms;

#if TASKS_PRODUCE_GROUND_LOAD == 1
    /* Produce a bit of CPU load. This call simulates some true application software. */
    del_delayMicroseconds(/* fullLoadThisNoMicroseconds */ 150 /* approx. 5% load */);
#endif    
    
} /* End of task3ms */



/**
 * Task function, cyclically activated every second.
 */
static void task1s(void)
{
    checkAndIncrementTaskCnts(idTask1s);

    ++ mai_cntTask1s;

    static int cntIsOn_ = 0;
    if(++cntIsOn_ >= 1)
        cntIsOn_ = -1;
    lbd_setLED(_ledTask1s, /* isOn */ cntIsOn_ >= 0);

#if TASKS_PRODUCE_GROUND_LOAD == 1
    /* Produce a bit of CPU load. This call simulates some true application software.
         Note, the cyclic task taskCpuLoad has a period time of 23 ms and it has the same
       priority as this task. Because of the busy loop here and because the faster task
       itself has a non negligible execution time, there's a significant chance of loosing
       an activation of the faster task once a second. */
    del_delayMicroseconds(/* fullLoadThisNoMicroseconds */ 20000 /* approx. 2% load */);
#endif    

    /* Simple code: First calculation of time to print is wrong. */
    static unsigned long tiPrintf = 0;
    unsigned long long tiFrom = GSL_PPC_GET_TIMEBASE();
    iprintf( "CPU load is %u.%u%%. Stack reserve: %u Byte. Task activations (lost):\r\n"
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
           , rtos_getStackReserve()
           , mai_cntTask1ms, rtos_getNoActivationLoss(idTask1ms)
           , mai_cntTask3ms, rtos_getNoActivationLoss(idTask3ms)
           , mai_cntTask1s, rtos_getNoActivationLoss(idTask1s)
           , mai_cntTaskNonCyclic, rtos_getNoActivationLoss(idTaskNonCyclic)
           , mai_cntTask17ms, rtos_getNoActivationLoss(idTask17ms)
           , mai_cntTaskOnButtonDown, rtos_getNoActivationLoss(idTaskOnButtonDown)
           , mai_cntTaskCpuLoad, rtos_getNoActivationLoss(idTaskCpuLoad)
           , mai_cntTaskIdle
           , tiPrintf
           );
    tiPrintf = (unsigned long)(GSL_PPC_GET_TIMEBASE() - tiFrom) / 120;

} /* End of task1s */



/**
 * A non cyclic task, which is solely activated by software triggers from other tasks.
 */
static void taskNonCyclic(void)
{
    checkAndIncrementTaskCnts(idTaskNonCyclic);
    ++ mai_cntTaskNonCyclic;

} /* End of taskNonCyclic */



/**
 * Task function, cyclically activated every 17ms.
 */
static void task17ms(void)
{
    checkAndIncrementTaskCnts(idTask17ms);
    ++ mai_cntTask17ms;

    /* This task has a higher priority than the software triggered, non cyclic task. Since
       the latter one is often active we have a significant likelihood of a failing
       activation from here -- always if we preempted the non cyclic task.
         Note, activating one and the same task from different contexts requires a critical
       section. This task had got the high application task priority so that the explicit
       Get/ReleaseResource could be omitted. Doing so would however break the possibility
       to play with the sample code and to arbitrarily change the priorities in the
       heading part of this file. */
    GetResource(RESOURCE_ACTIVATE_TASK_NON_CYCLIC);
    {
        if(!rtos_activateTask(idTaskNonCyclic))
            ++ mai_cntActivationLossTaskNonCyclic;
    }
    ReleaseResource();

#if TASKS_PRODUCE_GROUND_LOAD == 1
    /* Produce a bit of CPU load. This call simulates some true application software. */
    del_delayMicroseconds(/* fullLoadThisNoMicroseconds */ 17*40 /* approx. 4% load */);
#endif    
    
    /* A task can't activate itself, we do not queue activations and it's obviously active
       at the moment. Test it. */
#ifdef DEBUG
    bool bActivationAccepted =
#endif
    rtos_activateTask(idTask17ms);
    assert(!bActivationAccepted);

} /* End of task17ms */



/**
 * A non cyclic task, which is activated by software trigger every time the button on the
 * evaluation board is pressed.
 */
static void taskOnButtonDown(void)
{
    checkAndIncrementTaskCnts(idTaskOnButtonDown);
    ++ mai_cntTaskOnButtonDown;
    iprintf("You pressed the button the %lu. time\r\n", mai_cntTaskOnButtonDown);

    /* Change the value of artificial CPU load on every click by 10%. */
    if(_cpuLoadInPercent < 100)
        _cpuLoadInPercent += 10;
    else
        _cpuLoadInPercent = 0;

    iprintf("The additional, artificial CPU load has been set to %u%%\r\n", _cpuLoadInPercent);
#ifdef __VLE__
    /* This code is a work around for a bug in MinGW-powerpc-eabivle-4.9.4. If the code is
       compiled with optimization and for VLE instructions and if the iprintf (likely the
       same for all kinds of printf) is the very last operation of the C function, then the
       compiler emits a Book E instruction instead of the VLE equivalent. The execution goes
       into a trap. See https://community.nxp.com/message/966809 for details.
         Note, this is likely due to a specific optimizer decision, which is really related
       to the conditions "printf" and "last statement" but not a common error, which could
       unpredictedly anywhere else. */
    asm volatile ("se_nop\n");
#endif
} /* End of taskOnButtonDown */


/**
 * A cyclic task of low priority, which is used to produce some artificial CPU load.
 *   @remark
 * We need to consider that in this sample, the measurement is inaccurate because the
 * idle loop is not empty (besides measuring the load) and so the observation window is
 * discontinuous. The task has a cycle time of much less than the CPU measurement
 * observation window, which compensates for the effect of the discontinuous observation
 * window.
 */
static void taskCpuLoad(void)
{
    checkAndIncrementTaskCnts(idTaskCpuLoad);
    testPCP(idTaskCpuLoad);
    
    ++ mai_cntTaskCpuLoad;

    /* Producing load is implemented as producing full load for a given span of world time.
       This is not the same as producing an additional load of according percentage to the
       system since the task may be preempted and time elapses while this task is not
       loading the CPU. The percent value is only approximately. */
    const unsigned int tiDelayInUs = 23 /* ms = cycle time of this task */
                                     * 1000 /* ms to us to improve resolution */
                                     * _cpuLoadInPercent
                                     / 100;
    del_delayMicroseconds(/* fullLoadThisNoMicroseconds */ tiDelayInUs);

} /* End of taskCpuLoad */



/**
 * This demonstration software uses a number of fast interrupts to produce system load and
 * prove stability. The interrupts are timer controlled (for simplicity) but the
 * activations are chosen as asynchonous to the operating system clock as possible to
 * provoke a most variable preemption pattern.
 */
static void installInterruptServiceRoutines(void)
{
    /* 0x2: Disable all PIT timers during configuration. Note, this is a
       global setting for all four timers. Accessing the bits makes this rountine have race
       conditions with the RTOS initialization that uses timer PIT0. Both routines must not
       be called in concurrency. */
    PIT.PITMCR.R |= 0x2u;
    
    /* Install the ISRs now that all timers are stopped.
         Vector numbers: See MCU reference manual, section 28.7, table 28-4. */
    
    ihw_installINTCInterruptHandler( isrPit1
                                   , /* vectorNum */ 60
                                   , /* psrPriority */ prioISRPit1
                                   , /* isPreemptable */ true
                                   );
    ihw_installINTCInterruptHandler( isrPit2
                                   , /* vectorNum */ 61
                                   , /* psrPriority */ prioISRPit2
                                   , /* isPreemptable */ true
                                   );
    ihw_installINTCInterruptHandler( isrPit3
                                   , /* vectorNum */ 127
                                   , /* psrPriority */ prioISRPit3
                                   , /* isPreemptable */ true
                                   );

    /* Peripheral clock has been initialized to 120 MHz. The timer counts at this rate. The
       RTOS operates in ticks of 1ms we use prime numbers to get good asynchronity with the
       RTOS clock.
         Note, one interrupt is much slower than the two others. The reason is it does much
       more, it takes part at the test of safely accessing data shared with the application
       tasks.
         -1: See MCU reference manual, 36.5.1, p. 1157. */
    PIT.LDVAL1.R = 11987-1;/* Interrupt rate approx. 10kHz */
    PIT.LDVAL2.R = 4001-1; /* Interrupt rate approx. 30kHz */
    PIT.LDVAL3.R = 3989-1; /* Interrupt rate approx. 30kHz */

    /* Enable interrupts by the timers and start them. */
    PIT.TCTRL1.R = 0x3;
    PIT.TCTRL2.R = 0x3;
    PIT.TCTRL3.R = 0x3;
    
    /* Enable timer operation, all four timers are affected. Interrupt processing should
       start. */
    PIT.PITMCR.R &= ~0x2u;
    
} /* End of installInterruptServiceRoutines */




/**
 * Entry point into C code. The C main function is entered without arguments and despite of
 * its return code definition it must never be left. (Returning from main would enter an
 * infinite loop in the calling assembler startup code.)
 */
void main(void)
{
    /* The first operation of the main function is the call of ihw_initMcuCoreHW(). The
       assembler implemented startup code has brought the MCU in a preliminary working
       state, such that the C compiler constructs can safely work (e.g. stack pointer is
       initialized, memory access through MMU is enabled).
         ihw_initMcuCoreHW() does the remaining hardware initialization, that is still
       needed to bring the MCU in a basic stable working state. The main difference to the
       preliminary working state of the assembler startup code is the selection of
       appropriate clock rates. Furthermore, the interrupt controller is configured.
         This part of the hardware configuration is widely application independent. The
       only reason, why this code has not been called directly from the assembler code
       prior to entry into main() is code transparency. It would mean to have a lot of C
       code without an obvious point, where it is called. */
    ihw_initMcuCoreHW();

#ifdef DEBUG
    /* Check linker script. It's error prone with respect to keeping the initialized RAM
       sections and the according initial-data ROM sections strictly in sync. As long as
       this has not been sorted out by a redesign of linker script and startup code we put
       a minimal plausibility check here, which will likely detect typical errors.
         If this assertion fires your initial RAM contents will be corrupt. */
    extern const uint8_t ld_dataSize[0], ld_dataMirrorSize[0];
    assert(ld_dataSize == ld_dataMirrorSize);
#endif

    /* Initialize the button and LED driver for the eval board. */
    lbd_initLEDAndButtonDriver();

    /* Initialize the serial output channel as prerequisite of using printf. */
    sio_initSerialInterface(/* baudRate */ 115200);

    /* The external interrupts are enabled after configuring I/O devices. (Initialization
       of the RTOS can be done later.) */
    ihw_resumeAllInterrupts();

    /* The RTOS is restricted to eight tasks at maximum. */
    _Static_assert(noRegisteredTasks <= 8, "RTOS only supports eight tasks");

    /* Register the application tasks at the RTOS. Note, we do not really respect the ID,
       which is assigned to the task by the RTOS API rtos_registerTask(). The returned
       value is redundant. This technique requires that we register the task in the right
       order and this requires in practice a double-check by assertion - later maintenance
       errors are unavoidable otherwise. */
#ifdef DEBUG
    unsigned int idTask =
#endif
    rtos_registerTask( &(rtos_taskDesc_t){ .taskFct = task1ms
                                         , .tiCycleInMs = 1
                                         , .priority = prioTask1ms
                                         }
                     , /* tiFirstActivationInMs */ 10
                     );
    assert(idTask == idTask1ms);

#ifdef DEBUG
    idTask =
#endif
    rtos_registerTask( &(rtos_taskDesc_t){ .taskFct = task3ms
                                         , .tiCycleInMs = 3
                                         , .priority = prioTask3ms
                                         }
                     , /* tiFirstActivationInMs */ 17
                     );
    assert(idTask == idTask3ms);

#ifdef DEBUG
    idTask =
#endif
    rtos_registerTask( &(rtos_taskDesc_t){ .taskFct = task1s
                                         , .tiCycleInMs = 1000
                                         , .priority = prioTask1s
                                         }
                     , /* tiFirstActivationInMs */ 100
                     );
    assert(idTask == idTask1s);

#ifdef DEBUG
    idTask =
#endif
    rtos_registerTask( &(rtos_taskDesc_t){ .taskFct = taskNonCyclic
                                         , .tiCycleInMs = 0 /* non-cyclic */
                                         , .priority = prioTaskNonCyclic
                                         }
                     , /* tiFirstActivationInMs */ 0
                     );
    assert(idTask == idTaskNonCyclic);

#ifdef DEBUG
    idTask =
#endif
    rtos_registerTask( &(rtos_taskDesc_t){ .taskFct = task17ms
                                         , .tiCycleInMs = 17
                                         , .priority = prioTask17ms
                                         }
                     , /* tiFirstActivationInMs */ 0
                     );
    assert(idTask == idTask17ms);

#ifdef DEBUG
    idTask =
#endif
    rtos_registerTask( &(rtos_taskDesc_t){ .taskFct = taskOnButtonDown
                                         , .tiCycleInMs = 0 /* Event task, no coycle time */
                                         , .priority = prioTaskOnButtonDown
                                         }
                     , /* tiFirstActivationInMs */ 0
                     );
    assert(idTask == idTaskOnButtonDown);

#ifdef DEBUG
    idTask =
#endif
    rtos_registerTask( &(rtos_taskDesc_t){ .taskFct = taskCpuLoad
                                         , .tiCycleInMs = 23 /* ms */
                                         , .priority = prioTaskCpuLoad
                                         }
                     , /* tiFirstActivationInMs */ 3
                     );
    assert(idTask == idTaskCpuLoad);

    /* The last check ensures that we didn't forget to register a task. */
    assert(idTask == noRegisteredTasks-1);

    /* Initialize the RTOS kernel. */
    rtos_initKernel();

    /* Installing more interrupts should be possible while the system is already running.
       We place the PIT timer initialization here to prove this statement. */
    del_delayMicroseconds(500000);
    installInterruptServiceRoutines();
       
    /* The code down here becomes our idle task. It is executed when and only when no
       application task is running. */

    while(true)
    {
        checkAndIncrementTaskCnts(idTaskIdle);
        testPCP(idTaskIdle);
        ++ mai_cntTaskIdle;

        /* Activate the non cyclic task. Note, the execution time of this task activation
           will by principle not be considered by the CPU load measurement started from the
           same task (the idle task). */
#ifdef DEBUG
        bool bActivationAccepted =
#endif
        rtos_activateTask(idTaskNonCyclic);
        assert(bActivationAccepted);

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
#if 0
        /* Test of return from main: After 10s */
        if(mai_cntTask1ms >= 10000)
            break;
#endif
    }
} /* End of main */
