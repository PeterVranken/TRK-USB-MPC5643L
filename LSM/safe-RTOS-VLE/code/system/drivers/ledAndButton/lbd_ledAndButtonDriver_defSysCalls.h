#ifndef LBD_LEDANDBUTTONDRIVER_DEFSYSCALLS_INCLUDED
#define LBD_LEDANDBUTTONDRIVER_DEFSYSCALLS_INCLUDED
/**
 * @file lbd_ledAndButtonDriver_defSysCalls.h
 * Definition of global interface of module lbd_ledAndButtonDriver_defSysCalls.c
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
#include "lbd_ledAndButtonDriver.h"


/*
 * Defines
 */

#ifndef RTOS_SYSCALL_TABLE_ENTRY_0016
# if LBD_SYSCALL_SET_LED != 16
#  error Inconsistent definition of system call
# endif
# define RTOS_SYSCALL_TABLE_ENTRY_0016  RTOS_SC_TABLE_ENTRY(lbd_scSmplHdlr_setLED, SIMPLE)
#else
# error System call 0016 is ambiguously defined
/* We purposely redefine the table entry and despite of the already reported error; this
   make the compiler emit a message with the location of the conflicting previous
   definition.*/
# define RTOS_SYSCALL_TABLE_ENTRY_0016    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif


#ifndef RTOS_SYSCALL_TABLE_ENTRY_0017
# if LBD_SYSCALL_GET_BUTTON != 17
#  error Inconsistent definition of system call
# endif
# define RTOS_SYSCALL_TABLE_ENTRY_0017  RTOS_SC_TABLE_ENTRY(lbd_scSmplHdlr_getButton, SIMPLE)
#else
# error System call 0017 is ambiguously defined
/* We purposely redefine the table entry and despite of the already reported error; this
   make the compiler emit a message with the location of the conflicting previous
   definition.*/
# define RTOS_SYSCALL_TABLE_ENTRY_0017    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
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

/* System call implementation to make the LED driver available to user mode tasks. */
uint32_t lbd_scSmplHdlr_setLED(uint32_t callingPid ATTRIB_UNUSED, lbd_led_t led, bool isOn);

/* System call implementation to read the button states of SW2 and SW3 on the eval board. */
uint32_t lbd_scSmplHdlr_getButton(uint32_t callingPid ATTRIB_UNUSED, lbd_button_t button);

#endif  /* LBD_LEDANDBUTTONDRIVER_DEFSYSCALLS_INCLUDED */
