#ifndef IHW_INITMCUCOREHW_INCLUDED
#define IHW_INITMCUCOREHW_INCLUDED
/**
 * @file ihw_initMcuCoreHW.h
 * Definition of global interface of module ihw_initMcuCoreHW.c
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
/* Module interface
 *   ihw_initMcuCoreHW
 *   ihw_suspendAllInterrupts
 *   ihw_resumeAllInterrupts
 *   ihw_enterCriticalSection
 *   ihw_leaveCriticalSection
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>

#include "typ_types.h"


/*
 * Defines
 */


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global inline functions
 */

/**
 * Disable all External Interrupts. This is done unconditionally, there's no nesting
 * counter.
 *   @remark Note, suspending all External Interrupts does not affect all other interrupts
 * (effectively CPU traps), like Machine Check interrupt.
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user 
 * code will lead to a privileged exception.
 */
static ALWAYS_INLINE void ihw_suspendAllInterrupts()
{
    /* The completion synchronizing character of the wrteei instruction forms the memory
       barrier, which ensures that all memory operations before the now entered critical
       section are completed before we enter (see core RM, 4.6.1, p. 151). The "memory"
       constraint ensures that the compiler won't reorder instructions from behind the
       wrteei to before it. */
    asm volatile ( /* AssemblerTemplate */
                   "wrteei 0\n"
                 : /* OutputOperands */
                 : /* InputOperands */
                 : /* Clobbers */ "memory"
                 );
} /* End of ihw_suspendAllInterrupts */



/**
 * Enable all External Interrupts. This is done unconditionally, there's no nesting
 * counter.
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user 
 * code will lead to a privileged exception.
 */
static ALWAYS_INLINE void ihw_resumeAllInterrupts()
{
    /* The completion synchronizing character of the wrteei instruction forms the memory
       barrier, which ensures that all memory operations inside the now left critical
       section are completed before we leave (see core RM, 4.6.1, p. 151). The "memory"
       constraint ensures that the compiler won't reorder instructions from before the
       wrteei to behind it. */
    asm volatile ( /* AssemblerTemplate */
                   "wrteei 1\n"
                 : /* OutputOperands */
                 : /* InputOperands */
                 : /* Clobbers */ "memory"
                 );
} /* End of ihw_resumeAllInterrupts */



/**
 * Start the code of a critical section, thus code, which operates on data, that must not
 * be touched from another execution context at the same time.\n
 *   The critical section is implemented by globally disabling all interrupts.
 *   @return
 * The machine status register content of before disabling the interrupts is returned. The
 * caller will safe it and pass it back to ihw_leaveCriticalSection() at the end of the
 * critical section. This way the nestability is implemented.
 *   @remark
 * The main difference of this function in comparison to ihw_suspendAllInterrupts() is the
 * possibility to nest the calls at different hierarchical code sub-function levels.
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user 
 * code will lead to a privileged exception.
 */
static ALWAYS_INLINE uint32_t ihw_enterCriticalSection()
{
    /* The completion synchronizing character of the mfmsr instruction forms the memory
       barrier, which ensures that all memory operations before the now entered critical
       section are completed before we enter (see core RM, 4.6.1, p. 151). The "memory"
       constraint ensures that the compiler won't reorder instructions from behind the
       wrteei to before it. */
    uint32_t msr;
    asm volatile ( /* AssemblerTemplate */
                   "mfmsr %0\n\t"
                   "wrteei 0\n\t"
                 : /* OutputOperands */ "=r" (msr)
                 : /* InputOperands */
                 : /* Clobbers */ "memory"
                 );
    return msr;

} /* End of ihw_enterCriticalSection */



/**
 * End the code of a critical section, thus code, which operates on data, that must not
 * be touched from another execution context at the same time.\n
 *   The critical section is implemented by globally disabling all interrupts.
 *   @param msr
 * The machine status register content as it used to be at entry into the critical section.
 * See ihw_enterCriticalSection() for more.
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user 
 * code will lead to a privileged exception.
 */
static ALWAYS_INLINE void ihw_leaveCriticalSection(uint32_t msr)
{
    /* The completion synchronizing character of the wrtee instruction forms the memory
       barrier, which ensures that all memory operations inside the now left critical
       section are completed before we leave (see core RM, 4.6.1, p. 151). The "memory"
       constraint ensures that the compiler won't reorder instructions from before the
       wrtee to behind it. */
    asm volatile ( /* AssemblerTemplate */
                   "wrtee %0\n"
                 : /* OutputOperands */
                 : /* InputOperands */ "r" (msr)
                 : /* Clobbers */ "memory"
                 );
} /* End of ihw_leaveCriticalSection */



/*
 * Global prototypes
 */

/** Init core HW of MCU so that it can be safely operated. */
void ihw_initMcuCoreHW(void);

#endif  /* IHW_INITMCUCOREHW_INCLUDED */
