#ifndef RTOS_INCLUDED
#define RTOS_INCLUDED
/**
 * @file rtos.h
 * Definition of global interface of module rtos.c
 *
 * Copyright (C) 2017-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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

#include "MPC5643L.h"


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
 * Priority ceiling protocol, partial interrupt lock: All interrupts up to the specified
 * priority level won't be handled by the CPU. This function is intended for implementing
 * mutual exclusion of sub-sets of tasks; the use of
 *   - ihw_enterCriticalSection() and
 *   - ihw_leaveCriticalSection()
 * or
 *   - ihw_suspendAllInterrupts() and
 *   - ihw_resumeAllInterrupts()
 * locks all interrupt processing and no other task (or interrupt handler) can become
 * active while the task is inside the critical section code. Using this function is much
 * better: Call it with the highest priority of all tasks, which should be locked, i.e. which
 * compete for the critical section to protect. This may still lock other, not competing
 * tasks, but at least all non competing tasks of higher priority will still be served (and
 * this will likely include most interrupt handlers).\n
 *   To leave the critical section, call the counterpart function
 * rtos_resumeAllInterruptsByPriority(), which restores the original interrupt/task
 * priority level.
 *   @return
 * The priority level at entry into this function (and into the critical section) is
 * returned. This level needs to be restored on exit from the critical section using
 * rtos_resumeAllInterruptsByPriority().
 *   @param suspendUpToThisPriority
 * All tasks/interrupts up to and including this priority will be locked. The CPU will
 * not handle them until the priority level is lowered again.
 *   @remark
 * To support the use case of nested calls of OSEK/VDX like GetResource/ReleaseResource
 * functions, this function compares the stated value to the current priority level. If \a
 * suspendUpToThisPriority is less than the current value then the current value is not
 * altered. The function still returns the current value and the calling code doesn't need
 * to take care: It can unconditionally end a critical section with
 * rtos_resumeAllInterruptsByPriority() stating the returned priority level value. (The
 * resume function will have no effect in this case.) This makes the OSEK like functions
 * usable without deep inside or full transparency of the priority levels behind the scene;
 * just use the pairs of Get-/ResumeResource, be they nested or not.
 *   @remark
 * The expense in terms of CPU consumption of using this function instead of the others
 * mentioned above is negligible for all critical section code, which consists of more than a
 * few machine instructions.
 *   @remark
 * The use of this function to implement critical sections is usually quite static. For any
 * protected entity (usually a data object or I/O device) the set of competing tasks
 * normally is a compile time known. The priority level to set for entry into the critical
 * section is the maximum of the priorities of all tasks in the set. The priority level to
 * restore on exit from the critical section is the priority of the calling task. All of
 * this static knowledge would typically be put into encapsulating macros that actually
 * invoke this function. (OSEK/VDX like environments would use this function pair to
 * implement the GetResource/ReleaseResource concept.)
 *   @remark
 * It is a severe application error if the priority is not restored again by the same task
 * and before it ends. The RTOS behavior will become unpredictable if this happens. It is
 * not possible to consider this function a mutex, which can be acquired in one task
 * activation and which can be releases in an arbitrary later task activation or from
 * another task.
 */
static inline uint32_t rtos_suspendAllInterruptsByPriority(uint32_t suspendUpToThisPriority)
{
    /* All priorities are in the range 0..15. Everything else points to an application
       error even if the hardware wouldn't mind. */
    assert((suspendUpToThisPriority & ~0xfu) == 0);

    /* MCU reference manual, section 28.6.6.2, p. 932: The change of the current priority
       in the INTC should be done under global interrupt lock. */
    asm volatile ( "wrteei 0\n" ::: );
    uint32_t priorityLevelSoFar = INTC.CPR_PRC0.R;
    
    /* It is useless and a waste of CPU but not a severe error to let this function set the
       same priority level we already have.
         It leads to immediate failure of the RTOS if we lower the level; however, from the
       perspective of the application code it is not necessarily an error to do so: If an
       application is organized in OSEK/VDX like resources it may be stringent (not
       optimal) to acquire different resources before an operation on them is started. These
       resources may be mapped onto different priority ceilings and the application may use
       nested calls of GetResource to acquire all of them. We must not force the
       application code to order the calls in order of increasing priority ceiling -- this
       will be intransparent to the application.
         These considerations leads to different strategies, and all of them are justified:
         To force the application be optimal in terms of CPU consumption, we would place an
       assertion: assert(suspendUpToThisPriority > priorityLevelSoFar);
         To be relaxed about setting the same priority ceiling, we would place another
       assertion: assert(suspendUpToThisPriority >= priorityLevelSoFar);
         If we want to build an OSEK/VDX like GetResource on top of this function, we
       should place a run-time condition: if(suspendUpToThisPriority > priorityLevelSoFar) */
    //assert(suspendUpToThisPriority > priorityLevelSoFar);
    //assert(suspendUpToThisPriority >= priorityLevelSoFar);
    if(suspendUpToThisPriority > priorityLevelSoFar)
        INTC.CPR_PRC0.R = suspendUpToThisPriority;

    /* We put a memory barrier before we reenable the interrupt handling. The write to the
       CPR register is surely done prior to next interrupt.
         Note, the next interrupt can still be a last one of priority less than or equal to
       suspendUpToThisPriority. This happens occasional when the interrupt asserts, while
       we are here inside the critical section. Incrementing CPR does not un-assert an
       already asserted interrupt. The isync instruction ensures that this last interrupt
       has completed prior to the execution of the first code inside the critical section.
       See https://community.nxp.com/message/993795 for more. */
    asm volatile ( "mbar\n\t"
                   "wrteei 1\n\t"
#ifdef __VLE__
                   "se_isync\n"
#else
                   "isync\n"
#endif
                   :::
                 );
                 
    return priorityLevelSoFar;
    
} /* End of rtos_suspendAllInterruptsByPriority */



/** 
 * This function is called to end a critical section of code, which requires mutual
 * exclusion of two or more tasks/ISRs. It is the counterpart of function
 * rtos_suspendAllInterruptsByPriority(), refer to this function for more details.\n
 *   Note, this function simply and unconditionally sets the current task/interrupt
 * priority level to the stated value. It can therefore be used to build further optimized
 * mutual exclusion code if it is applied to begin \a and to end a critical section. This
 * requires however much more control of the specified priority levels as when using the
 * counterpart function rtos_suspendAllInterruptsByPriority() on entry. Accidental
 * temporary lowering of the level will make the RTOS immediately fail.
 *   @param resumeDownToThisPriority
 * All tasks/interrupts above this priority level are resumed again. All tasks/interrupts
 * up to and including this priority remain locked.
 *   @remark
 * An application can (temporarily) rise the current priority level of handled tasks/ISRs
 * but must never lower them, the RTOS would fail: The hardware bit, that notified the
 * currently executing task/interrupt will be reset only at the end of the service routine,
 * so it is still pending. At the instance of lowering the priority level, the currently
 * executed task/ISR would be recursively called again.
 */ 
static inline void rtos_resumeAllInterruptsByPriority(uint32_t resumeDownToThisPriority)
{
    /* MCU reference manual, section 28.6.6.2, p. 932: The change of the current priority
       in the INTC should be done under global interrupt lock. A memory barrier ensures
       that all memory operations inside the now left critical section are completed. */
    asm volatile ( "mbar\n\t"
                   "wrteei 0\n"
                   :::
                 );
    INTC.CPR_PRC0.R = resumeDownToThisPriority;
    asm volatile ( "wrteei 1\n" ::: );

} /* End of rtos_resumeAllInterruptsByPriority */



/*
 * Global prototypes
 */

/** Task registration. */
unsigned int rtos_registerTask( const rtos_taskDesc_t *pTaskDesc
                              , unsigned int tiFirstActivationInMs
                              );

/** Kernel initialization. */
void rtos_initKernel(void);

/** The operating system's scheduler function. */
void rtos_onOsTimerTick(void);

/** Software triggered task activation. Can be called from other tasks or interrupts. */
bool rtos_activateTask(unsigned int idTask);

/** Get the current number of failed task activations since start of the RTOS scheduler. */
unsigned int rtos_getNoActivationLoss(unsigned int idTask);

/** Compute how many bytes of the stack area are still unused. */
unsigned int rtos_getStackReserve(void);

#endif  /* RTOS_INCLUDED */
