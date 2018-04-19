/**
 * @file tc05_sendEventAndReconfigureSystemTimer.c
 *   Test case 05 of RTuinOS. Several tasks of different priority are defined. Task
 * switches are partly controlled by posted events and counted and reported in the idle
 * task.\n
 *   A task of low priority waits for events posted by the idle task.\n
 *   A task of high priority is triggered once by an event posted by a second task of low
 * priority. The triggering task is a regular task of high frequency. The dependent,
 * triggered task is expected to cycle synchronously.\n
 *   Secondary, and not essential for what has been said before, this test case proves the 
 * possibility to change the system timer clock by configuration, i.e. without changing
 * RTuinOS itself. The clock frequency is changed by configuration; in this sample, RTuinOS
 * is running with a system timer frequency of 0.2 kHz or 5 ms tick duration respectively.
 * Note, the original Arduino implementation had exchanged the interrupt source. The e200z4
 * port doesn't support this. It only offers to configure the interrupt rate.\n
 *   Observations:\n
 * The waitForEvent operation in the slow task T00_C0 times out irregularly. The
 * asynchronous idle task posts the event sometimes but not frequently enough to satisfy
 * the task. Due to the irregularity of the idle task we see more or fewer timeout
 * events.\n
 *   The code inside the tasks proves that the second task of low priority is tightly
 * coupled with the task of high priority. The display of the counters on the console seems
 * to indicate the opposite. However, this is a multitasking effect only: The often
 * interrupted idle task samples the data of the different tasks at different times and
 * does not apply a critical section to synchronize the data.\n
 *   The limitations of the recognition of task overruns can be seen in the slow task
 * T00_C0. It has a cycle time of more than half the system timer (the 8 Bit timer is
 * chosen) and then there's a significant probability of seeing overruns which actually
 * aren't any. The code in the task proves the correct task timing. Note, this effect is
 * not visible in the e200z4 port; here, the system timer generally has 32 Bit.\n
 *   The display of the task stack consumption is demonstrated. To prove operability the
 * task T00_C0 invokes a subroutine only after a while. The console output shows a related
 * decrease of the stack reserve.
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
 *   blink
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

#include "lbd_ledAndButtonDriver.h"
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

static void blink(unsigned int noFlashes);
static void _Noreturn task00_class00(uint32_t postedEventVec);
static void _Noreturn task01_class00(uint32_t postedEventVec);
static void _Noreturn task00_class01(uint32_t postedEventVec);


/*
 * Data definitions
 */

static _Alignas(uint64_t) uint8_t _taskStack00_C0[STACK_SIZE_IN_BYTE]
                                , _taskStack01_C0[STACK_SIZE_IN_BYTE]
                                , _taskStack00_C1[STACK_SIZE_IN_BYTE];

static volatile unsigned int _noLoopsIdleTask = 0;
static volatile unsigned int _noLoopsTask00_C0 = 0;
static volatile unsigned int _noLoopsTask01_C0 = 0;
static volatile unsigned int _noLoopsTask00_C1 = 0;
static volatile unsigned int _task00_C0_cntWaitTimeout = 0;
static volatile unsigned int _task00_C0_trueTaskOverrunCnt = 0;


/*
 * Function implementation
 */


/**
 * Trivial routine that flashes the LED a number of times to give simple feedback. The
 * routine is blocking in the sense that the time it is executed is not available to other
 * tasks. It produces significant system load.
 *   @param noFlashes
 * The number of times the LED is lit.
 */

static void blink(unsigned int noFlashes)
{
#define TI_FLASH 150

    while(noFlashes-- > 0)
    {
        lbd_setLED(lbd_led_D4_red, /* isOn */ true);    /* Turn the LED on. */
        delay(TI_FLASH);                                /* The flash time. */
        lbd_setLED(lbd_led_D4_red, /* isOn */ false);   /* Turn the LED off. */
        delay(TI_FLASH);                                /* Time between flashes. */

        /* Blink takes many hundreds of Milliseconds. To prevent too many timeouts in
           task00_C0 we post the event also inside of blink. */
        rtos_sendEvent(/* eventVec */ RTOS_EVT_EVENT_03);
    }

    /* Wait for a second after the last flash - this command could easily be invoked
       immediately again and the series need to be separated. */
    delay(500);
    rtos_sendEvent(/* eventVec */ RTOS_EVT_EVENT_03);
    delay(500-TI_FLASH);

#undef TI_FLASH
}




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

static volatile uint8_t _touchedBySubRoutine; /* Attempt to discard removal of recursion by
                                                 optimization. */
static void subRoutine(uint8_t nestedCalls)
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




#if 0   /* Overloading the interrupt initialization routine for the system timer of the
           RTOS is no longer supported in the e200z4 port. The concept doesn't fit to the
           interrupt concept of the core. A similar concept is easily implementable:
             The RTOS separates the two elements of its system timer interrupt service
           routine: The acknowledge of the hardware interrupt bit and the scheduler action,
           which is clocked by the interrupt. The latter is offered as public API. We could
           then make it an option, whether the standard interrupt is used or not. If not,
           it is in the responsibility of the application code to provide an interrupt that
           regularly invokes the API. */

/**
 * Test of redefining the central interrupt of RTuinOS. The default implementation of the
 * interrupt configuration function is overridden by redefining the same function.\n
 */
void rtos_enableIRQTimerTick(void)
{
    fputs("Overloaded interrupt initialization rtos_enableIRQTimerTick in " __FILE__, stdout);

#ifdef __AVR_ATmega2560__
    /* Timer 4 is reconfigured. Arduino has put it into 8 Bit fast PWM mode. We need the
       phase and frequency correct mode, in which the frequency can be controlled by
       register OCA. This register is buffered, the CPU writes into the buffer only. After
       each period, the buffered value is read and determines the period duration of the
       next period. The period time (and thus the frequency) can be varied in a well
       defined and glitch free manner. The settings are:
         WGM4 = %1001, the mentioned operation mode is selected. The 4 Bit word is partly
       found in TCCR4A and partly in TCCR4B.
         COM4A/B/C: don't change. The three 2 Bit words determine how to derive up to
       three PWM output signals from the counter. We don't change the Arduino setting; no
       PWM wave form is generated. The three words are found as the most significant 6 Bit
       of register TCCR4A.
         OCR4A = 1 MHz/f_irq, the frequency determining 16 Bit register. OCR4A must not
       be less than 3.
         CS4 = %010, the counter selects the CPU clock divided by 8 as clock. */
    TCCR4A &= ~0x03; /* Lower half word of WGM */
    TCCR4A |=  0x01;

    TCCR4B &= ~0x1f; /* Upper half word of WGM and CS */
    TCCR4B |=  0x12;

    /* We choose OCR4A = 1000, or f_irq = 1000 Hz. This is more than double the system
       clock of RTuinOS in its standard configuration. */
    OCR4A = 1000u;

    TIMSK4 |= 1;    /* Enable overflow interrupt. */
#else
# error Modification of code for other AVR CPU required
#endif

} /* End of rtos_enableIRQTimerTick */
#endif





/**
 * One of the low priority tasks in this test case.
 *   @param initCondition
 * Which events made the task run the very first time?
 *   @remark
 * A task function must never return; this would cause a reset.
 */
static void _Noreturn task00_class00(uint32_t initCondition ATTRIB_UNUSED)
{
    uint32_t ti1, ti2=0;

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

        /* Wait for an event from the idle task. The idle task is asynchrounous and its
           speed depends on the system load. The behavior is thus not perfectly
           predictable. */
        if(rtos_waitForEvent( /* eventMask */ RTOS_EVT_EVENT_03 | RTOS_EVT_DELAY_TIMER
                            , /* all */ false
                            , /* timeout */ 40 /* unit: 5ms */
                            )
           == RTOS_EVT_DELAY_TIMER
          )
        {
            ++ _task00_C0_cntWaitTimeout;
        }

        /* This tasks cycles with the lowest frequency, once per system timer cycle.
            CAUTION: Normally, this is not permitted. If the suspend time is more than
           half the range of the data type chosen for its system time RTuinOS is no longer
           capable to safely recognize task overruns. False recognitions would lead to bad
           task timing as the corrective action is to make the (only seemingly) late task
           due immediately.
             e200z4 port: The uint8 system timer is useless for the 32 Bit CPU and no
           longer supported. We emulate the Arduino behavior by stating 256 time units
           instead of 0. */
        rtos_suspendTaskTillTime(/* deltaTimeTillRelease */ /*0*/ 256);

        /* A task period of more than half the system timer cycle leads to a high
           probability of seeing task overruns where no such overruns happen. (See RTuinOS
           manual.)
             We therefore disable the standard corrective action in case of overruns; macro
           RTOS_OVERRUN_TASK_IS_IMMEDIATELY_DUE is set to RTOS_FEATURE_OFF.
             The false overruns are counted nonetheless by rtos_getTaskOverrunCounter.
           Here, we implement our own overrun counter by comparing the task cycle time with
           the Arduino timer which coexists with the RTuinOS system timer. */
        ti1 = millis();
        if(ti2 > 0)
        {
            ti2 = ti1-ti2;
            if(ti2 < (uint32_t)(0.9*256.0*RTOS_TICK*1000.0)
               ||  ti2 > (uint32_t)(1.1*256.0*RTOS_TICK*1000.0)
              )
            {
                ++ _task00_C0_trueTaskOverrunCnt;
            }
        }
        ti2 = ti1;

        /* What looks like CPU consuming floating point operations actually is a
           compile time operation. Here's the prove - which also produces no CPU load
           as it is removed by the optimizer. (Sounds contradictory but it isn't.) */
        assert(__builtin_constant_p((uint32_t)(0.9*256.0*RTOS_TICK*1000.0))
               &&  __builtin_constant_p((uint32_t)(1.1*256.0*RTOS_TICK*1000.0))
              );

    } /* End for(ever) */

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
        unsigned int u ATTRIB_DBG_ONLY;

        ++ _noLoopsTask01_C0;

        /* For test purpose only: This task consumes the CPU for about 50% of the cycle
           time. */
        delay(5 /*ms*/);

        /* Release high priority task for a single cycle. It should continue operation
           before we return from the suspend function sendEvent. Check it. */
        u = _noLoopsTask00_C1;
        rtos_sendEvent(/* eventVec */ RTOS_EVT_EVENT_00);
        assert(u+1 == _noLoopsTask00_C1);

        /* Double-check that this task keeps in sync with the triggered task of higher
           priority. */
        assert(_noLoopsTask01_C0 == _noLoopsTask00_C1);

        /* This tasks cycles with about 10 ms. This will succeed only if the other task in
           the same priority class does not use lengthy blocking operations. */
        rtos_suspendTaskTillTime(/* deltaTimeTillRelease */ 2 /* unit: 5ms */);
    }
} /* End of task01_class00 */





/**
 * Task of high priority.
 *   @param initCondition
 * Which events made the task run the very first time?
 *   @remark
 * A task function must never return; this would cause a reset.
 */
static void _Noreturn task00_class01(uint32_t initCondition ATTRIB_DBG_ONLY)
{
    assert(initCondition == RTOS_EVT_EVENT_00);

    /* This tasks cycles once when it is awaked by the event. */
    do
    {
        /* As long as we stay in the loop we didn't see a timeout. */
        ++ _noLoopsTask00_C1;
    }
    while(rtos_waitForEvent( /* eventMask */ RTOS_EVT_EVENT_00 | RTOS_EVT_DELAY_TIMER
                           , /* all */ false
                           , /* timeout */ 3 /* unit: 5ms */
                           )
          == RTOS_EVT_EVENT_00
         );

    /* We must never get here. Otherwise the test case failed. In compilation mode
       PRODUCTION, when there's no assertion, we would see an immediate reset because we
       leave a task function. */
    assert(false);

} /* End of task00_class01 */





/**
 * The initalization of the RTOS tasks and general board initialization.
 */

void setup(void)
{
    /* Print standard greeting of RTuinOS applications. */
    fputs(RTOS_EOL RTOS_RTUINOS_STARTUP_MSG RTOS_EOL, stdout);

    /* Task 0 of priority class 0 */
    rtos_initializeTask( /* idxTask */          0
                       , /* taskFunction */     task00_class00
                       , /* prioClass */        0
                       , /* pStackArea */       &_taskStack00_C0[0]
                       , /* stackSize */        sizeof(_taskStack00_C0)
                       , /* startEventMask */   RTOS_EVT_ABSOLUTE_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     0
                       );

    /* Task 1 of priority class 0 */
    rtos_initializeTask( /* idxTask */          1
                       , /* taskFunction */     task01_class00
                       , /* prioClass */        0
                       , /* pStackArea */       &_taskStack01_C0[0]
                       , /* stackSize */        sizeof(_taskStack01_C0)
                       , /* startEventMask */   RTOS_EVT_ABSOLUTE_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     1
                       );

    /* Task 0 of priority class 1 */
    rtos_initializeTask( /* idxTask */          2
                       , /* taskFunction */     task00_class01
                       , /* prioClass */        1
                       , /* pStackArea */       &_taskStack00_C1[0]
                       , /* stackSize */        sizeof(_taskStack00_C1)
                       , /* startEventMask */   RTOS_EVT_EVENT_00
                       , /* startByAllEvents */ false
                       , /* startTimeout */     2
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

    ++ _noLoopsIdleTask;

    /* An event can be posted even if nobody is listening for it. */
    rtos_sendEvent(/* eventVec */ RTOS_EVT_EVENT_04);

    /* This event will release task 0 of class 0. However we do not get here again fast
       enough to avoid all timeouts in that task. */
    rtos_sendEvent(/* eventVec */ RTOS_EVT_EVENT_03);

    iprintf("RTuinOS is idle\r\n");
    iprintf("noLoopsIdleTask: %u\r\n" , _noLoopsIdleTask);
    iprintf("noLoopsTask00_C0: %u\r\n", _noLoopsTask00_C0);
    iprintf("noLoopsTask01_C0: %u\r\n", _noLoopsTask01_C0);
    iprintf("noLoopsTask00_C1: %u\r\n", _noLoopsTask00_C1);

    iprintf("task00_C0_cntWaitTimeout: %u\r\n", _task00_C0_cntWaitTimeout);

    /* Look for the stack usage. */
    for(idxStack=0; idxStack<RTOS_NO_TASKS; ++idxStack)
    {
        iprintf("Stack reserve of task %u: %u, task overrun: %u\r\n"
               , idxStack
               , rtos_getStackReserve(idxStack)
               , /* The RTuinOS task overrun counter is not reliable for very slow tasks.
                    We've implemented our own counter inside the task function of the slow
                    task task00_C0. */
                 idxStack == 0? _task00_C0_trueTaskOverrunCnt
                              : rtos_getTaskOverrunCounter(idxStack, /* doReset */ false)
               );
    }

    /* Blink takes many hundreds of milli seconds. To prevent too many timeouts in
       task00_C0 we post the event also inside of blink. */
    blink(2);

} /* End of loop */




