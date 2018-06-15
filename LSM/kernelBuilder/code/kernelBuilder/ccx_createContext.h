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

/** Create a (still suspended) new context and its descriptor for later resume. */
void ccx_createContext( int_contextSaveDesc_t *pContextSaveDesc
                      , void *stackPointer
                      , int_fctEntryIntoContext_t fctEntryIntoContext
                      , bool privilegedMode
                      );

/** Create the descriptor for an execution context intended for on-the-fly creation. */
void ccx_createContextOnTheFly
                ( int_contextSaveDesc_t *pNewContextSaveDesc
                , void *stackPointer
                , int_fctEntryIntoContext_t fctEntryIntoOnTheFlyStartedContext
                , bool privilegedMode
                );

#if INT_USE_SHARED_STACKS == 1
/** Create a new execution context, which shares the stack with another context. */
void ccx_createContextShareStack( int_contextSaveDesc_t *pNewContextSaveDesc
                                , const int_contextSaveDesc_t *pPeerContextSaveDesc
                                , int_fctEntryIntoContext_t fctEntryIntoContext
                                , bool privilegedMode
                                );
#endif

#endif  /* CCX_CREATECONTEXT_INCLUDED */
