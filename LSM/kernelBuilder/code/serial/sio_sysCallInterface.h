#ifndef SIO_SYSCALLINTERFACE_INCLUDED
#define SIO_SYSCALLINTERFACE_INCLUDED
/**
 * @file sio_sysCallInterface.h
 * Definition of global interface of module sio_sysCallInterface.c
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

/** If the package is integrated into a project that builds on package kernelBuilder then
    the API of sio_serial is most likely needed in form of system calls. To enable
    compilation of the system call API this macro is set to 1. The macro needs to be
    defined as 0 if kernelBuilder is not element of the project. */
#define SIO_USE_KERNEL_BUILDER_SYSTEM_CALLS     1

#if SIO_USE_KERNEL_BUILDER_SYSTEM_CALLS == 1

/*
 * Include files
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "sc_systemCalls.h"

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

/** System call implementation: Never call this function directly. */
uint32_t _sio_sc_writeSerial( uint32_t * const pMSR ATTRIB_UNUSED
                            , const char *msg
                            , size_t noBytes
                            );

/** System call implementation: Never call this function directly. */
uint32_t _sio_sc_getChar(uint32_t * const pMSR ATTRIB_UNUSED);

/** System call implementation: Never call this function directly. */
uint32_t _sio_sc_getLine( uint32_t * const pMSR ATTRIB_UNUSED
                        , char str[]
                        , unsigned int sizeOfStr
                        );

/*
 * System calls
 */

/** Simple system call: Invoke the API sio_writeSerial() from the serial I/O driver as a
    system call. For a detailed function description refer to the API function. */
#define /* void */ sio_sc_writeSerial(/* const char * */ msg, /* size_t */ noBytes )    \
                    int_systemCall(SIO_IDX_SIMPLE_SYS_CALL_SIO_WRITE_SERIAL, msg, noBytes)

/** Simple system call: Invoke the API sio_getChar() from the serial I/O driver as a
    system call. For a detailed function description refer to the API function. */
#define /* signed int */ sio_sc_getChar(/* void */) \
                    int_systemCall(SIO_IDX_SIMPLE_SYS_CALL_SIO_GET_CHAR)

/** Simple system call: Invoke the API sio_getLine() from the serial I/O driver as a
    system call. For a detailed function description refer to the API function. */
#define /* char* */ sio_sc_getLine(/* char[] */ str, /* unsigned int */ sizeOfStr)   \
                    int_systemCall(SIO_IDX_SIMPLE_SYS_CALL_SIO_GET_LINE, str, sizeOfStr)

#endif /* SIO_USE_KERNEL_BUILDER_SYSTEM_CALLS == 1 */
#endif  /* SIO_SYSCALLINTERFACE_INCLUDED */
