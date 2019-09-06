#ifndef RTOS_PROCESS_DEFSYSCALLS_INCLUDED
#define RTOS_PROCESS_DEFSYSCALLS_INCLUDED
/**
 * @file rtos_process_defSysCalls.h
 * Definition of global interface of module rtos_process_defSysCalls.c
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

#include "rtos.h"
#include "rtos_process.h"


/*
 * Defines
 */

#ifndef RTOS_SYSCALL_TABLE_ENTRY_0009
# if RTOS_SYSCALL_SUSPEND_PROCESS != 9
#  error Inconsistent definition of system call
# endif
# define RTOS_SYSCALL_TABLE_ENTRY_0009  \
                    RTOS_SC_TABLE_ENTRY(rtos_scSmplHdlr_suspendProcess, SIMPLE)
#else
# error System call 0009 is ambiguously defined
/* We purposely redefine the table entry and despite of the already reported error; this
   makes the compiler emit a message with the location of the conflicting previous
   definition.*/
# define RTOS_SYSCALL_TABLE_ENTRY_0009    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
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

/* System call implementation to suspend a process: Abort the running tasks belonging to
   the given process and stop that process forever (i.e. no further task or I/O driver
   callback execution). */
void rtos_scSmplHdlr_suspendProcess(uint32_t callingPid, uint32_t PID);

#endif  /* RTOS_PROCESS_DEFSYSCALLS_INCLUDED */
