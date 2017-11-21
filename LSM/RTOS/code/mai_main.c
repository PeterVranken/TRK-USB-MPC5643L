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
 *   The idle task is used to report the system state, CPU load, stack usage and task
 * overrun events (more precise: failed activations).
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
 *   task1ms
 *   task3ms
 *   task1s
 *   taskNonCyclic
 *   task17ms
 *   taskOnButtonDown
 *   taskCpuLoad
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
#include "gsl_systemLoad.h"
#include "mai_main.h"


/*
 * Defines
 */

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
#define RESOURCE_CNT_TASK_ARY   RESOURCE_ALL_TASKS

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
    idTask1ms = 0
    , idTask3ms
    , idTask1s
    , idTaskNonCyclic
    , idTask17ms
    , idTaskOnButtonDown
    , idTaskCpuLoad
    
    /** The number of tasks to register. */
    , noTasks

    /** The idle task is not a task under control of the RTOS and it doesn't have an ID.
        We assign it a pseudo task ID that is used to store some task related data in the
        same array here in this sample application as we do by true task ID for all true
        tasks. */
    , pseudoIdTaskIdle = noTasks
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
volatile unsigned long long _cntTaskAry[noTasks+1] = {[0 ... noTasks] = 0};

volatile unsigned long mai_cntIdle = 0      /** Counter of cycles of infinite main loop. */
                     , mai_cntTask1ms = 0   /** Counter of cyclic task. */
                     , mai_cntTask3ms = 0   /** Counter of cyclic task. */
                     , mai_cntTask1s = 0    /** Counter of cyclic task. */
                     , mai_cntTaskNonCyclic = 0  /** Counter of calls of software triggered
                                                     task */
                     , mai_cntTask17ms = 0  /** Counter of cyclic task. */
                     , mai_cntTaskOnButtonDown = 0  /** Counter of button event task. */
                     , mai_cntTaskCpuLoad = 0   /** Counter of cyclic task. */
                     , mai_cntActivationLossTaskNonCyclic = 0; /* Lost activations of non
                                                                  cyclic task by 17ms
                                                                  cyclic task. */

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
    unsigned long long cntTaskAryCpy[noTasks+1]
                     , cntAllTasksCpy;
    _Static_assert(sizeof(_cntTaskAry) == sizeof(cntTaskAryCpy), "Bad data definition");
    const uint32_t msr = ihw_enterCriticalSection();
    {
        memcpy(&cntTaskAryCpy[0], (void*)&_cntTaskAry[0], sizeof(cntTaskAryCpy));
        cntAllTasksCpy = _cntAllTasks;
    }
    ihw_leaveCriticalSection(msr);

    /* Check consistency of got data. +1: The last entry is meant for the idle task. */
    unsigned int u;
    for(u=0; u<noTasks+1; ++u)
        cntAllTasksCpy -= cntTaskAryCpy[u];
    assert(cntAllTasksCpy == 0);

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

    /* Check consistency of got data. +1: The last entry is meant for the idle task. */
    for(u=0; u<noTasks+1; ++u)
        cntAllTasksCpy -= cntTaskAryCpy[u];
    assert(cntAllTasksCpy == 0);

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
    if(idTask == pseudoIdTaskIdle)
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
           which has the highest priority in the sub-set. Omitting the critical section
           code invalidates the code if the task priorities are redefined in the heading
           part of the file and we need to put an assertion to double check this. */
        _Static_assert( prioTask1ms >= prioTaskIdle  &&  prioTask1ms >= prioTask1ms
                      , "Task priorities do not meet the requirements of function testPCP"
                      );
        ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTask1ms;
        ++ _sharedDataTasksIdleAnd1msAndCpuLoad.cntTotal;
    }
    else
    {
        /* This function is intended only for a sub-set of tasks. */
        assert(false);
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
           
            /* The application is halted in DEBUG compilation. This makes the error
               observable without connected terminal. */
            assert(false);
        }
    }
    ReleaseResource();

} /* End of testPCP. */




/**
 * Task function, cyclically activated every Millisecond. The LED D4 is switched on and off
 * and the button SW3 is read and evaluated.
 */
static void task1ms()
{
    checkAndIncrementTaskCnts(idTask1ms);
    testPCP(idTask1ms);

    ++ mai_cntTask1ms;

    /* Activate the non cyclic task.
         Note, the non cyclic task is of higher priority than this task and it'll be
       executed immediatly, preempting this task. The second activation below, on button
       down must not lead to an activation loss.
         Note, activating one and the same task from different contexts requires a critical
       section. */
    GetResource(RESOURCE_ACTIVATE_TASK_NON_CYCLIC);
    {
        rtos_activateTask(idTaskNonCyclic);
    }
    ReleaseResource();

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
static void task3ms()
{
    checkAndIncrementTaskCnts(idTask3ms);
    ++ mai_cntTask3ms;

} /* End of task3ms */



/**
 * Task function, cyclically activated every second.
 */
static void task1s()
{
    checkAndIncrementTaskCnts(idTask1s);

    ++ mai_cntTask1s;

    static int cntIsOn_ = 0;
    if(++cntIsOn_ >= 1)
        cntIsOn_ = -1;
    lbd_setLED(_ledTask1s, /* isOn */ cntIsOn_ >= 0);

} /* End of task1s */



/**
 * A non cyclic task, which is solely activated by software triggers from other tasks.
 */
static void taskNonCyclic()
{
    checkAndIncrementTaskCnts(idTaskNonCyclic);
    ++ mai_cntTaskNonCyclic;

} /* End of taskNonCyclic */



/**
 * Task function, cyclically activated every 17ms.
 */
static void task17ms()
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
static void taskOnButtonDown()
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
static void taskCpuLoad()
{
    checkAndIncrementTaskCnts(idTaskCpuLoad);
    testPCP(idTaskCpuLoad);
    
    ++ mai_cntTaskCpuLoad;

    const unsigned int tiDelayInUs = 23 /* ms = cycle time of this task */
                                     * 1000 /* ms to us to improve resolution */
                                     * _cpuLoadInPercent
                                     / 100;

    /* The factor 120 is required to consider the unit of __builtin_ppc_get_timebase(),
       which is the CPU clock tick. */
    const uint64_t tiEnd = __builtin_ppc_get_timebase() + tiDelayInUs*120;

    /* Busy loop. Note, preemption is possible, which effectively lowers the additional CPU
       load, this loop produces. The higher the system load, the more grows this effect. */
    while(__builtin_ppc_get_timebase() < tiEnd)
        ;

} /* End of taskCpuLoad */




/**
 * Entry point into C code. The C main function is entered without arguments and despite of
 * its return code definition it must never be left. (Returning from main would enter an
 * infinite loop in the calling assembler startup code.)
 */
void main()
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
    _Static_assert(noTasks <= 8, "RTOS only supports eight tasks");

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
    assert(idTask == noTasks-1);

    /* Initialize the RTOS kernel. */
    rtos_initKernel();

    /* The code down here becomes our idle task. It is executed when and only when no
       application task is running. */

    while(true)
    {
        checkAndIncrementTaskCnts(pseudoIdTaskIdle);
        testPCP(pseudoIdTaskIdle);
        ++ mai_cntIdle;

        /* Activate the non cyclic task. */
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
        iprintf( "CPU load is %u.%u%%. Stack reserve: %u Byte. Failed task activations:\r\n"
                 "  task1ms: %u\r\n"
                 "  task3ms: %u\r\n"
                 "  task1s: %u\r\n"
                 "  taskNonCyclic: %u\r\n"
                 "  task17ms: %u\r\n"
                 "  taskOnButtonDown: %u\r\n"
                 "  taskCpuLoad: %u\r\n"
               , mai_cpuLoad/10, mai_cpuLoad%10
               , rtos_getStackReserve()
               , rtos_getNoActivationLoss(idTask1ms)
               , rtos_getNoActivationLoss(idTask3ms)
               , rtos_getNoActivationLoss(idTask1s)
               , rtos_getNoActivationLoss(idTaskNonCyclic)
               , rtos_getNoActivationLoss(idTask17ms)
               , rtos_getNoActivationLoss(idTaskOnButtonDown)
               , rtos_getNoActivationLoss(idTaskCpuLoad)
               );
      
        /* In PRODUCTION compilation we print the inconsistencies found in the PCP test. */
        if(_sharedDataTasksIdleAnd1msAndCpuLoad.noErrors != 0)
        {
            iprintf( "CAUTION: %u errors found in PCP self-test!\n"
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
