#ifndef SC_SYSTEMCALLS_INCLUDED
#define SC_SYSTEMCALLS_INCLUDED
/**
 * @file sc_systemCalls.h
 * Definition of global interface of module sc_systemCalls.c
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

/** The enumeration of system call indexes.\n
      Caution, this enumeration needs to be always in sync with the table of function
    pointers! */
#define SC_IDX_SYS_CALL_CREATE_NEW_CONTEXT  0
#define SC_IDX_SYS_CALL_WAIT_FOR_EVENT      1
#define SC_IDX_SYS_CALL_SEND_EVENT          2

/** The number of system calls. */
#define SC_NO_SYSTEM_CALLS                  3

/** System call: Create a new execution context and possibly start it.\n
      This macro invokes the system call trap with system call index
    #SC_IDX_SYS_CALL_CREATE_NEW_CONTEXT.\n
      Find a detailed function description at function ccx_sc_createContext(), which
    implements the system call. */
#define /* void */ sc_createNewContext( /* void (*) (uint32_t) */    executionEntryPoint    \
                                      , /* uint32_t* */              stackPointer           \
                                      , /* bool */                   privilegedMode         \
                                      , /* bool */                   runImmediately         \
                                      , /* int_contextSaveDesc_t* */ pNewContextSaveDesc    \
                                      , /* int_contextSaveDesc_t* */ pThisContextSaveDesc   \
                                      , /* uint32_t */               initialData            \
                                      )                                                     \
                    int_systemCall( SC_IDX_SYS_CALL_CREATE_NEW_CONTEXT                      \
                                  , executionEntryPoint                                     \
                                  , stackPointer                                            \
                                  , privilegedMode                                          \
                                  , runImmediately                                          \
                                  , pNewContextSaveDesc                                     \
                                  , pThisContextSaveDesc                                    \
                                  , initialData                                             \
                                  )


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */



#endif  /* SC_SYSTEMCALLS_INCLUDED */
