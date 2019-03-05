#ifndef IVR_IVORHANDLER_DEFSYSCALLS_INCLUDED
#define IVR_IVORHANDLER_DEFSYSCALLS_INCLUDED
/**
 * @file ivr_ivorHandler_defSysCalls.h
 * Declaration of system calls offered by and implemented in module ivr_ivorHandler.S. This
 * header file has to be included by source file sc_systemCall.c, which collects all system
 * call declarations and assembles the const table of system call descriptors.
 *
 * Copyright (C) 2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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

#include "typ_types.h"
#include "ivr_ivorHandler.h"


/*
 * Defines
 */

#ifndef SC_SYSCALL_TABLE_ENTRY_0000
# if IVR_SYSCALL_SUSPEND_TERMINATE_TASK != 0
#  error Inconsistent definition of system call
# endif
/* The basic system call handler is assembler implemented in ivr_ivorHandler.S. Note,
   despite of the C style prototype, this is not a C callable function. The calling
   convention is different to C. This is the reason, why we declare it here instead of
   publishing it globally in ivr_ivorHandler.h. */
extern void ivr_scBscHdlr_terminateUserTask(int32_t result);
# define SC_SYSCALL_TABLE_ENTRY_0000                                        \
            { .addressOfFct = (uint32_t)ivr_scBscHdlr_terminateUserTask     \
            , .conformanceClass = SC_HDLR_CONF_CLASS_BASIC                  \
            }
#else
# error System call 0000 is ambiguously defined
/* We purposely redefine the table entry and despite of the already reported error; this
   make the compiler emit a message with the location of the conflicting previous
   definition.*/
# define SC_SYSCALL_TABLE_ENTRY_0000    SC_SYSCALL_DUMMY_TABLE_ENTRY
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


/*
 * Global inline functions
 */

#endif  /* IVR_IVORHANDLER_DEFSYSCALLS_INCLUDED */
