/**
 * @file mai_main.c
 *   Blinking LED: Note the slight phase shift due to the differing task start times.
 *   CPU load: At nominal 100% it drops to about 50%: The execution time of the cyclic task
 * that produces the load exceeds the nominal cycle time of the task and every second
 * activation is lost. The activation loss counter in the RTOS' task array constantly
 * increases.
 *   @todo The function documentation is still a copy of the root code, which this sample
 * has been copied and derived from.
 *   ...
 *
 *   The main entry point of the C code. The assembler implemented startup code has been
 * executed and brought the MCU in a preliminary working state, such that the C compiler
 * constructs can safely work (e.g. stack pointer is initialized, memory access through MMU
 * is enabled). After that it branches here, into the C entry point main().\n
 *  The first operation of the main function is the call of the remaining hardware
 * initialization ihw_initMcuCoreHW() that is still needed to bring the MCU in basic stable
 * working state. The main difference to the preliminary working state of the assembler
 * startup code is the selection of appropriate clock rates. Furthermore, the interrupt
 * controller is configured. This part of the hardware configuration is widely application
 * independent. The only reason, why this code has not been called from the assembler code
 * prior to entry into main() is code transparency. It would mean to have a lot of C
 * code without an obvious point, where it is used.\n
 *   In this most basic sample the main function implements the standard Hello World
 * program of the Embedded Software World, the blinking LED.\n
 *   The main function configures the application dependent hardware, which is a cyclic
 * timer (Programmable Interrupt Timer 0, PIT 0) with cycle time 1ms. An interrupt handler
 * for this timer is registered at the Interrupt Controller (INTC). A second interrupt
 * handler is registered for software interrupt 3. Last but not least the LED outputs and
 * button inputs of the TRK-USB-MPC5643L are initialized.\n
 *   The code enters an infinite loop and counts the cycles. Any 500000 cycles it triggers
 * the software interrupt.\n
 *   Both interrupt handlers control one of the LEDs. LED 4 is toggled every 500ms by the
 * cyclic timer interrupt. We get a blink frequency of 1 Hz.\n 
 *   The software interrupt toggles LED 5 every other time it is raised. This leads to a
 * blinking of irrelated frequency.\n
 *   The buttons are unfortunately connected to GPIO inputs, which are not interrupt
 * enabled. We use the timer interrupt handler to poll the status. On button press of
 * Switch 3 the colors of the LEDs are toggled.
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
 *   task1ms
 *   task3ms
 *   task1s
 *   taskNonCyclic
 *   task17ms
 *   taskOnButtonDown
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
 
/** The enumeration of all tasks; the values are the task IDs. Actually, the ID is provided
    by the RTOS at runtime, when registering the task. However, it is guaranteed that the
    IDs returned by rtos_registerTask() are dealt out form the series 0, 1, 2, ..., 7. So
    we don't need to have a dynamic storage of the IDs; we define them as constants and
    double-check by assertion that we got the correct expected IDs. Note, this requires
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
};


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
    the OSEK/VDX standard*/
#define RESOURCE_ALL_TASKS  (MAXP(prioTask1ms,MAXP(prioTask3ms, \
                             MAXP(prioTask1s,MAXP(prioTaskNonCyclic, \
                             MAXP(prioTask17ms,prioTaskOnButtonDown))))))


/** Some helper code to get the maximum task priority as a constant for the priority ceiling
    protocol. */
#define MAXP(p1,p2)  ((p2)>(p1)?(p2):(p1))


/*
 * Local type definitions
 */
 
 
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
 *   @param taskId
 * The ID (or index) of the calling task. Needed to identify the task related counter.
 */
static void checkAndIncrementTaskCnts(unsigned int taskId)
{
    /* Increment task related counter and shared counter in an atomic operation. */
    assert(taskId < sizeOfAry(_cntTaskAry));
    GetResource(RESOURCE_CNT_TASK_ARY);
    {
        ++ _cntTaskAry[taskId];
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
 * Task function, cyclically activated every Millisecond. The LED D4 is switched on and off
 * and the button SW3 is read and evaluated.
 */
static void task1ms()
{
    checkAndIncrementTaskCnts(idTask1ms);
    
    ++ mai_cntTask1ms;

    /* Activate the non cyclic task.
         Note, the non cyclic task is of higher priority than this task and it'll be
       executed immediatly, preempting this task. The second activation below, on button
       down must not lead to an activation loss. */
    rtos_activateTask(idTaskNonCyclic);
    
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
               have been processed meanwhile and this one should be accepted, too.*/
            bool bActivationAccepted = rtos_activateTask(idTaskNonCyclic);
            assert(bActivationAccepted);
            
            /* Activate our button down event task. The activation will normally succeed
               but at high load and very fast button press events it it theoretically
               possible that not. We don't place an assertion. */
            bActivationAccepted = rtos_activateTask(idTaskOnButtonDown);
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
       activation from here -- always if we preempted the non cyclic task. */
    if(!rtos_activateTask(idTaskNonCyclic))
        ++ mai_cntActivationLossTaskNonCyclic;

    /* A task can't activate itself, we do not queue activations and it's obviously active
       at the moment. Test it. */
    assert(!rtos_activateTask(idTask17ms));
    
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
    /* Init core HW of MCU so that it can be safely operated. */
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
        ++ mai_cntIdle;

        /* Activate the non cyclic task. */
        bool bActivationAccepted = rtos_activateTask(idTaskNonCyclic);
        assert(bActivationAccepted);

        /* Compute the average CPU load. Note, this operation lasts about 1s and has a
           significant impact on the cycling speed of this infinite loop. Furthermore, it
           measures only the load produced by the tasks and system interrupts but not that
           of the rest of the code in the idle loop. */
        mai_cpuLoad = gsl_getSystemLoad();
        iprintf("CPU load is %u.%u%%\r\n", mai_cpuLoad/10, mai_cpuLoad%10);
#if 0
        /* Test of return from main: After 10s */
        if(mai_cntTask1ms >= 10000)
            break;
#endif
    }
} /* End of main */
