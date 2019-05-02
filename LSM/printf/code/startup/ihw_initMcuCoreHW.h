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
 *   ihw_installINTCInterruptHandler
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
 */
static ALWAYS_INLINE void ihw_suspendAllInterrupts()
{
    asm volatile ( /* AssemblerTemplate */
                   "wrteei 0\n"
                 : /* OutputOperands */
                 : /* InputOperands */
                 : /* Clobbers */
                 );
} /* End of ihw_suspendAllInterrupts */



/**
 * Enable all External Interrupts. This is done unconditionally, there's no nesting
 * counter.
 */
static ALWAYS_INLINE void ihw_resumeAllInterrupts()
{
    asm volatile ( /* AssemblerTemplate */
                   "msync\n\t"
                   "wrteei 1\n"
                 : /* OutputOperands */
                 : /* InputOperands */
                 : /* Clobbers */
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
 */
static ALWAYS_INLINE uint32_t ihw_enterCriticalSection()
{
    uint32_t msr;
    asm volatile ( /* AssemblerTemplate */
                   "mfmsr %0\n\t"
                   "wrteei 0\n\t"
                 : /* OutputOperands */ "=r" (msr)
                 : /* InputOperands */
                 : /* Clobbers */
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
 */
static ALWAYS_INLINE void ihw_leaveCriticalSection(uint32_t msr)
{
    asm volatile ( /* AssemblerTemplate */
                   "msync\n\t"
                   "mtmsr %0\n"
                 : /* OutputOperands */
                 : /* InputOperands */ "r" (msr)
                 : /* Clobbers */
                 );
} /* End of ihw_leaveCriticalSection */



/*
 * Global prototypes
 */

/** Init core HW of MCU so that it can be safely operated. */
void ihw_initMcuCoreHW(void);

/** Let the client code install an interrupt handler. */
void ihw_installINTCInterruptHandler( void (*interruptHandler)(void)
                                    , unsigned short vectorNum
                                    , unsigned char psrPriority
                                    , bool isPreemptable
                                    );

#endif  /* IHW_INITMCUCOREHW_INCLUDED */
