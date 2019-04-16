#ifndef RTOS_PROCESS_INCLUDED
#define RTOS_PROCESS_INCLUDED
/**
 * @file rtos_process.h
 * Definition of global interface of module rtos_process.c
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

#include "typ_types.h"
#include "rtos_ivorHandler.h"
#include "rtos.h"


/*
 * Defines
 */

/** Index of implemented system call for aborting running tasks belonging to a given
    process and stopping that process forever (i.e. no further task or I/O driver callback
    execution). */
#define RTOS_SYSCALL_SUSPEND_PROCESS     9


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Initialize the data structure with all process descriptors. */
rtos_errorCode_t rtos_initProcesses(bool isProcessConfiguredAry[1+RTOS_NO_PROCESSES]);

/** Grant permission to particular processes for using the service rtos_suspendProcess(). */
void rtos_grantPermissionSuspendProcess(unsigned int pidOfCallingTask, unsigned int targetPID);

/** Kernel function to initially release a process. */
void rtos_osReleaseProcess(uint32_t PID);

/** Kernel function to suspend a process. */
void rtos_osSuspendProcess(uint32_t PID);

/** Kernel function to read the suspend status of a process. */
bool rtos_isProcessSuspended(uint32_t PID);

/** Get the number of task failures counted for the given process since start of the kernel. */
unsigned int rtos_getNoTotalTaskFailure(unsigned int PID);

/** Get the number of task failures of given category for the given process. */
unsigned int rtos_getNoTaskFailure(unsigned int PID, unsigned int kindOfErr);


/*
 * Inline functions
 */

#endif  /* RTOS_PROCESS_INCLUDED */
