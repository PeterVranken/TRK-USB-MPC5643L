/**
 * @file tc04_2Task1PrioClass.c
 *   Test case 04 of RTuinOS. Two tasks of same priority class are defined besides the idle
 * task.
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
 *
 * Module interface
 *   setup
 *   loop
 * Local functions
 *   task01_class00
 *   task02_class00
 */

/*
 * Include files
 */

#include <stdio.h>
#include <assert.h>
#include "f2d_float2Double.h"
#include "mai_main.h"
#include "rtos.h"


/*
 * Defines
 */
 
/** The number of interrupt levels, we use in this application is required for an
    estimation of the appropriate stack sizes.\n
      We have 2 interrupts for the serial interface and the RTOS system timer. */
#define NO_IRQ_LEVELS_IN_USE    3

/** The stack usage by the application tasks itself; interrupts disregarded here. */
#define STACK_USAGE_IN_BYTE     1000

/** The stack size. */
#define STACK_SIZE_IN_BYTE \
            (RTOS_REQUIRED_STACK_SIZE_IN_BYTE(STACK_USAGE_IN_BYTE, NO_IRQ_LEVELS_IN_USE))


/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
 
/*
 * Data definitions
 */
 
static _Alignas(uint64_t) uint8_t _taskStack1[STACK_SIZE_IN_BYTE];
static _Alignas(uint64_t) uint8_t _taskStack2[STACK_SIZE_IN_BYTE];
static volatile unsigned int _t1=0, _t2=0, _id=0;
 
 
/*
 * Function implementation
 */

/**
 * The first task in this test case (besides idle).
 *   @param resumeCondition
 * Which events made the task run the very first time?
 *   @remark
 * A task function must never return; this would cause a reset.
 */ 
static void _Noreturn task01_class00(uint32_t resumeCondition)
{
    assert(resumeCondition == RTOS_EVT_ABSOLUTE_TIMER);
    unsigned long ti0, ti1;
    
    iprintf("task01_class00: Activated by 0x%08lx", resumeCondition);

    for(;;)
    {
        ++ _t1;
        iprintf("_t1: %u, _t2: %u, _id: %u\r\n", _t1, _t2, _id);
        fputs("task01_class00: rtos_delay(20)\r\n", stdout);
        ti0 = micros();
        rtos_delay(20);
        ti1 = micros();
        printf("task01_class00: Back from delay after %.3f ticks\r\n"
              , f2d((ti1-ti0)/(1e6*RTOS_TICK))
              );
        
        iprintf("task01_class00: Suspending at %lu\r\n", millis());

        resumeCondition = rtos_suspendTaskTillTime(/* deltaTimeTillRelease */ 250);
        assert(resumeCondition == RTOS_EVT_ABSOLUTE_TIMER);

        iprintf("task01_class00: Released at %lu\r\n", millis());
    }
} /* End of task01_class00 */




/**
 * The second task in this test case (besides idle).
 *   @param resumeCondition
 * Which events made the task run the very first time?
 *   @remark
 * A task function must never return; this would cause a reset.
 */ 
static void _Noreturn task02_class00(uint32_t resumeCondition ATTRIB_DBG_ONLY)
{
    assert(resumeCondition == RTOS_EVT_DELAY_TIMER);
    for(;;)
    {
        ++ _t2;

        resumeCondition = rtos_suspendTaskTillTime(/* deltaTimeTillRelease */ 100);
        assert(resumeCondition == RTOS_EVT_ABSOLUTE_TIMER);
    }
} /* End of task02_class00 */





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
    unsigned int u;    
    bool ok = true;
    
    /* Check stack reserve. */
    for(u=0; u<10; ++u)
    {
        if(_taskStack1[u] != 0xa5  ||  _taskStack2[u] != 0xa5)
        {
            ok = false;
            break;
        }
    }
    
    /* Try to indicate corruption of stack (if this still works then ...). */
    if(ok)
        mai_blink(2);
    else
        mai_blink(3);

    /* No task overruns should occur. */
    for(u=0; u<RTOS_NO_TASKS; ++u)
    {
        assert(rtos_getTaskOverrunCounter(/* idxTask */ u, /* doReset */ false) == 0);
    }

    ++ _id;    
   
} /* End of loop */




/**
 * The initalization of the RTOS tasks and general board initialization.
 */ 
void setup(void)
{
    /* Print standard greeting of RTuinOS applications. */
    iprintf(RTOS_EOL RTOS_RTUINOS_STARTUP_MSG RTOS_EOL);

    /* Task 1 of priority class 0 */
    rtos_initializeTask( /* idxTask */          0
                       , /* taskFunction */     task01_class00
                       , /* prioClass */        0
                       , /* pStackArea */       &_taskStack1[0]
                       , /* stackSize */        sizeof(_taskStack1)
                       , /* startEventMask */   RTOS_EVT_ABSOLUTE_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     10
                       );

    /* Task 2 of priority class 0 */
    rtos_initializeTask( /* idxTask */          1
                       , /* taskFunction */     task02_class00
                       , /* prioClass */        0
                       , /* pStackArea */       &_taskStack2[0]
                       , /* stackSize */        sizeof(_taskStack2)
                       , /* startEventMask */   RTOS_EVT_DELAY_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     99
                       );
} /* End of setup */




