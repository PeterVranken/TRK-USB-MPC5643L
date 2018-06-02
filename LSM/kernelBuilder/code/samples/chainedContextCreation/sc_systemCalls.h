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

#include "int_interruptHandler.h"
#include "lbd_sysCallInterface.tableEntries.h"
#include "sio_sysCallInterface.tableEntries.h"


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

/** The enumeration of indexes of kernel relevant system calls.\n
      Note, kernel relevant system calls are distinguished from simple system calls in that
    they use the negative range of indexes.\n
      Caution, this enumeration needs to be always in sync with table
    int_systemCallHandlerAry of function pointers! */
#define SC_IDX_SYS_CALL_CREATE_NEW_CONTEXT  (-1)
#define SC_IDX_SYS_CALL_SWITCH_CONTEXT      (-2)

/** The number of kernel relevant system calls. */
#define SC_NO_SYSTEM_CALLS                  2

/** System call: Create a new execution context and possibly start it.\n
      This macro invokes the system call trap with system call index
    #SC_IDX_SYS_CALL_CREATE_NEW_CONTEXT.\n
      Find a detailed function description at function ccx_sc_createContext(), which
    implements the system call. */
#define /* void */ sc_createNewContext( /* const xsw_contextDesc_t* */ pNewContextDesc      \
                                      , /* bool */                     runImmediately       \
                                      , /* uint32_t */                 initialData          \
                                      , /* int_contextSaveDesc_t* */   pNewContextSaveDesc  \
                                      , /* int_contextSaveDesc_t* */   pThisContextSaveDesc \
                                      )                                                     \
                    (void)(int_systemCall( SC_IDX_SYS_CALL_CREATE_NEW_CONTEXT               \
                                         , pNewContextDesc                                  \
                                         , runImmediately                                   \
                                         , initialData                                      \
                                         , pNewContextSaveDesc                              \
                                         , pThisContextSaveDesc                             \
                                         )                                                  \
                          )

/** System call: Immediate, cooperative context switch.\n
      This macro invokes the system call trap with system call index
    #SC_IDX_SYS_CALL_SWITCH_CONTEXT.\n
      Find a detailed function description at function cxs_sc_switchContext(), which
    implements the system call. */
#define /* uint32_t */ sc_switchContext( /* unsigned int */ idxOfResumedContext             \
                                       , /* uint32_t */ signalToResumedContext              \
                                       )                                                    \
                    int_systemCall( SC_IDX_SYS_CALL_SWITCH_CONTEXT                          \
                                  , idxOfResumedContext                                     \
                                  , signalToResumedContext                                  \
                                  )


/** The enumeration of indexes of kernel unrelated, simple system calls.\n
      Caution, this enumeration needs to be always in sync with table
    int_simpleSystemCallHandlerAry of function pointers! */
typedef enum sc_enum_simpleSystemCallIndex_t
{
    SIO_SIMPLE_SYSTEM_CALLS_ENUMERATION
    LBD_SIMPLE_SYSTEM_CALLS_ENUMERATION
    
    /** The number of kernel unrelated, simple system calls. */
    SC_NO_SIMPLE_SYSTEM_CALLS

} sc_enum_simpleSystemCallIndex_t;



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
