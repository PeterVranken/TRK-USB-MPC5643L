/**
 * @file tc15_semaphores.c
 *   Test case tc15 has been created to catch a problem found in a pre-release of RTuinOS
 * 1.0. It's used as regression test only and doesn't do anything exciting or instructive.
 * It double-checks the correctness of semaphore counting balances. The test results are
 * checked by assertion, it's useless to compile this code in production configuation.
 *
 * Copyright (C) 2013-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   taskT0C3
 *   taskT0C2
 *   taskT0C1
 *   taskT0C0
 */

/*
 * Include files
 */

#include <stdio.h>
#include <assert.h>

#include "ihw_initMcuCoreHW.h"
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


/*
 * Local type definitions
 */


/*
 * Local prototypes
 */


/*
 * Data definitions
 */

static _Alignas(uint64_t) uint8_t _stackT0C3[STACK_SIZE_IN_BYTE]
                                , _stackT0C2[STACK_SIZE_IN_BYTE]
                                , _stackT0C1[STACK_SIZE_IN_BYTE]
                                , _stackT0C0[STACK_SIZE_IN_BYTE];

static volatile uint32_t _noLoopsT0C3 = 0
                       , _noLoopsT0C2 = 0
                       , _noLoopsT0C1 = 0
                       , _noLoopsT0C0 = 0;

unsigned int rtos_semaphoreAry[RTOS_NO_SEMAPHORE_EVENTS] = {0, 0, 0, 0, 0, 0, 0, 0};


/*
 * Function implementation
 */

/**
 * Task of highest priority. It's a regular task, which generates semaphore counts.
 *   @param initialResumeCondition
 * The vector of events, which made this task initially due.
 */
static void taskT0C3(uint32_t initialResumeCondition ATTRIB_UNUSED)
{
    do
    {
        /* All events 0..7 are defined as semaphores. Increment count of some of these. The
           sent set is partly demanded by several other tasks. We can double-check if the
           one of higher priority gets it. */
        rtos_sendEvent(0x0E);

        /* Wait a bit and give chance to other due tasks of lower priority to become
           active. */
        rtos_delay(1 /* unit RTOS_TICK */);

        /* Increment count of other semaphores. */
        rtos_sendEvent(0x70);
        
        /* By counting the loops in between the two sendEvent operations and shortly
           suspending this task, we can double-check at the receiving task, if the instance
           of resuming is correct. */ 
        ++ _noLoopsT0C3;
        rtos_delay(1 /* unit RTOS_TICK */);
        
        /* Increment count of other semaphores. */
        rtos_sendEvent(0x80);
    }
    while(rtos_suspendTaskTillTime(/* deltaTimeTillResume */ 5 /* unit RTOS_TICK */));

} /* End of taskT0C3 */



/**
 * Task of medium priority. It's a got-semaphore triggered task, which consumes semaphore
 * counts.
 *   @param initialResumeCondition
 * The vector of events, which made this task initially due.
 */
static void taskT0C2(uint32_t initialResumeCondition ATTRIB_UNUSED)
{
    for(;;)
    {
#ifdef DEBUG
        uint16_t gotEventsVec =
#endif
        rtos_waitForEvent( /* eventMask */ 0x07 | RTOS_EVT_DELAY_TIMER
                         , /* all       */ false
                         , /* timeout   */ 6 /* unit RTOS_TICK */
                         );
        assert(gotEventsVec == 0x06);
        
        assert(_noLoopsT0C2 == _noLoopsT0C3);
        ++ _noLoopsT0C2;
    }
} /* End of taskT0C2 */



/**
 * Task of lower priority. It's a got-semaphore triggered task, which consumes semaphore
 * counts.
 *   @param initialResumeCondition
 * The vector of events, which made this task initially due.
 */

static void taskT0C1(uint32_t initialResumeCondition ATTRIB_UNUSED)
{
    for(;;)
    {
#ifdef DEBUG
        uint16_t gotEventsVec =
#endif
        rtos_waitForEvent( /* eventMask */ 0x0F | RTOS_EVT_DELAY_TIMER
                         , /* all       */ false
                         , /* timeout   */ 6 /* unit RTOS_TICK */
                         );

        /* This task of lower priority just got one of the sent semaphores, the two others,
           which are also requested by T0C2 went to that task as it has the higher
           priority. */
        assert(gotEventsVec == 0x08);

        assert(_noLoopsT0C1 == _noLoopsT0C3);
        ++ _noLoopsT0C1;
    }
} /* End of taskT0C1 */




/**
 * Task of lowest priority. It's a got-semaphore triggered task, which consumes semaphore
 * counts.
 *   @param initialResumeCondition
 * The vector of events, which made this task initially due.
 */

static void taskT0C0(uint32_t initialResumeCondition ATTRIB_UNUSED)
{
    for(;;)
    {
#ifdef DEBUG
        uint16_t gotEventsVec =
#endif
        rtos_waitForEvent( /* eventMask */ 0xF0 | RTOS_EVT_DELAY_TIMER
                         , /* all       */ true
                         , /* timeout   */ 6 /* unit RTOS_TICK */
                         );
        assert(gotEventsVec == 0xF0);
        
        ++ _noLoopsT0C0;
        assert(_noLoopsT0C0 == _noLoopsT0C3);
    }
} /* End of taskT0C0 */




/**
 * Initialization of system, particularly specification of tasks and their properties.
 */

void setup()
{
    /* Print standard greeting of RTuinOS applications. */
    iprintf(RTOS_EOL RTOS_RTUINOS_STARTUP_MSG RTOS_EOL);
    
    rtos_initializeTask( /* idxTask          */ 0
                       , /* taskFunction     */ taskT0C0
                       , /* prioClass        */ 0
                       , /* pStackArea       */ _stackT0C0
                       , /* stackSize        */ sizeof(_stackT0C0)
                       , /* startEventMask   */ RTOS_EVT_DELAY_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout     */ 1
                       );

    rtos_initializeTask( /* idxTask          */ 1
                       , /* taskFunction     */ taskT0C1
                       , /* prioClass        */ 1
                       , /* pStackArea       */ _stackT0C1
                       , /* stackSize        */ sizeof(_stackT0C1)
                       , /* startEventMask   */ RTOS_EVT_DELAY_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout     */ 1
                       );

    rtos_initializeTask( /* idxTask          */ 2
                       , /* taskFunction     */ taskT0C2
                       , /* prioClass        */ 2
                       , /* pStackArea       */ _stackT0C2
                       , /* stackSize        */ sizeof(_stackT0C2)
                       , /* startEventMask   */ RTOS_EVT_DELAY_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout     */ 1
                       );

    rtos_initializeTask( /* idxTask          */ 3
                       , /* taskFunction     */ taskT0C3
                       , /* prioClass        */ 3
                       , /* pStackArea       */ _stackT0C3
                       , /* stackSize        */ sizeof(_stackT0C3)
                       , /* startEventMask   */ RTOS_EVT_DELAY_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout     */ 1
                       );

} /* End of setup */




/**
 * The idle task loop function. Is cyclically invoked by the RTuinOS kernel if no other
 * task is due.
 */

void loop()
{
    static const char idleMsg[] = "RTuinOS is idle" RTOS_EOL;
    fputs(idleMsg, stdout);

    unsigned int cpuLoad = gsl_getSystemLoad();
    printf("CPU load: %3u.%u%%" RTOS_EOL, cpuLoad/10, cpuLoad%10);
    
    ihw_suspendAllInterrupts();
    uint32_t noLoops = _noLoopsT0C2;
    ihw_resumeAllInterrupts();
    printf("%5lu test cycles after %7lu ms" RTOS_EOL, noLoops, millis());

} /* End of loop */







