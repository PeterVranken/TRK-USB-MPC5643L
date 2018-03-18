#ifndef CCX_CREATECONTEXT_INCLUDED
#define CCX_CREATECONTEXT_INCLUDED
/**
 * @file ccx_createContext.h
 * Definition of global interface of module ccx_createContext.c
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


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Implementation of system call to create a new execution context. */
uint32_t ccx_sc_createContext( int_cmdContextSwitch_t *pCmdContextSwitch
                             , void (*executionEntryPoint)(uint32_t)
                             , uint32_t *stackPointer
                             , bool privilegedMode
                             , bool runImmediately
                             , int_contextSaveDesc_t *pNewContextSaveDesc
                             , int_contextSaveDesc_t *pThisContextSaveDesc
                             , uint32_t initialData
                             );

#endif  /* CCX_CREATECONTEXT_INCLUDED */
