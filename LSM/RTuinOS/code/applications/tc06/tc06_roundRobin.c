/**
 * @file tc06_roundRobin.c
 *   Test case 06 of RTuinOS. Two round robin tasks of same priority are defined. Task
 * switches are controlled by manually posted and time-slice-elapsed events and counted and
 * reported in the idle task. The sample tests correct priority handling when activating
 * resumed tasks and demonstrates how difficult to predict task timing becomes if round
 * robin time slices are in use. Here we have a task which seems to be regular on the first
 * glance but the round robin strategy introduces significant uncertainties. See comments
 * below.\n
 *   The test success is mainly checked by many assertions. The task overruns reported in
 * the console output for task index 1 are unavoidable and no failure. (The function
 * \a rtos_getTaskOverrunCounter is applicable only for simple, regular tasks.) 
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
 *   rtos_enableIRQTimerTic
 *   loop
 * Local functions
 *   subRoutine
 *   task00_class00
 *   task01_class00
 *   task00_class01
 */

/*
 * Include files
 */

#include <stdio.h>
#include <assert.h>
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
 
static _Noreturn void task00_class00(uint32_t postedEventVec);
static _Noreturn void task01_class00(uint32_t postedEventVec);
static _Noreturn void task00_class01(uint32_t postedEventVec);
static void subRoutine(unsigned int);
 
 
/*
 * Data definitions
 */
 
static _Alignas(uint64_t) uint8_t _taskStack00_C0[STACK_SIZE_IN_BYTE]
                                , _taskStack01_C0[STACK_SIZE_IN_BYTE]
                                , _taskStack00_C1[STACK_SIZE_IN_BYTE];

static volatile unsigned int _noLoopsTask00_C0 = 0;
static volatile unsigned int _noLoopsTask01_C0 = 0;
static volatile unsigned int _noLoopsTask00_C1 = 0;
 
static volatile unsigned int _task00_C0_cntWaitTimeout = 0;
static volatile unsigned int _touchedBySubRoutine = 0;

 
/*
 * Function implementation
 */


/**
 * A sub routine which has the only meaning of consuming stack - in order to test the stack
 * usage computation.
 *   @param nestedCalls
 * The routine will call itself recursively \a nestedCalls-1 times. In total the stack will
 * be burdened by \a nestedCalls calls of this routine.
 *   @remark
 * The optimizer removes the recursion completely. The stack-use effect of the sub-routine
 * is very limited, but still apparent the first time it is called.
 */ 
static void subRoutine(unsigned int nestedCalls)
{
    volatile uint8_t stackUsage[43];
    if(nestedCalls > 1)
    {
        _touchedBySubRoutine += 2;
        stackUsage[0] = 
        stackUsage[sizeof(stackUsage)-1] = 0;
        subRoutine(nestedCalls-1);    
    }
    else
    {
        ++ _touchedBySubRoutine;
        stackUsage[0] = 
        stackUsage[sizeof(stackUsage)-1] = nestedCalls;
    }
} /* End of subRoutine */





/**
 * One of the low priority round robin tasks in this test case.
 *   @param initCondition
 * Which events made the task run the very first time?
 *   @remark
 * A task function must never return; this would cause a reset.
 */ 
static _Noreturn void task00_class00(uint32_t initCondition ATTRIB_UNUSED)
{
    for(;;)
    {
        ++ _noLoopsTask00_C0;

        /* To see the stack reserve computation working we invoke a nested sub-routine
           after a while. */
        if(millis() > 20000ul)
            subRoutine(1);
        if(millis() > 30000ul)
            subRoutine(2);
        if(millis() > 40000ul)
            subRoutine(3);
        
        /* The next operation (Arduino delay function) takes the demanded world time in ms
           (as opposed to CPU time) even if it is interrupted because of an elapsed round
           robin counter.
             This task has a round robin time slice of 10 tics (20 ms) only, so it should
           surely be interrupted during execution of delay. The other round robin task has
           a time slice of 4 ms. No other tasks demand the CPU significantly. Consequently,
           the code in delay should not be interrupted for longer than about 4 ms. Coming
           back here means to immediately do the next check if the demanded time has
           elapsed. We expect thus to not prolongue the demanded time by more than about 4
           ms. */
#ifdef DEBUG
        uint32_t ti0 = millis();
#endif
        delay(600 /* ms */);
#ifdef DEBUG
        uint16_t dT = (uint16_t)(millis() - ti0);
        assert(dT >= 599);
        assert(dT < 609);
#endif

        /* Wait for an event from the idle task. The idle task is asynchrounous and its
           speed depends on the system load. The behavior is thus not perfectly
           predictable. Let's have a look on the overrrun counter for this task. It might
           occasionally be incremented. */
        if(rtos_waitForEvent( /* eventMask */ RTOS_EVT_EVENT_03 | RTOS_EVT_DELAY_TIMER
                            , /* all */ false
                            , /* timeout */ 1000 /* unit 2 ms */
                            )
           == RTOS_EVT_DELAY_TIMER
          )
        {
            ++ _task00_C0_cntWaitTimeout;
        }
    }
} /* End of task00_class00 */





/**
 * Second round robin task of low priority in this test case.
 *   @param initCondition
 * Which events made the task run the very first time?
 *   @remark
 * A task function must never return; this would cause a reset.
 */ 
static _Noreturn void task01_class00(uint32_t initCondition ATTRIB_UNUSED)
{
    uint32_t tiCycle0 = millis();
    for(;;)
    {
        unsigned int u;
        
        ++ _noLoopsTask01_C0;

        /* The next operation (Arduino delay function) takes the demanded world time in ms
           (as opposed to CPU time) even if it is interrupted because of an elapsed round
           robin counter.
             As this task has a round robin time slice of 4 ms, the delay operation will
           surely be interrupted by the other task - which may consume the CPU for up to 20
           ms. The delay operation may thus return after 24 ms. */
        uint32_t ti0 = millis();
        delay(8 /* ms */);
        uint16_t dT = (uint16_t)(millis() - ti0);
        assert(dT >= 7);
        assert(dT <= 25);

        /* Release the high priority task for a single cycle. It should continue operation
           before we leave the suspend function here. Check it. */
        ti0 = millis();
        u = _noLoopsTask00_C1;
        rtos_sendEvent(/* eventVec */ RTOS_EVT_EVENT_00);
        assert(u+1 == _noLoopsTask00_C1);
        assert(_noLoopsTask01_C0 == _noLoopsTask00_C1);
        dT = (uint16_t)(millis() - ti0);
        assert(dT <= 2);
        
        /* The body of this task takes up to about 26 ms (see before). If it suspends here,
           the other round robin task will most often become active and consume the CPU the
           next 20 ms. This tasks wants to cycle with 40 ms. So it'll become due while the
           other round robin task is active. This task will become active only after the
           time slice of the other task has elapsed. Exact cycle time is impossible for
           this task.
             It can even be worse if the other round robin task should be suspendend while
           this task suspends itself till the next multiple of 40 ms: Occasionally, the
           other task will resume just before this task and the activation of this task
           will be delayed by the full time slice duration of the other round robin task.
           Task overruns are unavoidable for this (ir-)regular task, but we can give an
           upper boundary for the cycle time, which is tested by assertion. */
        rtos_suspendTaskTillTime(/* deltaTimeTillRelease */ 20 /* unit 2 ms */);
        uint32_t tiCycleEnd = millis();
        dT = (uint16_t)(tiCycleEnd - tiCycle0);
        tiCycle0 = tiCycleEnd;
        assert(dT <= 62);
    }
} /* End of task01_class00 */





/**
 * Task of high priority.
 *   @param initCondition
 * Which events made the task run the very first time?
 *   @remark
 * A task function must never return; this would cause a reset.
 */ 
static _Noreturn void task00_class01(uint32_t initCondition ATTRIB_DBG_ONLY)
{
    assert(initCondition == RTOS_EVT_EVENT_00);
    
    /* This tasks cycles once each time it is awaked by the event. The timeout condition
       must be weak: The triggering task seems to have a cycle time 40 ms on the first
       glance, but there's an uncertainty in the magnitude of the round robin time slice
       duration of the second, concurring task. Although this leads to an upper boundary of
       about 60 ms for the (irregular) cycle time of the triggering task, the uncertainty
       here is even larger: The point in time of the trigger event relative to the begin of
       a cycle does also vary in the magnitude of the other round robin's time slice. The
       maximum distance in time of two trigger events can thus be accordingly larger in the
       worsed case. */
    do
    {
        /* As long as we stay in the loop we didn't see a timeout. */
        
        /* Count the loops. */
        ++ _noLoopsTask00_C1;
    }
    while(rtos_waitForEvent( /* eventMask */ RTOS_EVT_EVENT_00 | RTOS_EVT_DELAY_TIMER
                           , /* all */ false
                           , /* timeout */ (62+20)/2 /* unit 2 ms */
                           )
          == RTOS_EVT_EVENT_00
         );
    
    /* We must never get here. Otherwise the test case failed. In compilation mode
       PRODUCTION, when there's no assertion, we will see an immediate reset because we
       leave a task function. */
    assert(false);

} /* End of task00_class01 */





/**
 * The initalization of the RTOS tasks and general board initialization.
 */ 

void setup(void)
{
    /* Print standard greeting of RTuinOS applications. */
    iprintf(RTOS_EOL RTOS_RTUINOS_STARTUP_MSG RTOS_EOL);

    /* Task 0 of priority class 0 */
    rtos_initializeTask( /* idxTask */          0
                       , /* taskFunction */     task00_class00
                       , /* prioClass */        0
#if RTOS_ROUND_ROBIN_MODE_SUPPORTED == RTOS_FEATURE_ON
                       , /* timeRoundRobin */   10
#endif
                       , /* pStackArea */       &_taskStack00_C0[0]
                       , /* stackSize */        sizeof(_taskStack00_C0)
                       , /* startEventMask */   RTOS_EVT_DELAY_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     0
                       );

    /* Task 1 of priority class 0 */
    rtos_initializeTask( /* idxTask */          1
                       , /* taskFunction */     task01_class00
                       , /* prioClass */        0
#if RTOS_ROUND_ROBIN_MODE_SUPPORTED == RTOS_FEATURE_ON
                       , /* timeRoundRobin */   2
#endif
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
#if RTOS_ROUND_ROBIN_MODE_SUPPORTED == RTOS_FEATURE_ON
                       , /* timeRoundRobin */   0
#endif
                       , /* pStackArea */       &_taskStack00_C1[0]
                       , /* stackSize */        sizeof(_taskStack00_C1)
                       , /* startEventMask */   RTOS_EVT_EVENT_00
                       , /* startByAllEvents */ false
                       , /* startTimeout */     0
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
    uint8_t idxStack;
    
    /* An event can be posted even if nobody is listening for it. */
    rtos_sendEvent(/* eventVec */ RTOS_EVT_EVENT_04);

    /* This event will release task 0 of class 0. However we do not get here again fast
       enough to avoid all timeouts in that task. */
    rtos_sendEvent(/* eventVec */ RTOS_EVT_EVENT_03);

    iprintf("RTuinOS is idle");
    iprintf("noLoopsTask00_C0: %u\r\n", _noLoopsTask00_C0);
    iprintf("_task00_C0_cntWaitTimeout: %u\r\n", _task00_C0_cntWaitTimeout);
    iprintf("noLoopsTask01_C0: %u\r\n", _noLoopsTask01_C0);
    iprintf("noLoopsTask00_C1: %u\r\n", _noLoopsTask00_C1);

    /* Look for the stack usage and task overruns. (The task concept implemented here
       brings such overruns for task 1.) */
    for(idxStack=0; idxStack<RTOS_NO_TASKS; ++idxStack)
    {
        iprintf( "Stack reserve of task %u: %u, task overrun: %u\r\n"
               , idxStack
               , rtos_getStackReserve(idxStack)
               , rtos_getTaskOverrunCounter(idxStack, /* doReset */ false)
               );
    }
    
    mai_blink(2);
    
} /* End of loop */




