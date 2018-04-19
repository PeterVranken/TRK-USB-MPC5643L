/**
 * @file tc08_applInterrupt.c
 * Test case 08 of RTuinOS. Two timers different to the RTuinOS system timer are installed
 * as additional task switch causing interrupt sources. These interrupts set an individual
 * event which triggers an associated task of high priority. The interrupt events of the
 * associated tasks are counted to demonstrate the operation.\n
 *   A dedicated task is used for feedback. The Arduino LED signals the number of
 * application interrupts. Occasionally, a series of flashes is produced, which represents
 * the number of interrupts so far. (To not overburden the flashes counting human, the
 * length of the series is limited to ten.) This feedback giving task gets active only on
 * demand; it's triggered by an application event from another task.\n
 *   Observations:\n
 *   The frequency of the timer interrupts (timers 4 and 5 have been used) can be varied in
 * a broad range. In this test case the application interrupt 00 is configured to occur
 * with about 1 kHz. This is more than double the frequency of the RTuinOS system clock,
 * which determines the highest frequency of calling regular tasks. Having an even faster
 * application interrupt doesn't matter, the scheduler easily handles task switches faster
 * than the system timer.
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
 *   rtos_enableIRQUser00
 * Local functions
 *   blinkNoBlock
 *   taskT0_C0
 *   taskT0_C1
 *   taskT0_C2
 *   taskT1_C2
 */

/*
 * Include files
 */

#include <stdio.h>
#include <assert.h>

#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
#include "del_delay.h"
#include "mai_main.h"
#include "rtos.h"
#include "tc08_applEvents.h"

/*
 * Defines
 */

/** The number of interrupt levels, we use in this application is required for an
    estimation of the appropriate stack sizes.\n
      We have 2 interrupts for the serial interface and the RTOS system timer
    plus 2 application defined interrupts. */
#define NO_IRQ_LEVELS_IN_USE    5

/** The stack usage by the application tasks itself; interrupts disregarded here. */
#define STACK_USAGE_IN_BYTE     100

/** The stack size. */
#define STACK_SIZE_IN_BYTE \
            (RTOS_REQUIRED_STACK_SIZE_IN_BYTE(STACK_USAGE_IN_BYTE, NO_IRQ_LEVELS_IN_USE))

/** The number of system timer ticks required to implement the time span given in Milliseconds.
      @remark
    The double operations are limited to the compile time. No such operation is found in
    the machine code. */
#define TICKS(tiInMs) ((unsigned int)((double)(tiInMs)/RTOS_TICK_MS+0.5))


/*
 * Local type definitions
 */

/** The task indexes. */
enum { idxTaskT0_C0 = 0
     , idxTaskT0_C1
     , idxTaskT0_C2
     , idxTaskT1_C2
     , noTasks
     };
     

/*
 * Local prototypes
 */

static void taskT0_C0(uint32_t postedEventVec);
static void taskT0_C1(uint32_t postedEventVec);
static void taskT0_C2(uint32_t postedEventVec);
static void taskT1_C2(uint32_t postedEventVec);


/*
 * Data definitions
 */

static _Alignas(uint64_t) uint8_t _stackT0_C0[STACK_SIZE_IN_BYTE]
                                , _stackT0_C1[STACK_SIZE_IN_BYTE]
                                , _stackT0_C2[STACK_SIZE_IN_BYTE]
                                , _stackT1_C2[STACK_SIZE_IN_BYTE];

/** Task owned variables which record what happens. */
static volatile uint32_t _cntLoopsT0_C2 = 0
                       , _cntLoopsT1_C2 = 0;

/** The application interrupt handler counts missing interrupt events (timeouts) as errors. */
static volatile uint16_t _errT0_C2 = 0;

/** Input for the blink-task: If it is triggered, it'll read this variable and produce a
    sequence of flashes of according length. */
static volatile uint8_t _blink_noFlashes = 0;
   

/*
 * Function implementation
 */


/**
 * Trivial routine that flashes the LED a number of times to give simple feedback. The
 * routine is non blocking. It must not be called by the idle task as it uses a suspend
 * command.
 *   @param noFlashes
 * The number of times the LED is lit.
 */

static void blinkNoBlock(uint8_t noFlashes)
{
#define TI_FLASH 250 /* ms */

    while(noFlashes-- > 0)
    {
        lbd_setLED(lbd_led_D4_red, /* isOn */ true);    /* Turn the LED on. */
        rtos_delay(TICKS(TI_FLASH));                    /* The flash time. */
        lbd_setLED(lbd_led_D4_red, /* isOn */ false);   /* Turn the LED off. */
        rtos_delay(TICKS(TI_FLASH));                    /* Time between flashes. */
    }    
    
    /* Wait for two seconds after the last flash - this command could easily be invoked
       immediately again and the bursts need to be separated. */
    rtos_delay(TICKS(2000-TI_FLASH));
    
#undef TI_FLASH
}




/**
 * The task of lowest priority (besides idle) is used for reporting. When released by an
 * event it produces a sequence of flash events of the LED. The number of flashes is
 * determined by the value of the global variable \a _blink_noFlashes. The sequence is
 * released by RTuinOS event \a EVT_START_FLASH_SEQUENCE.
 *   @param initCondition
 * Which events made the task run the very first time?
 *   @remark
 * A task function must never return; this would cause a reset.\n
 *   The number of times the LED is lit is read by side effect from the global variable \a
 * _blink_noFlashes.
 */

static void taskT0_C0(uint32_t initCondition ATTRIB_DBG_ONLY)
{
    assert(initCondition == EVT_START_FLASH_SEQUENCE);
    do
    {
        /* The access to the shared variable is not protected: The variable is an uint8_t
           and a read operation is atomic anyway. */
        blinkNoBlock(_blink_noFlashes);
    }
    while(rtos_waitForEvent(EVT_START_FLASH_SEQUENCE, /* all */ false, /* timeout */ 0));
    
} /* End of taskT0_C0 */




/**
 * A task of medium priority. It looks at the counter incremented by the interrupt handler
 * and reports when it reaches a certain limit. Reporting is done by releasing the blinking
 * task. 
 *   @param initCondition
 * Which events made the task run the very first time?
 *   @remark
 * A task function must never return; this would cause a reset.
 */
 
static void taskT0_C1(uint32_t initCondition ATTRIB_UNUSED)
{
#define TASK_TIME_T0_C1_MS  50 
#define TRIGGER_DISTANCE    8000

    /* Since we are the only client of the blink task we can abuse the interface variable as
       static counter at the same time. The first sequence shall have a single flash. */
    _blink_noFlashes = 0;
    
    /* The task inspects the results of the interrupt on a regular base. */
    do
    {
        static uint32_t lastTrigger_ = TRIGGER_DISTANCE;
        
        /* The use of enter/leaveCriticalSection is a relict from the original
           Arduino implementation, where the counter could not be read in a single atomic
           instruction. Here, in the e200z4 port it is obsolete but must not harm. */
        rtos_enterCriticalSection();
        bool trigger = _cntLoopsT0_C2 >= lastTrigger_;
        rtos_leaveCriticalSection();

        if(trigger)
        {
            /* Next reported event is reached. Start the flashing task. The number of times
               the LED is lit is exchanged by side effect in the global variable
               _blink_noFlashes. Writing this variable doesn't basically require access
               synchronization as this task has a higher priority than the blink task and
               because it's a simple uint8_t. However, we are anyway inside a critical
               section. */
               
            /* Limit the length of the sequence to a still recognizable value.
                 A read-modify-write on the shared variable outside a critical section can
               solely be done since we are the only writing task. */
            if(_blink_noFlashes < 10)
                ++ _blink_noFlashes;
                
            /* TRigger the other task. As it has the lower priority, it's actually not
               activated before we suspend a little bit later. */
            rtos_sendEvent(EVT_START_FLASH_SEQUENCE);
            
            /* Set next trigger point. If we are too slow, it may run away. */
            lastTrigger_ += TRIGGER_DISTANCE;
        }
    }
    while(rtos_suspendTaskTillTime(TICKS(TASK_TIME_T0_C1_MS)));

#undef TASK_TIME_T0_C1_MS
} /* End of taskT0_C1 */




/**
 * A task of high priority is associated with the application interrupts. It counts its
 * occurances and when it is missing (timeout).
 *   @param initCondition
 * Which events made the task run the very first time?
 *   @remark
 * A task function must never return; this would cause a reset.
 */
 
static void taskT0_C2(uint32_t initCondition ATTRIB_UNUSED)
{
#define TIMEOUT_MS  10

    /* This task just reports the application interrupt 00 by incrementing a global counter. */
    while(true)
    {
        while(rtos_waitForEvent( RTOS_EVT_ISR_USER_00 | RTOS_EVT_DELAY_TIMER
                               , /* all */ false
                               , /* timeout */ TICKS(TIMEOUT_MS)
                               )
              == RTOS_EVT_ISR_USER_00
             )
        {
            /* Normal situation: Application interrupt came before timeout. No access
               synchronization is required as this task has the highest priority of all
               data accessors. */
            ++ _cntLoopsT0_C2;
        }
    
        /* Inner loop left because of timeout. This may happen only at system
           initialization, because the application interrupts are always enabled a bit
           later than the RTuinOS system timer interrupt.
              No access synchronization is required as this task has the highest priority
           of all data accessors. */
        if(_errT0_C2 < (uint16_t)-1)
            ++ _errT0_C2;

        /* Outer while condition: No true error recovery, just wait for next application
           interrupt. */
    }
#undef TIMEOUT_MS
} /* End of taskT0_C2 */




static void taskT1_C2(uint32_t initCondition ATTRIB_UNUSED)
{
    /* This task just reports the application interrupt 01 by incrementing a global counter. */
    while(true)
    {
#ifdef DEBUG
        assert(
#endif
               rtos_waitForEvent(RTOS_EVT_ISR_USER_01, /* all */ false, /* timeout */ 0)
#ifdef DEBUG
               == RTOS_EVT_ISR_USER_01
              )
#endif
        ;

        /* No access synchronization is required as this task has the highest priority of all
           data accessors. */
        ++ _cntLoopsT1_C2;

        /* Outer while condition: Wait for next application interrupt. */
    }
#undef TIMEOUT_MS
} /* End of taskT1_C2 */







/**
 * Callback from RTuinOS, rtos_initRTOS(): The application interrupt 00 is configured and
 * released.
 */
void rtos_enableIRQUser00()
{
    /* Disable all PIT timers during configuration. */
    PIT.PITMCR.R = 0x2;

    /* Peripheral clock has been initialized to 120 MHz. To get a 1ms interrupt tick we
       need to count till 120000. We choose  f_irq = 976 Hz. This is about double the system
       clock of RTuinOS in its standard configuration (which is used in this test case).
         -1: See MCU reference manual, 36.5.1, p. 1157. */
    //const unsigned int count = (uint32_t)((1.0f/976.0f) * 120e6); // 122950
    const unsigned int count = 122953; /* Choose close prime number. */
    PIT.LDVAL0.R = (uint32_t)count - 1;

    /* Enable interrupts by this timer and start it. */
    PIT.TCTRL0.R = 0x3;

    /* Enable timer operation and let them be stopped on debugger entry. Note, this is a
       global setting for all four timers, even if we use and reserve only one for the
       RTOS. */
    PIT.PITMCR.R = 0x1;

} /* End of rtos_enableIRQUser00 */





/**
 * Callback from RTuinOS: The application interrupt 01 is configured and released.
 */

void rtos_enableIRQUser01()
{
    /* Disable all PIT timers during configuration. */
    PIT.PITMCR.R = 0x2;

    /* Peripheral clock has been initialized to 120 MHz. To get a 1ms interrupt tick we
       need to count till 120000. We choose  f_irq = 1 Hz.
         -1: See MCU reference manual, 36.5.1, p. 1157. */
    //const unsigned int count = (uint32_t)((1.0f/1.0f) * 120e6); // 120000000
    const unsigned int count = 119999987; /* Choose close prime number. */
    PIT.LDVAL1.R = (uint32_t)count - 1;

    /* Enable interrupts by this timer and start it. */
    PIT.TCTRL1.R = 0x3;

    /* Enable timer operation and let them be stopped on debugger entry. Note, this is a
       global setting for all four timers, even if we use and reserve only one for the
       RTOS. */
    PIT.PITMCR.R = 0x1;

} /* End of rtos_enableIRQUser01 */





/**
 * The initalization of the RTOS tasks and general board initialization.
 */

void setup(void)
{
    /* Print standard greeting of RTuinOS applications. */
    iprintf(RTOS_EOL RTOS_RTUINOS_STARTUP_MSG RTOS_EOL);
    
    uint8_t idxTask = 0;
    rtos_initializeTask( /* idxTask */          idxTask++
                       , /* taskFunction */     taskT0_C0
                       , /* prioClass */        0
                       , /* pStackArea */       &_stackT0_C0[0]
                       , /* stackSize */        sizeof(_stackT0_C0)
                       , /* startEventMask */   EVT_START_FLASH_SEQUENCE
                       , /* startByAllEvents */ false
                       , /* startTimeout */     0
                       );
    rtos_initializeTask( /* idxTask */          idxTask++
                       , /* taskFunction */     taskT0_C1
                       , /* prioClass */        1
                       , /* pStackArea */       &_stackT0_C1[0]
                       , /* stackSize */        sizeof(_stackT0_C1)
                       , /* startEventMask */   RTOS_EVT_DELAY_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     0
                       );
    rtos_initializeTask( /* idxTask */          idxTask++
                       , /* taskFunction */     taskT0_C2
                       , /* prioClass */        2
                       , /* pStackArea */       &_stackT0_C2[0]
                       , /* stackSize */        sizeof(_stackT0_C2)
                       , /* startEventMask */   RTOS_EVT_DELAY_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     0
                       );
    rtos_initializeTask( /* idxTask */          idxTask++
                       , /* taskFunction */     taskT1_C2
                       , /* prioClass */        2
                       , /* pStackArea */       &_stackT1_C2[0]
                       , /* stackSize */        sizeof(_stackT1_C2)
                       , /* startEventMask */   RTOS_EVT_ISR_USER_01
                       , /* startByAllEvents */ false
                       , /* startTimeout */     0
                       );
    assert(idxTask == RTOS_NO_TASKS  &&  noTasks == RTOS_NO_TASKS);

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
    /* Get a safe copy of the volatile global data. */
    rtos_enterCriticalSection();
    uint32_t noInt = _cntLoopsT0_C2;
    uint16_t noTimeout = _errT0_C2;
    rtos_leaveCriticalSection();
            
    iprintf("No application interrupts 00: %lu, timeouts: %hu\r\n", noInt, noTimeout);

    rtos_enterCriticalSection();
    noInt = _cntLoopsT1_C2;
    rtos_leaveCriticalSection();
    
    iprintf("No application interrupts 01: %lu\r\n", noInt);
    
    iprintf( "Stack reserve: %u, %u, %u, %u\r\n"
           , rtos_getStackReserve(/* idxTask */ 0)
           , rtos_getStackReserve(/* idxTask */ 1)
           , rtos_getStackReserve(/* idxTask */ 2)
           , rtos_getStackReserve(/* idxTask */ 3)
           );

    iprintf( "Overrun T0_C1: %u\r\n"
           , rtos_getTaskOverrunCounter(/* idxTask */ idxTaskT0_C1, /* doReset */ false)
           );
    
    /* Don't flood the console windows too much. We anyway show only arbitrarily sampled
       data.
         Caution: Do not use rtos_delay here in the idle task. An attempt to suspend the
       idle task definitely causes a crash. */
    del_delayMicroseconds(800*1000);

} /* End of loop */




