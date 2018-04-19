/**
 * @file tc03_RTTasks.c
 *   Test case 03 of RTuinOS. Several tasks of different priority are defined. Task
 * switches are counted and reported in the idle task.
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
 *   blink
 *   task01_class00
 */

/*
 * Include files
 */

#include <stdio.h>
#include <assert.h>

#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
#include "del_delay.h"
#include "mai_main.h"
#include "rtos.h"


/*
 * Defines
 */
 
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

/** The stack usage by the application itself; interrupts disregarded here. */
#define STACK_USAGE_IN_BYTE     1000

/** The stack size in byte is derived from #STACK_USAGE_IN_BYTE and #NO_IRQ_LEVELS.
    Alignment constraints are considered in the computation. */
#define STACK_SIZE_TASK_IN_BYTE ((((NO_IRQ_LEVELS)*(S_I_StFr)+(S_SC_StFr)+STACK_USAGE_IN_BYTE)+7)&~7)


#define STACK_SIZE_TASK00_C0   (STACK_SIZE_TASK_IN_BYTE)
#define STACK_SIZE_TASK01_C0   (STACK_SIZE_TASK_IN_BYTE)
#define STACK_SIZE_TASK00_C1   (STACK_SIZE_TASK_IN_BYTE)
 

/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
static void _Noreturn task00_class00(uint32_t postedEventVec);
static void _Noreturn task01_class00(uint32_t postedEventVec);
static void _Noreturn task00_class01(uint32_t postedEventVec);
 
 
/*
 * Data definitions
 */
 
static _Alignas(uint64_t) uint8_t _taskStack00_C0[STACK_SIZE_TASK00_C0]
                                , _taskStack01_C0[STACK_SIZE_TASK01_C0]
                                , _taskStack00_C1[STACK_SIZE_TASK00_C1];

static volatile unsigned int _noLoopsTask00_C0 = 0;
static volatile unsigned int _noLoopsTask01_C0 = 0;
static volatile unsigned int _noLoopsTask00_C1 = 0;


/*
 * Function implementation
 */

/**
 * One of the low priority tasks in this test case.
 *   @param initCondition
 * Which events made the task run the very first time?
 *   @remark
 * A task function must never return; this would cause a reset.
 */ 
static void _Noreturn task00_class00(uint32_t initCondition ATTRIB_UNUSED)
{
    for(;;)
    {
        ++ _noLoopsTask00_C0;

        /* This tasks cycles with about 200ms but it is nearly always suspended and doesn't
           produce significant CPU load. */
        rtos_delay(160);
        rtos_suspendTaskTillTime(/* deltaTimeTillRelease */ 200);
    }
} /* End of task00_class00 */





/**
 * Second task of low priority in this test case.
 *   @param initCondition
 * Which events made the task run the very first time?
 *   @remark
 * A task function must never return; this would cause a reset.
 */ 
static void _Noreturn task01_class00(uint32_t initCondition ATTRIB_UNUSED)
{
    for(;;)
    {
        ++ _noLoopsTask01_C0;

        /* For test purpose only: This task consumes the CPU for most of the cycle time and
           while the task is active. The call of delay produces a CPU load of about 80%.
           This is less self-explaining as it looks on the first glance. The Arduino
           function delay is implemented as loop, which compares the current system time
           with a target time, the desired time of return. The current system time is
           clocked by an interrupt independent of RTuinOS. This loop will basically run
           during 80 ms in a cycle of about 100 ms - but not continuously. The task of
           higher priority will frequently interrupt and shortly halt the loop. Therefore
           the 80% of CPU load do not result from this task (as it may seem) but from this
           task and all others which may interrupt it while it is looping inside delay. */
        delay(80 /*ms*/);
        
        /* This tasks cycles with about 100ms. */
        rtos_suspendTaskTillTime(/* deltaTimeTillRelease */ 100);
    }
} /* End of task01_class00 */





/**
 * Task of high priority.
 *   @param initCondition
 * Which events made the task run the very first time?
 *   @remark
 * A task function must never return; this would cause a reset.
 */ 
static void task00_class01(uint32_t initCondition ATTRIB_UNUSED)
{
    for(;;)
    {
        ++ _noLoopsTask00_C1;

        /* This tasks cycles with about 2 ms. */
        //u = rtos_delay(255);
        rtos_suspendTaskTillTime(/* deltaTimeTillRelease */ 2);
    }
} /* End of task00_class01 */





/**
 * The initalization of the RTOS tasks and general board initialization.
 */ 

void setup(void)
{
    /* Print standard greeting of RTuinOS applications. */
    iprintf(RTOS_EOL RTOS_RTUINOS_STARTUP_MSG RTOS_EOL);
    
    /* All tasks are set up. */
    
    /* Task 0 of priority class 0 */
    rtos_initializeTask( /* idxTask */          0
                       , /* taskFunction */     task00_class00
                       , /* prioClass */        0
                       , /* pStackArea */       &_taskStack00_C0[0]
                       , /* stackSize */        sizeof(_taskStack00_C0)
                       , /* startEventMask */   RTOS_EVT_DELAY_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     5
                       );

    /* Task 1 of priority class 0 */
    rtos_initializeTask( /* idxTask */          1
                       , /* taskFunction */     task01_class00
                       , /* prioClass */        0
                       , /* pStackArea */       &_taskStack01_C0[0]
                       , /* stackSize */        sizeof(_taskStack01_C0)
                       , /* startEventMask */   RTOS_EVT_DELAY_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     15
                       );

    /* Task 0 of priority class 1 */
    rtos_initializeTask( /* idxTask */          2
                       , /* taskFunction */     task00_class01
                       , /* prioClass */        1
                       , /* pStackArea */       &_taskStack00_C1[0]
                       , /* stackSize */        sizeof(_taskStack00_C1)
                       , /* startEventMask */   RTOS_EVT_DELAY_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     5
                       );
    
} /* End of setup */




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
    fputs("RTuinOS is idle\r\n", stdout);
    iprintf("_noLoopsTask00_C0: %u\r\n", _noLoopsTask00_C0);
    iprintf("_noLoopsTask01_C0: %u\r\n", _noLoopsTask01_C0);
    iprintf("_noLoopsTask00_C1: %u\r\n", _noLoopsTask00_C1);
    mai_blink(4);
    
} /* End of loop */
