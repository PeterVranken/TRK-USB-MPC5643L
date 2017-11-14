#ifndef RTOS_INCLUDED
#define RTOS_INCLUDED
/**
 * @file rtos.h
 * Definition of global interface of module rtos.c
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

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>


/*
 * Defines
 */

/* The software is written as portable as reasonably possible. This requires the awareness
   of the C language standard it is compiled with. */
#if defined(__STDC_VERSION__)
# if (__STDC_VERSION__)/100 == 2011
#  define _STDC_VERSION_C11
# elif (__STDC_VERSION__)/100 == 1999
#  define _STDC_VERSION_C99
# endif
#endif


/*
 * Global type definitions
 */

typedef struct rtos_taskDesc_t
{
    /** The task function pointer. This function is regularly executed under control of the
        RTOS kernel. */
    void (*taskFct)(void);
    
    /** The period time of the task activation in ms. The permitted range is 0..2^30-1. 0
        means no regular, timer controlled activation. This task is only enabled for
        software triggered activation using rtos_activateTask() (by interrupts or other
        tasks). */
    unsigned int tiCycleInMs;
    
    /** The priority of the task in the range 1..15. Different tasks can share the same
        priority or have different priorities. The execution of tasks, which share the
        priority will be sequenced when they become due at same time or with overlap. */
    unsigned int priority;
    
} rtos_taskDesc_t;



/*
 * Global data declarations
 */


/*
 * Inline functions
 */

/** 
 * Partial interrupt lock: All interrupts up to the specified priority level won't be
 * handled by the CPU. This function is intended for implementing mutual exclusion of
 * sub-sets of tasks; the use of 
 *   - ihw_enterCriticalSection() and
 *   - ihw_leaveCriticalSection()
 * or
 *   - ihw_suspendAllInterrupts() and
 *   - ihw_resumeAllInterrupts()
 * locks all interrupt processing and no other task (or interrupt handler) can become
 * active while the task is inside its critical section code. Using this function is much
 * better: Call it with the highest priority of all tasks, which should be locked, i.e. which
 * compete for the critical section to protect. This may still lock other, not competing
 * tasks, but at least all non competing tasks of higher priority will still be served (and
 * this will likely include most interrupt handlers).\n
 *   Note, there is no counterpart function. To leave the critical section, call this
 * function again to restore the interrupt/task priority level at entry into the critical
 * section.
 *   @param suspendUpToThisPriority
 * All interrupts up to and including this interrupt priority will be locked. The CPU will
 * not handle them until the priority level is lowered again.
 *   @remark
 *   The expense in terms of CPU consumption of using this function instead of the others
 * mentioned above negligible for all critical section code, which consists of more than a
 * few machine instructions.
 *   @remark
 * The use of this function to implement critical sections is usually quite static. For any
 * protected entity (usually a data object or I/O device) the set of competing tasks
 * normally is a compile time known. The priority level to set for entry into the critical
 * section is the maximum of the priorities of all tasks in the set. The priority level to
 * restore on exit from the critical section is the priority of the calling task. All of
 * this static knowledge would typically be put into encapsulating macros that actually
 * invoke this function. (OSEK/VDX like environments would use this function to implement
 * the GetResource/ReleaseResource concept.)
 *   @remark
 * It is a severe application error if the priority is not restored again by the same task
 * and before it ends. The RTOS behavior will become unpredictable if this happens. It is
 * not possible to considere this function a mutex, which can be acquired in one task
 * activation and which can be releases in an arbitrary later task activation or from
 * another task.
 *   @remark
 * An application must use this function solely to (temporarily) rise the current priority
 * level of handled tasks/ISRs. If the application code fails to manage the correct levels
 * (as maximum of the task set on entry) and lowers the priority accidentally then the RTOS
 * fails. The currently executed task/ISR will be recursively called again: The interrupt
 * notifying bit will be reset at the end of the service routine and is still pending.
 */
static inline unsigned int rtos_suspendAllInterruptsByPriority
                                            (unsigned int suspendUpToThisPriority)
{
    /* All priorities are in the range 0..15. Everything else points to an application
       error even if the hardware wouldn't mind. */
    assert((suspendUpToThisPriority & ~0xfu) == 0);

    /* MCU reference manual, section 28.6.6.2, p. 932: The change of the current priority
       in the INTC should be done under global interrupt lock. */
    asm volatile ( "wrteei 0\n" ::: );
    unsigned int priorityLevelSofar = INTC.CPR_PRC0.R;
    INTC.CPR_PRC0.R = suspendUpToThisPriority;
    asm volatile ( "wrteei 1\n" ::: );
                 
    return priorityLevelSofar;
    
} /* End of rtos_suspendAllInterruptsByPriority */



/*
 * Global prototypes
 */

/** Task registration. */
unsigned int rtos_registerTask( const rtos_taskDesc_t *pTaskDesc
                              , unsigned int tiFirstActivationInMs
                              );

/** Kernel initialization. */
void rtos_initKernel(void);

/** Software triggered task activation. Can be called from other tasks or interrupts. */
bool rtos_activateTask(unsigned int id);

#endif  /* RTOS_INCLUDED */
