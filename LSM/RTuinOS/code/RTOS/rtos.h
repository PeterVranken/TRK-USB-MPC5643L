#ifndef RTOS_INCLUDED
#define RTOS_INCLUDED
/**
 * @file rtos.h
 * Definition of global interface of module rtos.c
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
 */

/*
 * Include files
 */

#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "sc_systemCalls.h"
#include "int_defStackFrame.h"
#include "int_interruptHandler.h"
#include "rtos.config.h"


/*
 * Defines
 */

/** Version string of RTuinOS. */
#define RTOS_RTUINOS_VERSION    "1.0"

/** Startup message for RTuinOS applications. */
#define RTOS_RTUINOS_STARTUP_MSG                                                            \
    "RTuinOS " RTOS_RTUINOS_VERSION " for NXP MPC5643L" RTOS_EOL                            \
    "Copyright (C) 2012-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)" RTOS_EOL        \
    "This is free software; see the source for copying conditions. There is NO" RTOS_EOL    \
    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."

/** Switch to make feature selecting defines readable. Here: Feature is enabled. */
#define RTOS_FEATURE_ON     1
/** Switch to make feature selecting defines readable. Here: Feature is disabled. */
#define RTOS_FEATURE_OFF    0

/** The EOL character sequence used in the startup message. */
#ifndef RTOS_EOL
# define RTOS_EOL "\n"
#endif

/** A macro to help estimating the appropriate stack size. The stack size in byte is
    derived from the macro arguments \a stackRequirementTaskInByte and \a
    noUsedIrqLevels.\n
      Furthermore, the computed value is rounded in order to consider the alignment
    constraints of a PowerPC stack.
      @param stackRequirementTaskInByte
    The number of bytes requires by the task code itself. This value needs to be estimated
    by the function designer.
      @param noUsedIrqLevels
    The number of interrupt levels in use. This needs to include all interrupts, from all
    I/O drivers and from the RTOS kernel. The macro considers the worst case stack space
    requirement for the stack frames for these interrupts and adds it to the task's own
    requirement. */
#define RTOS_REQUIRED_STACK_SIZE_IN_BYTE(stackRequirementTaskInByte, noUsedIrqLevels) \
            ((((noUsedIrqLevels)*(S_I_StFr)+(S_SC_StFr)+(stackRequirementTaskInByte))+7)&~7)


/** Derive a switch telling whether events of type semaphore are in use. */
#if RTOS_NO_SEMAPHORE_EVENTS > 0
# define RTOS_USE_SEMAPHORE RTOS_FEATURE_ON
#else
# define RTOS_USE_SEMAPHORE RTOS_FEATURE_OFF
#endif


/** Derive a switch telling whether events of type mutex are in use. */
#if RTOS_NO_MUTEX_EVENTS > 0
# define RTOS_USE_MUTEX RTOS_FEATURE_ON
#else
# define RTOS_USE_MUTEX RTOS_FEATURE_OFF
#endif


/* Some global, general purpose events and the two timer events. Used to specify the
   resume condition when suspending a task.
     Conditional definition: If the application defines an interrupt which triggers an
   event, the same event gets a deviating name. */
/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 0
# define RTOS_EVT_SEMAPHORE_00      ((uint32_t)1<<0)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 0
# define RTOS_EVT_MUTEX_00          ((uint32_t)1<<0)
#else
# define RTOS_EVT_EVENT_00          ((uint32_t)1<<0)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 1
# define RTOS_EVT_SEMAPHORE_01      ((uint32_t)1<<1)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 1
# define RTOS_EVT_MUTEX_01          ((uint32_t)1<<1)
#else
# define RTOS_EVT_EVENT_01          ((uint32_t)1<<1)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 2
# define RTOS_EVT_SEMAPHORE_02      ((uint32_t)1<<2)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 2
# define RTOS_EVT_MUTEX_02          ((uint32_t)1<<2)
#else
# define RTOS_EVT_EVENT_02          ((uint32_t)1<<2)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 3
# define RTOS_EVT_SEMAPHORE_03      ((uint32_t)1<<3)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 3
# define RTOS_EVT_MUTEX_03          ((uint32_t)1<<3)
#else
# define RTOS_EVT_EVENT_03          ((uint32_t)1<<3)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 4
# define RTOS_EVT_SEMAPHORE_04      ((uint32_t)1<<4)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 4
# define RTOS_EVT_MUTEX_04          ((uint32_t)1<<4)
#else
# define RTOS_EVT_EVENT_04          ((uint32_t)1<<4)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 5
# define RTOS_EVT_SEMAPHORE_05      ((uint32_t)1<<5)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 5
# define RTOS_EVT_MUTEX_05          ((uint32_t)1<<5)
#else
# define RTOS_EVT_EVENT_05          ((uint32_t)1<<5)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 6
# define RTOS_EVT_SEMAPHORE_06      ((uint32_t)1<<6)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 6
# define RTOS_EVT_MUTEX_06          ((uint32_t)1<<6)
#else
# define RTOS_EVT_EVENT_06          ((uint32_t)1<<6)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 7
# define RTOS_EVT_SEMAPHORE_07      ((uint32_t)1<<7)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 7
# define RTOS_EVT_MUTEX_07          ((uint32_t)1<<7)
#else
# define RTOS_EVT_EVENT_07          ((uint32_t)1<<7)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 7
# define RTOS_EVT_SEMAPHORE_07      ((uint32_t)1<<7)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 7
# define RTOS_EVT_MUTEX_07          ((uint32_t)1<<7)
#else
# define RTOS_EVT_EVENT_07          ((uint32_t)1<<7)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 8
# define RTOS_EVT_SEMAPHORE_08      ((uint32_t)1<<8)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 8
# define RTOS_EVT_MUTEX_08          ((uint32_t)1<<8)
#else
# define RTOS_EVT_EVENT_08          ((uint32_t)1<<8)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 9
# define RTOS_EVT_SEMAPHORE_09      ((uint32_t)1<<9)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 9
# define RTOS_EVT_MUTEX_09          ((uint32_t)1<<9)
#else
# define RTOS_EVT_EVENT_09          ((uint32_t)1<<9)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 10
# define RTOS_EVT_SEMAPHORE_10      ((uint32_t)1<<10)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 10
# define RTOS_EVT_MUTEX_10          ((uint32_t)1<<10)
#else
# define RTOS_EVT_EVENT_10          ((uint32_t)1<<10)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 11
# define RTOS_EVT_SEMAPHORE_11      ((uint32_t)1<<11)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 11
# define RTOS_EVT_MUTEX_11          ((uint32_t)1<<11)
#else
# define RTOS_EVT_EVENT_11          ((uint32_t)1<<11)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 12
# define RTOS_EVT_SEMAPHORE_12      ((uint32_t)1<<12)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 12
# define RTOS_EVT_MUTEX_12          ((uint32_t)1<<12)
#else
# define RTOS_EVT_EVENT_12          ((uint32_t)1<<12)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 13
# define RTOS_EVT_SEMAPHORE_13      ((uint32_t)1<<13)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 13
# define RTOS_EVT_MUTEX_13          ((uint32_t)1<<13)
#else
# define RTOS_EVT_EVENT_13          ((uint32_t)1<<13)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 14
# define RTOS_EVT_SEMAPHORE_14      ((uint32_t)1<<14)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 14
# define RTOS_EVT_MUTEX_14          ((uint32_t)1<<14)
#else
# define RTOS_EVT_EVENT_14          ((uint32_t)1<<14)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 15
# define RTOS_EVT_SEMAPHORE_15      ((uint32_t)1<<15)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 15
# define RTOS_EVT_MUTEX_15          ((uint32_t)1<<15)
#else
# define RTOS_EVT_EVENT_15          ((uint32_t)1<<15)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 16
# define RTOS_EVT_SEMAPHORE_16      ((uint32_t)1<<16)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 16
# define RTOS_EVT_MUTEX_16          ((uint32_t)1<<16)
#else
# define RTOS_EVT_EVENT_16          ((uint32_t)1<<16)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 17
# define RTOS_EVT_SEMAPHORE_17      ((uint32_t)1<<17)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 17
# define RTOS_EVT_MUTEX_17          ((uint32_t)1<<17)
#else
# define RTOS_EVT_EVENT_17          ((uint32_t)1<<17)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 18
# define RTOS_EVT_SEMAPHORE_18      ((uint32_t)1<<18)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 18
# define RTOS_EVT_MUTEX_18          ((uint32_t)1<<18)
#else
# define RTOS_EVT_EVENT_18          ((uint32_t)1<<18)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 19
# define RTOS_EVT_SEMAPHORE_19      ((uint32_t)1<<19)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 19
# define RTOS_EVT_MUTEX_19          ((uint32_t)1<<19)
#else
# define RTOS_EVT_EVENT_19          ((uint32_t)1<<19)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 20
# define RTOS_EVT_SEMAPHORE_20      ((uint32_t)1<<20)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 20
# define RTOS_EVT_MUTEX_20          ((uint32_t)1<<20)
#else
# define RTOS_EVT_EVENT_20          ((uint32_t)1<<20)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 21
# define RTOS_EVT_SEMAPHORE_21      ((uint32_t)1<<21)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 21
# define RTOS_EVT_MUTEX_21          ((uint32_t)1<<21)
#else
# define RTOS_EVT_EVENT_21          ((uint32_t)1<<21)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 22
# define RTOS_EVT_SEMAPHORE_22      ((uint32_t)1<<22)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 22
# define RTOS_EVT_MUTEX_22          ((uint32_t)1<<22)
#else
# define RTOS_EVT_EVENT_22          ((uint32_t)1<<22)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 23
# define RTOS_EVT_SEMAPHORE_23      ((uint32_t)1<<23)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 23
# define RTOS_EVT_MUTEX_23          ((uint32_t)1<<23)
#else
# define RTOS_EVT_EVENT_23          ((uint32_t)1<<23)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 24
# define RTOS_EVT_SEMAPHORE_24      ((uint32_t)1<<24)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 24
# define RTOS_EVT_MUTEX_24          ((uint32_t)1<<24)
#else
# define RTOS_EVT_EVENT_24          ((uint32_t)1<<24)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 25
# define RTOS_EVT_SEMAPHORE_25      ((uint32_t)1<<25)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 25
# define RTOS_EVT_MUTEX_25          ((uint32_t)1<<25)
#else
# define RTOS_EVT_EVENT_25          ((uint32_t)1<<25)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 26
# define RTOS_EVT_SEMAPHORE_26      ((uint32_t)1<<26)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 26
# define RTOS_EVT_MUTEX_26          ((uint32_t)1<<26)
#else
# define RTOS_EVT_EVENT_26          ((uint32_t)1<<26)
#endif

/** General purpose event, posted explicitly by rtos_sendEvent. */
#if RTOS_NO_SEMAPHORE_EVENTS > 27
# define RTOS_EVT_SEMAPHORE_27      ((uint32_t)1<<27)
#elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 27
# define RTOS_EVT_MUTEX_27          ((uint32_t)1<<27)
#else
# define RTOS_EVT_EVENT_27          ((uint32_t)1<<27)
#endif

/* The name of the next event depends on the configuration of RTuinOS. */
#if RTOS_USE_APPL_INTERRUPT_01 == RTOS_FEATURE_ON
# define RTOS_EVT_ISR_USER_01       ((uint32_t)1<<28)
# if RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 28
#  error Too many semaphores and mutexes specified. The limit is 28 when using two application interrupts
# endif
#else
/** General purpose event, posted explicitly by rtos_sendEvent. */
# if RTOS_NO_SEMAPHORE_EVENTS > 28
#  define RTOS_EVT_SEMAPHORE_28      ((uint32_t)1<<28)
# elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 28
#  define RTOS_EVT_MUTEX_28          ((uint32_t)1<<28)
# else
#  define RTOS_EVT_EVENT_28          ((uint32_t)1<<28)
# endif
#endif

/* The name of the next event depends on the configuration of RTuinOS. */
#if RTOS_USE_APPL_INTERRUPT_00 == RTOS_FEATURE_ON
# define RTOS_EVT_ISR_USER_00       (0x0001<<29)
# if RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 29
#  error Too many semaphores and mutexes specified. The limit is 29 when using a single application interrupt
# endif
#else
/** General purpose event, posted explicitly by rtos_sendEvent. */
# if RTOS_NO_SEMAPHORE_EVENTS > 29
#  define RTOS_EVT_SEMAPHORE_29      ((uint32_t)1<<29)
# elif RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 29
#  define RTOS_EVT_MUTEX_29          ((uint32_t)1<<29)
# else
#  define RTOS_EVT_EVENT_29          ((uint32_t)1<<29)
# endif
#endif

#if RTOS_NO_SEMAPHORE_EVENTS + RTOS_NO_MUTEX_EVENTS > 30
# error Too many semaphores and mutexes specified. The limit is 30 in total
#endif

/** Real time clock is elapsed for the task. */
#define RTOS_EVT_ABSOLUTE_TIMER     ((uint32_t)1<<30)
/** The relative-to-start clock is elapsed for the task */
#define RTOS_EVT_DELAY_TIMER        ((uint32_t)1<<31)


/** The system timer frequency as floating point constant. The unit is Hz.\n
      The value is derived from #RTOS_TICK, which is 1 ms in the standard configuration.
    The macro is defined in the configuration file rtos.config.h as it might be subject to
    changes by the application. */
#define RTOS_TICK_FREQUENCY (1.0f/(RTOS_TICK))

/** The RTuinOS system timer tick as float constant in unit ms.\n
      The value is derived from #RTOS_TICK, which is 1 ms in the standard configuration.
    The macro is defined in the configuration file rtos.config.h as it might be subject to
    changes by the application. */
#define RTOS_TICK_MS ((RTOS_TICK)*1000.0f)


/** Function prototype decoration which declares a function of RTuinOS just a default
    implementation of the required functionality. The application code can redefine the
    function and override the default implementation.\n
      We use this type decoration for the initialization of the system timer interrupt --
    an RTuinOS application may use any other interrupts source than the default timer
    PID 0. */
#define RTOS_DEFAULT_FCT __attribute__((weak))


/**
 * Suspend the current task (i.e. the one which invokes this method) until a specified
 * combination of events occur. In the special case, that the specified combination of
 * events can be satisfied with currently available synchronization objects (mutexes and
 * semaphores) the function will return immediately without suspending the calling task.\n
 *   A task is suspended in the instance of calling this method. It specifies a list of
 * events. The task becomes due again, when either the first one or all of the specified
 * events have been posted by other tasks.\n
 *   The idle task can't be suspended. If it calls this function a crash would be the
 * immediate result.
 *   @return
 * The set of actually resuming events is returned as a bit vector which corresponds
 * bitwise to eventMask. See \a rtos.h for a list of known events.
 *   @param eventMask
 * The bit vector of events to wait for. Needs to include either the delay timer event
 * #RTOS_EVT_DELAY_TIMER or #RTOS_EVT_ABSOLUTE_TIMER, if a timeout is required, but not both
 * of them.\n
 *   The normal use case will probably be the delay timer. However, a regular task may also
 * specify the absolute timer with the next regular task activation time as parameter so
 * that it surely becomes due outermost at its usual task time.
 *   @param all
 *   If \a false, the task is made due as soon as the first event mentioned in \a eventMask
 * is seen.\n
 *   If \a true, the task is made due only when all events are posted to the suspending
 * task - except for the timer events, which are still OR combined. If you say "all" but
 * the event mask contains either #RTOS_EVT_DELAY_TIMER or #RTOS_EVT_ABSOLUTE_TIMER, the
 * task will resume when either the timer elapsed or when all other events in the mask were
 * seen.\n
 *   Caution: If all is true, there need to be at least one event bit set in \a eventMask
 * besides a timer bit; RTuinOS crashes otherwise. Saying: "No particular event, just a
 * timer condition" needs to be implemented by \a all set to false and \a eventMask set to
 * only a timer event bit.
 *   @param timeout
 * If \a eventMask contains #RTOS_EVT_DELAY_TIMER:\n
 *   The number of system timer ticks from now on until the timeout elapses. One should be
 * aware of the resolution of any timing being the tick of the system timer, see
 * #RTOS_TICK. A timeout of \a n may actually mean any delay in the range \a n .. \a n+1
 * ticks.\n
 *   Even specifying 0 will suspend the task a short time and give others the chance to
 * become active - particularly other tasks belonging to the same priority class.\n
 *   If \a eventMask contains #RTOS_EVT_ABSOLUTE_TIMER:\n
 *   The absolute time the task becomes due again at latest. The time designation is
 * relative; it refers to the last recent absolute time at which this task had been
 * resumed. See #rtos_suspendTaskTillTime for details.\n
 *   Now the range of the parameter is 1 till INT_MAX (and not UINT_MAX).\n
 *   If neither #RTOS_EVT_DELAY_TIMER nor #RTOS_EVT_ABSOLUTE_TIMER is set in the event mask,
 * this parameter should be zero.
 *   @remark
 * This macro invokes the system call trap with system call index
 * #SC_IDX_SYS_CALL_WAIT_FOR_EVENT.
 */
#define /* uint32_t */ rtos_waitForEvent( /* uint32_t */     eventMask                      \
                                        , /* bool */         all                            \
                                        , /* unsigned int */ timeout                        \
                                        )                                                   \
                int_systemCall(SC_IDX_SYS_CALL_WAIT_FOR_EVENT, (eventMask), (all), (timeout))


/**
 * Delay a task without looking at other events. \a rtos_delay(delayTime) is identical to
 * \a rtos_waitForEvent(#RTOS_EVT_DELAY_TIMER, false, delayTime), i.e. \a eventMask's only
 * set bit is the delay timer event.
 *   @param delayTime
 * The duration of the delay in the unit of the system time, see #RTOS_TICK. The permitted
 * range is 0..max(unsigned int). The resolution of any timing operation is the tick of the
 * system timer. A delay time of \a n may actually mean any delay in the range \a n .. \a
 * n+1 ticks.
 *   @remark
 * This method is one of the task suspend commands. It must not be used by the idle task,
 * which can't be suspended. A crash would be the immediate consequence.
 *   @remark
 * This function actually is a macro calling #rtos_waitForEvent using fixed parameters.
 */
#define rtos_delay(delayTime)                                               \
                rtos_waitForEvent(RTOS_EVT_DELAY_TIMER, false, delayTime)




/**
 * Suspend the current task (i.e. the one which invokes this method) until a specified
 * point in time.\n
 *   Although specified as a increment in time, the time is meant absolute. The meant time
 * is the time specified at the last recent call of this function by this task plus the now
 * specified increment. This way of specifying the desired time of resume supports the
 * intended use case, which is the implementation of regular real time tasks: A task will
 * suspend itself with a constant time value at the end of the infinite loop which contains
 * its functional code. This (fixed) time value becomes the sample time of the task. This
 * behavior is opposed to a delay or sleep function: The execution time of the task is no
 * time which additionally elapses between two task resumes.\n
 *   The idle task can't be suspended. If it calls this function a crash would be the
 * immediate result.
 *   @return
 * The event mask of resuming events is returned. Since no combination with other events
 * than the elapsed system time is possible, this will always be #RTOS_EVT_ABSOLUTE_TIMER.
 *   @param deltaTimeTillResume
 * \a deltaTimeTillResume specifies a time in the future at which the task will become due
 * again. To support the most relevant use case of this function, the implementation of
 * regular real time tasks, the time designation is relative. It refers to the last recent
 * absolute time at which this task had been resumed. This time is defined by the last
 * recent call of either this function or \a rtos_waitForEvent with parameter
 * #RTOS_EVT_ABSOLUTE_TIMER. In the very first call of the function it refers to the point
 * in time the task was started.\n
 *   The value of \a deltaTimeTillResume must neither be 0 nor exceed half the range of
 * the data type configured for the system time. Otherwise a false task overrun recognition
 * and bad task timing could result. Please, refer to the RTuinOS manual for details.
 *   @remark
 * This function actually is a macro calling \a rtos_waitForEvent using fixed parameters.
 *   @see rtos_waitForEvent
 */
#define /* uint32_t */ rtos_suspendTaskTillTime(/* unsigned int */ deltaTimeTillResume)     \
    rtos_waitForEvent( /* eventMask */ RTOS_EVT_ABSOLUTE_TIMER                              \
                     , /* all */       false                                                \
                     , /* timeout */   deltaTimeTillResume                                  \
                     )


/**
 * A task (including the idle task) may post an event. The event is broadcasted to all
 * suspended tasks which are waiting for it. An event is not saved beyond that. If a task
 * suspends and starts waiting for an event which has been posted by another task just
 * before, it'll wait forever and never be resumed.\n
 *   The posted event may resume another task, which may be of higher priority as the event
 * posting task. In this case #rtos_sendEvent() will cause a task switch. The calling task
 * stays due but stops to be the active task. It does not become suspended (this is why
 * even the idle task may call this function). The activated task will resume by coming out
 * of the suspend command it had been invoked to wait for this event. The return value of
 * this suspend command will then tell about the event sent here.\n
 *   If no task of higher priority is resumed by the posted event the calling task will be
 * continued immediately after execution of this method. In this case #rtos_sendEvent()
 * behaves like any ordinary sub-routine.
 *   @param eventVec
 * A bit vector of posted events. Known events are defined in rtos.h. The timer events
 * #RTOS_EVT_ABSOLUTE_TIMER and #RTOS_EVT_DELAY_TIMER cannot be posted.
 *   @see
 * #rtos_waitForEvent()
 *   @remark
 * This macro invokes the system call trap with system call index
 * #SC_IDX_SYS_CALL_SEND_EVENT. 
 */
#define /* void */ rtos_sendEvent(/* uint32_t */ eventVec)                                  \
                                int_systemCall(SC_IDX_SYS_CALL_SEND_EVENT, (eventVec))


/*
 * Global type definitions
 */

/** The type of any task.\n
      The function is of type void; it must never return.\n
      The function takes a single parameter. It is the event vector of the very event
    combination which makes the task initially run. Typically this is just the delay timer
    event. */
typedef void (*rtos_taskFunction_t)(uint32_t postedEventVec);


/*
 * Global data declarations
 */

/** The RTuinOS startup message. See #RTOS_RTUINOS_STARTUP_MSG for the definition of string
    contents. */
extern const char rtos_rtuinosStartupMsg[];


#if RTOS_USE_SEMAPHORE == RTOS_FEATURE_ON
/** All declared semaphores are held in an array of counters.\n
      The type of the counter depends on the maximum number of pooled resources managed by
    a semaphore and is configurable, please see rtos.config.h for the typedef of
    typeSemaphore_t.\n
      The array is declared extern and it is defined by the application code. This way,
    it's most easy to initialize the semaphore counters. Any value is possible as a start
    value, this depends only on the application. */
extern unsigned int rtos_semaphoreAry[RTOS_NO_SEMAPHORE_EVENTS];
#endif


/*
 * Global inline functions
 */

/**
 * This routine in cooperation with rtos_leaveCriticalSection makes the code sequence
 * located between the two functions atomic with respect to operations of the RTuinOS task
 * scheduler.\n
 *   Any access to data shared between different tasks should be placed inside this pair of
 * functions in order to avoid data inconsistencies due to task switches during the access
 * time. A exception are trivial access operations which are atomic as such, e.g. read or
 * write of a single machine word (up to 32 Bit for MPC5643L).\n
 *   The functions implement the critical sections by locking all kernel relevant
 * interrupts. The use of the function pair ihw_suspendAllInterrupts() and
 * ihw_resumeAllInterrupts() is an alternative to rtos_enter/leaveCriticalSection. Globally
 * locking the interrupts is somewhat less expensive than inhibiting a specific set but
 * degrades the responsiveness of the system. ihw_suspendAllInterrupts() and
 * ihw_resumeAllInterrupts() should preferably be used if the data accessing code is rather
 * short so that the global lock time of all interrupts stays very brief.
 *   @remark
 * The implementation does not permit recursive invocation of the function pair. The status
 * of the lock is not saved. If two pairs of the functions are nested, the task switches
 * are re-enabled as soon as the inner pair is left - the remaining code in the outer pair
 * of functions would no longer be protected against unforeseen task switches. The ususal
 * way out is offering a pair of functions in the application code, which count their
 * invokations and which invoke these RTOS functions only at the initial count.
 *   @remark
 * These function can only be used from tasks. They must not be used from a kernel relevant
 * ISR. This would make the kernel fail and it is useless because all kernel relevant ISRs
 * and all system calls are anyway serialized. They are no competing execution contexts.
 */
static ALWAYS_INLINE void rtos_enterCriticalSection()
{
    /* MCU reference manual, section 28.6.6.2, p. 932: The change of the current priority
       in the INTC should be done under global interrupt lock. */
    asm volatile ( "wrteei 0\n" ::: );
    
    /* The assertion can help detecting unpermitted, nested use of the function pair or
       unpermitted use from a system call or an External Interrupt handler. */
    assert( /* priorityLevelSoFar */ INTC.CPR_PRC0.R == 0);
    
    /* All RTuinOS kernel interrupts have priority one. Raise CPR to this level to inhibit
       servicing of future kernel interrupts. */
    INTC.CPR_PRC0.R = 1;

    /* We put a memory barrier before we reenable the interrupt handling. The write to the
       CPR register is surely done prior to the servicing the next interrupt.
         Note, the next interrupt can still be a last one of priority less than or equal to
       INT_MAX_KERNEL_IRQ_PRIO. This happens occasional when the interrupt asserts, while
       we are here inside the critical section. Incrementing CPR does not un-assert an
       already asserted interrupt. The isync instruction ensures that this last interrupt
       has completed prior to the execution of the first code inside the critical section.
       See https://community.nxp.com/message/993795 for more. */
    asm volatile ( "mbar\n\t"
                   "wrteei 1\n\t"
                   "isync\n"
                   :::
                 );
    
    /* A single interrupt at prio 1, which occured in the short time span between wrteei 0
       and the write to CPR, can preempt us here. The critical section is however not
       endangered - when code execution returns here (which may happen arbitrarily later
       due to context switches caused by that interrupt) then the CPR value will be
       restored to 1. After return from this function the critical section begins. */
    assert( /* priorityLevelSoFar */ INTC.CPR_PRC0.R == 1);

} /* End of rtos_enterCriticalSection */



/**
 * This macro is the counterpart of #rtos_enterCriticalSection. Please refer to
 * #rtos_enterCriticalSection for deatils.
 */
static ALWAYS_INLINE void rtos_leaveCriticalSection()
{
    /* MCU reference manual, section 28.6.6.2, p. 932: The change of the current priority
       in the INTC should be done under global interrupt lock. A memory barrier ensures
       that all memory operations inside the now left critical section are completed. */
    asm volatile ( "mbar\n\t"
                   "wrteei 0\n"
                   :::
                 );
    assert( /* priorityLevelSoFar */ INTC.CPR_PRC0.R == 1);
    INTC.CPR_PRC0.R = 0;
    asm volatile ( "wrteei 1\n" ::: );

} /* End of rtos_leaveCriticalSection */



/*
 * Global prototypes
 */

/** We adopt the Arduino programming model for the PowerPC migration: There is a user
    supplied function called setup for initialization purpose and a infinitely, cylically
    called function loop(). This is the declaration of setup().\n
      setup() needs to be implemented to initialize the RTOS. Call rtos_initializeTask()
    repeatedly from here and only from here. */
extern void setup(void);
    
/** The e200z4 port offers a more open and flexible way to deal with kernel interrupts.
    These interrupts must not be initialized prior to the initialization of the kernel;
    they can cause task switches and the kernel must already be in the state to accept
    this. In the original Arduino programming model, the only possible code location for
    doing so would be the function loop(), which is awkward, since it is repeatedly
    invoked. We offer another hook: It is called after the kernel initialization but still
    prior to the start of the system timer interrupts.
      @remark The use of this function is optional. */
extern void setupAfterKernelInit(void);

/** To complete the set of initialization hooks, the e200z4 port of RTuinOS offers a
    callback into the application code after having started the system timer and
    application defined interrupts. The system is now fully up and running. This callback
    is called from the context in which rtos_initRTOS() is invoked (normally the context of
    C's main(), which now becomes the idle task context). This function call can also be
    considered the first action of the idle task. After return from this function, RTuinOS
    enters the infinite loop calling idle hook loop().
      @remark An application may decide not to return from this function. The
    implementation of this function becomes the idle task and the Arduino loop() function
    will never be called.
      @remark The use of this function is optional. */
extern void setupAfterSystemTimerInit(void);

/** We adopt the Arduino programming model for the PowerPC migration: There is a user
    supplied function called setup() for initialization purpose and a infinitely, cylically
    called function loop(). This is the declaration of loop.\n
      loop is the implementation of the RTOS' idle task.
      @remark The use of this function is optional. */
extern void loop(void);

/* Initialize all application parameters of one task. To be called for each of the tasks
    prior to the start of the kernel. */
void rtos_initializeTask( unsigned int idxTask
                        , rtos_taskFunction_t taskFunction
                        , unsigned int prioClass
#if RTOS_ROUND_ROBIN_MODE_SUPPORTED == RTOS_FEATURE_ON
                        , unsigned int timeRoundRobin
#endif
                        , uint8_t * const pStackArea
                        , unsigned int stackSize
                        , uint32_t startEventMask
                        , bool startByAllEvents
                        , unsigned int startTimeout
                        );

/** Configure and enable the interrupt which clocks the system time of RTuinOS. This
    function has a default implementation, the application may but need not to implement
    it.\n
      If the application decides to set up its specific system timer interrupt, it'll
    probably have to state the new system clock frequency, see #RTOS_TICK.\n
      Note, this prototype is published only to enable overloading of the weak default
    implementation. You must not call this function, RTuinOS' initialization code will do. */
void rtos_enableIRQTimerTick(void);

#if RTOS_USE_APPL_INTERRUPT_00 == RTOS_FEATURE_ON
/** An application supplied callback, which contains the code to set up the hardware to
    generate application interrupt 0.\n
      Note, this prototype is published only to enable implementing the function. You must
    not call this function, RTuinOS' initialization code will do. */
extern void rtos_enableIRQUser00(void);
#endif

#if RTOS_USE_APPL_INTERRUPT_01 == RTOS_FEATURE_ON
/** An application supplied callback, which contains the code to set up the hardware to
    generate application interrupt 1.\n
      Note, this prototype is published only to enable implementing the function. You must
    not call this function, RTuinOS' initialization code will do. */
extern void rtos_enableIRQUser01(void);
#endif

/** Initialization of the internal data structures of RTuinOS and start of the timer
    interrupt (see void rtos_enableIRQTimerTick(void)). This function does not return but
    forks into the configured tasks.\n
      This function is ususally called once from main(). */
_Noreturn void rtos_initRTOS(void);

/** How often could a real time task not be reactivated timely? */
unsigned int rtos_getTaskOverrunCounter(unsigned int idxTask, bool doReset);

/** How many bytes of the stack of a task are still unused? */
unsigned int rtos_getStackReserve(unsigned int idxTask);

#endif  /* RTOS_INCLUDED */
