/**
 * @file tc10_systemLoad.c
 *   Test case 10 of RTuinOS. Three tasks with known CPU consumption are scheduled. A
 * simple system load estimation is compared to the the known CPU consumption of the
 * tasks.\n
 *   The system load estimation routine can be used in many other RTuinOS applications. The
 * prerequisite is that the idle task is not used in the application. Or outermost for very
 * infrequently done things. Most of the idle time is consumed by the system load
 * estimation.\n
 *   To reuse the load estimation in your application copy the files gsl_systemLoad.c/h to
 * your application and see the idle task implementation here to find out how to apply
 * the code.\n
 *   Observations:\n
 *   The system load is displayed alternatingly as 51%-52% or 63%-64%. The known loads by
 * the tasks are: 6%, 23% and 20%. Every few seconds one of the tasks produced an
 * additional load of 12% for a few seconds. Additional system load is introduced by the
 * system (the scheduler), by the Arduino interrupts and by the implementation of the task
 * functions (some simple loop constructs). These additional terms can not be predicted. If
 * we compare the known terms with the measured results, we find about 3% in sum for these
 * addends.\n
 *   The mentioned loads are produced by Arduino's \a delayMicroseconds. This function uses
 * a loop of known number of CPU clock ticks to execute. The time till return really
 * consumes the CPU for the specified time, any kind of interruption (by Arduino
 * interrupts, by RTuinOS task switches) is additional. If a regular task uses this
 * function it is clearly defined how much CPU load it causes in percent but it is open
 * how long (i.e. world time) the function will take to return.\n
 *   Arduino's function \a delay must not be used to produce a defined load: It measures
 * the world time till return. If the task invoking \a delay is interrupted it doesn't
 * produce CPU load (another task does) but after reactivation of the task \a delay might
 * nonetheless be satisfied and would return if it sees that enough time has gone by.
 * Therefore, if we'd applied \a delay here instead of \a delayMicroseconds we could not
 * predict the total system load by adding the loads of the distinct tasks.\n
 *   The observation window (i.e. the averaging time) of the system load measurement is
 * about 1 s of world time. The measurement is reliable only, if this time span captures a
 * number of repetitions of the complete task activation pattern. If only regular tasks are
 * implemented the slowest task should have a repetition time of significantly less than
 * 1 s. In this sample the slowest regular task has a cycle time of about 250 ms. By the way,
 * it's straight forward to prolong the averaging time of gsl_getSystemLoad(void) if an
 * RTuinOS application would require this because of very slow regular tasks.\n
 *   Besides measuring the current system load, \a loop is used to let the Arduino LED
 * blink. This is basically useless but demonstrates that the idle task is available to
 * other (infrequent) jobs even if gsl_getSystemLoad(void) is applied.
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
 *   taskT0C0
 *   taskT0C1
 *   taskT0C2
 */

/*
 * Include files
 */

#include <stdio.h>
#include <assert.h>

#include "ihw_initMcuCoreHW.h"
#include "del_delay.h"
#include "mai_main.h"
#include "rtos.h"
#include "gsl_systemLoad.h"


/*
 * Defines
 */

/** The number of interrupt levels, we use in this application is required for an
    estimation of the appropriate stack sizes.\n
      We have 2 interrupts for the serial interface and the RTOS system timer. */
#define NO_IRQ_LEVELS_IN_USE    3

/** The stack usage by the application tasks itself; interrupts disregarded here. */
#define STACK_USAGE_IN_BYTE     256

/** The stack size. */
#define STACK_SIZE_IN_BYTE \
            (RTOS_REQUIRED_STACK_SIZE_IN_BYTE(STACK_USAGE_IN_BYTE, NO_IRQ_LEVELS_IN_USE))

/** The number of system timer ticks required to implement the time span given in Milliseconds.
    Consider to use an expression like \a TIME_IN_MS(10) as argument to the time
    related RTuinOS API functions in order to get readable code.
      @param tiInMs
      The desired time in Milliseconds.
      @remark
    The double operations are limited to the compile time if the argument of the macro is a
    literal. No double operation is then found in the machine code. Never use this macro
    with runtime expressions! */
#define TIME_IN_MS(tiInMs) ((unsigned int)((double)(tiInMs)/RTOS_TICK_MS+0.5))



/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
static void taskT0C0(uint32_t taskParam);
static void taskT0C1(uint32_t taskParam);
static void taskT0C2(uint32_t taskParam);

 
/*
 * Data definitions
 */
 
static _Alignas(uint64_t) uint8_t _taskStackT0C0[STACK_SIZE_IN_BYTE]
                                , _taskStackT0C1[STACK_SIZE_IN_BYTE]
                                , _taskStackT0C2[STACK_SIZE_IN_BYTE];


/*
 * Function implementation
 */

/**
 * A load producing task.
 *   @param initCondition
 * The task gets an initialization parameter for whatever configuration purpose.
 *   @remark
 * A task function must never return; this would cause a reset.
 */ 
static void taskT0C0(uint32_t taskCondition ATTRIB_UNUSED)
{
#define TI_CYCLE_MS 250 /** Cycle time of regular task. */

    uint16_t cnt=0;
    uint32_t ti = millis()
           , tiCycle;
    
    for(;;)
    {
        rtos_delay(35);
      
        del_delayMicroseconds(/* tiDelayInuS */ 15*1000u); /* 15 of 250 ms, i.e. 6% load. */
        if(++cnt >= 40)
        {
            del_delayMicroseconds(/* tiDelayInuS */ 30*1000u);/* 30 of 250ms, i.e. 12% load. */
            if(cnt >= 80)
                cnt = 0;
        }

        rtos_suspendTaskTillTime(/* deltaTimeTillRelease */ TIME_IN_MS(TI_CYCLE_MS));
        tiCycle = millis();
#ifndef NDEBUG
        float tiCycleRel = (tiCycle-ti)
                           * (1.0/1000.0 / (TIME_IN_MS(TI_CYCLE_MS)/RTOS_TICK_FREQUENCY));
        assert(tiCycleRel >= 0.9  &&  tiCycleRel <= 1.1);
#endif
        ti = tiCycle;
    }
#undef TI_CYCLE_MS
} /* End of taskT0C0 */





/**
 * A load producing task.
 *   @param initCondition
 * The task gets an initialization parameter for whatever configuration purpose.
 *   @remark
 * A task function must never return; this would cause a reset.
 */ 
static void taskT0C1(uint32_t taskCondition ATTRIB_UNUSED)
{
#define TI_CYCLE_MS 30

    uint32_t ti = millis()
           , tiCycle;
    
    while(rtos_suspendTaskTillTime(/* deltaTimeTillRelease */ TIME_IN_MS(TI_CYCLE_MS)))
    {
        tiCycle = millis();
#ifdef DEBUG
        float tiCycleRel = (tiCycle-ti)
                           * (1.0/1000.0 / (TIME_IN_MS(TI_CYCLE_MS)/RTOS_TICK_FREQUENCY));
        assert(tiCycleRel >= 0.9  &&  tiCycleRel <= 1.1);
#endif
        rtos_delay(TIME_IN_MS(3));  /* Delay without load. */
        del_delayMicroseconds(/* tiDelayInuS */ 7 * 1000u); /* 7 of 30 ms, i.e. 23% load. */
        rtos_delay(TIME_IN_MS(7));  /* Delay without load. */

        ti = tiCycle;
    }
    
#undef TI_CYCLE_MS
} /* End of taskT0C1 */





/**
 * A load producing task.
 *   @param initCondition
 * The task gets an initialization parameter for whatever configuration purpose.
 *   @remark
 * A task function must never return; this would cause a reset.
 */ 
static void taskT0C2(uint32_t taskCondition ATTRIB_UNUSED)
{
#define TI_CYCLE_MS 10

    uint32_t ti = millis()
           , tiCycle;
    
    while(rtos_suspendTaskTillTime(/* deltaTimeTillRelease */ TIME_IN_MS(TI_CYCLE_MS)))
    {
        tiCycle = millis();
#ifdef DEBUG
        float tiCycleRel = (tiCycle-ti)
                           * (1.0/1000.0 / (TIME_IN_MS(TI_CYCLE_MS)/RTOS_TICK_FREQUENCY));
#endif
        /* The boundaries for the test need to be wider here; we have a resolution of
           millis() of 1 ms in relation to the cycle time of 10 ms, the basic accuracy of
           the computation itself is thus only 10%. */
        assert(tiCycleRel >= 0.8  &&  tiCycleRel <= 1.2);

        del_delayMicroseconds(/* tiDelayInuS */ 2 * 1000u); /* 2 of 10 ms, i.e. 20% load. */
        rtos_delay(TIME_IN_MS(2));  /* Delay without load. */

        ti = tiCycle;
    }
    
#undef TI_CYCLE_MS
} /* End of taskT0C2 */





/**
 * The initalization of the RTOS tasks and general board initialization.
 */ 
void setup(void)
{
    /* Print standard greeting of RTuinOS applications. */
    iprintf(RTOS_EOL RTOS_RTUINOS_STARTUP_MSG RTOS_EOL);
    
    /* Configure task 0 of priority class 0 */
    uint8_t idxTask = 0
          , idxClass = 0;
    rtos_initializeTask( /* idxTask */          idxTask++
                       , /* taskFunction */     taskT0C0
                       , /* prioClass */        idxClass++
                       , /* pStackArea */       &_taskStackT0C0[0]
                       , /* stackSize */        sizeof(_taskStackT0C0)
                       , /* startEventMask */   RTOS_EVT_ABSOLUTE_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     5
                       );
    rtos_initializeTask( /* idxTask */          idxTask++
                       , /* taskFunction */     taskT0C1
                       , /* prioClass */        idxClass++
                       , /* pStackArea */       &_taskStackT0C1[0]
                       , /* stackSize */        sizeof(_taskStackT0C1)
                       , /* startEventMask */   RTOS_EVT_ABSOLUTE_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     2
                       );
    rtos_initializeTask( /* idxTask */          idxTask++
                       , /* taskFunction */     taskT0C2
                       , /* prioClass */        idxClass++
                       , /* pStackArea */       &_taskStackT0C2[0]
                       , /* stackSize */        sizeof(_taskStackT0C2)
                       , /* startEventMask */   RTOS_EVT_ABSOLUTE_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     25
                       );
                       
    /* Test if GCC actually recognizes the constant expression. */
    assert(__builtin_constant_p(TIME_IN_MS(/* tiInMs */ 37)));

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
    /* The system load is computed in the idle task. */
    
    /* Compute the system load. Caution, this function may take a very long time to return
       in case of system loads close to 100%; normally, it takes about a second to return,
       this is the averaging time. */
    unsigned int systemLoad = gsl_getSystemLoad();
    iprintf("System load: %u%%\r\n", (systemLoad+5)/10);  /* Round to percent. */

    mai_blink(1);
    
    assert(rtos_getTaskOverrunCounter(0, false) == 0);
    assert(rtos_getTaskOverrunCounter(1, false) == 0);
    assert(rtos_getTaskOverrunCounter(2, false) == 0);

} /* End of loop */




