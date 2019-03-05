#ifndef SIO_SERIALIO_DEFSYSCALLS_INCLUDED
#define SIO_SERIALIO_DEFSYSCALLS_INCLUDED
/**
 * @file sio_serialIO_defSysCalls.h
 * Declaration of system calls offered by and implemented in module sio_serialIO.c. This
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

#include <stdint.h>
#include <stdbool.h>

#include "sio_serialIO.h"


/*
 * Defines
 */

#ifndef SC_SYSCALL_TABLE_ENTRY_0004
# if SIO_SYSCALL_WRITE_SERIAL != 4
#  error Inconsistent definition of system call
# endif
# define SC_SYSCALL_TABLE_ENTRY_0004   { .addressOfFct = (uint32_t)sio_scFlHdlr_writeSerial \
                                       , .conformanceClass = SC_HDLR_CONF_CLASS_FULL        \
                                       }
#else
# error System call 0004 is ambiguously defined
/* We purposely redefine the table entry and despite of the already reported error; this
   make the compiler emit a message with the location of the conflicting previous
   definition.*/
# define SC_SYSCALL_TABLE_ENTRY_0004    SC_SYSCALL_DUMMY_TABLE_ENTRY
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

/* Preemptable system call implementation to write to the serial interface. */
unsigned int sio_scFlHdlr_writeSerial( uint32_t pidOfCallingTask
                                     , const char *msg
                                     , unsigned int noBytes
                                     );

#endif  /* SIO_SERIALIO_DEFSYSCALLS_INCLUDED */
