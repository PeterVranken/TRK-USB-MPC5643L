#ifndef CCX_STARTCONTEXT_INCLUDED
#define CCX_STARTCONTEXT_INCLUDED
/**
 * @file ccx_startContext.h
 * Definition of global interface of module ccx_startContext.S.\n
 *   Note, this file is shared between C and assembly code. Therefore, many of the
 * contained elements are put into preprocessor conditions so that they are read by a C
 * compilation process only.
 *
 * Copyright (C) 2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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

#if defined(__STDC_VERSION__)
# include <stdint.h>
# include <stdbool.h>
#endif


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
 * Global prototypes
 */

#ifdef __STDC_VERSION__
/**
 * Prototype of assembler function, which branches into the new context and which can make
 * it on exit return to a predetermined other function to notify the
 * end-of-execution-context to whom it may concern.
 *   @param contextParam
 * This value is pssed to the context entry function as its only function argument.
 */
void ccx_startContext(uint32_t contextParam);
#endif

#endif  /* CCX_STARTCONTEXT_INCLUDED */
