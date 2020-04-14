#ifndef GSL_SYSTEMLOAD_INCLUDED
#define GSL_SYSTEMLOAD_INCLUDED
/**
 * @file gsl_systemLoad.h
 * Definition of global interface of module gsl_systemLoad.c
 *
 * This file has been downloaded from
 * https://svn.code.sf.net/p/rtuinos/code/trunk/code/RTOS/gsl_systemLoad.c on May 19, 2017.
 *
 * Copyright (C) 2012-2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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

#ifdef __VLE__
/** Preprocessor switch dependend implementation of global, hardware based system time in
    CPU clock ticks. See gsl_ppc_get_timebase() for details. This macro can be used in
    source code, which should not depend on the instruction set, VLE or BOOK E. */
# define GSL_PPC_GET_TIMEBASE() gsl_ppc_get_timebase()
#else
# define GSL_PPC_GET_TIMEBASE() __builtin_ppc_get_timebase()
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

#ifdef __VLE__
/**
 * Return the world time, which has elapsed since power up in units of CPU clock ticks. This
 * function is a substitute for the GCC builtin \a __builtin_ppc_get_timebase(), which fails
 * to compile with MinGW-powerpc-eabivle-4.9.4. See https://community.nxp.com/message/966808
 * for details.
 *   @return Get the world time elapsed since power up in units of 8.333... ns; our startup
 * code configures the CPU clock frequency to 120 Mhz=1/(8+1/3)ns.
 *   @remark
 * The function is compiled only in VLE mode, use \a __builtin_ppc_get_timebase() instead
 * in Book E applications.
 */
static inline uint64_t gsl_ppc_get_timebase(void)
{
    uint32_t TBU, TBU2nd, TBL;
    while(true)
    {
        asm volatile ( /* AssemblerTemplate */
                       "mfspr %0, 269\n\r" // SPR 269 = TBU
                       "mfspr %2, 268\n\r" // SPR 268 = TBL
                       "mfspr %1, 269\n\r" // SPR 269 = TBU
                     : /* OutputOperands */ "=r" (TBU),  "=r" (TBU2nd), "=r" (TBL)
                     : /* InputOperands */
                     : /* Clobbers */
                     );
        if(TBU == TBU2nd)
            break;
    }
    return ((uint64_t)TBU << 32) | TBL;
    
} /* End of gsl_ppc_get_timebase */
#endif


/*
 * Global prototypes
 */

/** Estimate the current system load. Must be used from the idle task only and takes above
    one second to execute. */
unsigned int gsl_getSystemLoad(void);


#endif  /* GSL_SYSTEMLOAD_INCLUDED */
