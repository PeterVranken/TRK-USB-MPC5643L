#ifndef RTOS_DEFSYSCALLS_INCLUDED
#define RTOS_DEFSYSCALLS_INCLUDED
/**
 * @file rtos_defSysCalls.h
 * Declaration of system calls offered by and implemented in module rtos.c. This header
 * file has to be included by source file sc_systemCall.c, which collects all system call
 * declarations and assembles the const table of system call descriptors.
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

#include <stdint.h>
#include <stdbool.h>

#include "prc_process.h"
#include "rtos.h"


/*
 * Defines
 */

#ifndef SC_SYSCALL_TABLE_ENTRY_0005
# if RTOS_SYSCALL_TRIGGER_EVENT != 5
#  error Inconsistent definition of system call
# endif
# define SC_SYSCALL_TABLE_ENTRY_0005                                    \
            { .addressOfFct = (uint32_t)rtos_scFlHdlr_triggerEvent      \
            , .conformanceClass = SC_HDLR_CONF_CLASS_FULL               \
            }
#else
# error System call 0005 is ambiguously defined
/* We purposely redefine the table entry and despite of the already reported error; this
   make the compiler emit a message with the location of the conflicting previous
   definition.*/
# define SC_SYSCALL_TABLE_ENTRY_0005    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif


#ifndef SC_SYSCALL_TABLE_ENTRY_0010
# if RTOS_SYSCALL_RUN_TASK != 10
#  error Inconsistent definition of system call
# endif
# define SC_SYSCALL_TABLE_ENTRY_0010                                    \
            { .addressOfFct = (uint32_t)rtos_scFlHdlr_runTask           \
            , .conformanceClass = SC_HDLR_CONF_CLASS_FULL               \
            }
#else
# error System call 0010 is ambiguously defined
/* We purposely redefine the table entry and despite of the already reported error; this
   make the compiler emit a message with the location of the conflicting previous
   definition.*/
# define SC_SYSCALL_TABLE_ENTRY_0010    SC_SYSCALL_DUMMY_TABLE_ENTRY
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

/** System call handler implementation to activate a task. */
uint32_t rtos_scFlHdlr_triggerEvent(unsigned int pidOfCallingTask, unsigned int idEvent);

/** System call handler implementation to create and run a task in another process. */
uint32_t rtos_scFlHdlr_runTask( unsigned int pidOfCallingTask
                              , const prc_userTaskConfig_t *pUserTaskConfig
                              , uintptr_t taskParam
                              );

#endif  /* RTOS_DEFSYSCALLS_INCLUDED */
