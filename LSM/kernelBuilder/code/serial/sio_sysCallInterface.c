/**
 * @file sio_sysCallInterface.c
 * This file implements a wrapper around the global interface of sio_serialIO.c, which
 * makes the driver functions available to user mode code.\n
 *   Module sio_sc_serialIO had been developed as a raw I/O driver. Later, we had
 * introduced kernelBuilder as a framework for kernel design. If kernelBuilder is applied
 * for the application then there will likely be some threads or tasks, which want to make
 * use of the serial I/O. However, if they are run in user mode - which is normal design of
 * application tasks - then they can't directly use the driver API and this wrapper is
 * required. The wrapper offers system call implementations, which can be put into
 * kernelBuilder's global table of system calls and the application code will rather make
 * the system calls than directly calling the driver functions.\n
 *   Note, the wrapper cannot be compiled in an environment without kernelBuilder. To make
 * the package serial still be self-contained and not dependent on kernelBuilder, we add a
 * preprocessor configuration macro to switch the system call support on or off.
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
/* Module interface
 *   _sio_sc_writeSerial
 *   _sio_sc_getChar
 *   _sio_sc_getLine
 * Local functions
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "typ_types.h"
#include "sio_serialIO.h"
#include "sio_sysCallInterface.h"

#if SIO_USE_KERNEL_BUILDER_SYSTEM_CALLS == 1

/*
 * Defines
 */
 

/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
 
/*
 * Data definitions
 */
 
 
/*
 * Function implementation
 */

/**
 * Application API function to read a single character from serial input or EOF if there's
 * no such character received meanwhile.
 *   This function is a wrapper around the according API offered by the driver. The wrapper
 * is implemented as a kernelBuilder simple system call, which makes the driver API
 * available to task running in user mode. The behavior of the system call is identical to
 * the driver API function. Refer to the documentation of sio_getChar().
 *   @param pMSR
 * The kernelBuilder API offers to continue the calling context with changed machine
 * status. The CPU register MSR value can be accessed by reference. However, this system
 * call doesn't change the machine status.
 *   @remark
 * This is the implementation of a system call. Never call this function directly. Only use
 * int_systemCall() from the kernelBuilder API to invoke it. Any attempt to call this
 * function in user mode will cause an exception.
 */
uint32_t _sio_sc_getChar(uint32_t * const pMSR ATTRIB_UNUSED)
{
    return (uint32_t)sio_getChar();

} /* End of _sio_sc_getChar */



/**
 * The function reads a line of text from serial in and stores it into the string pointed
 * to by \a str.
 *   This function is a wrapper around the according API offered by the driver. The wrapper
 * is implemented as a kernelBuilder simple system call, which makes the driver API
 * available to task running in user mode. The behavior of the system call is identical to
 * the driver API function. Refer to the documentation of sio_getLine().
 *   @param pMSR
 * The kernelBuilder API offers to continue the calling context with changed machine
 * status. The CPU register MSR value can be accessed by reference. However, this system
 * call doesn't change the machine status.
 *   @param str
 * This is the pointer to an array of chars where the C string is stored. \a str is the
 * empty string if the function returns NULL. See sio_getLine() for more.
 *   @param sizeOfStr
 * The capacity of \a str in Byte. The maximum message length is one less since a
 * terminating zero character is always appended. See sio_getLine() for more.
 *   @remark
 * This is the implementation of a system call. Never call this function directly. Only use
 * int_systemCall() from the kernelBuilder API to invoke it. Any attempt to call this
 * function in user mode will cause an exception.
 */
uint32_t _sio_sc_getLine( uint32_t * const pMSR ATTRIB_UNUSED
                        , char str[]
                        , unsigned int sizeOfStr
                        )
{
    return (uint32_t)sio_getLine(str, sizeOfStr);

} /* End of _sio_sc_getLine */



/**
 * A byte string is sent through the serial interface.\n
 *   This function is a wrapper around the according API offered by the driver. The wrapper
 * is implemented as a kernelBuilder simple system call, which makes the driver API
 * available to task running in user mode. The behavior of the system call is identical to
 * the driver API function. Refer to the documentation of sio_writeSerial().
 *   @param pMSR
 * The kernelBuilder API offers to continue the calling context with changed machine
 * status. The CPU register MSR value can be accessed by reference. However, this system
 * call doesn't change the machine status.
 *   @param msg
 * The string to send to the serial interface. See sio_writeSerial() for more.
 *   @param noBytes
 * The string length. See sio_writeSerial() for more.
 *   @remark
 * This is the implementation of a system call. Never call this function directly. Only use
 * int_systemCall() from the kernelBuilder API to invoke it. Any attempt to call this
 * function in user mode will cause an exception.
 */
uint32_t _sio_sc_writeSerial( uint32_t * const pMSR ATTRIB_UNUSED
                           , const char *msg
                           , size_t noBytes
                           )
{
    return (uint32_t)sio_writeSerial(msg, noBytes);

} /* End of _sio_sc_writeSerial */

#endif /* SIO_USE_KERNEL_BUILDER_SYSTEM_CALLS == 1 */
