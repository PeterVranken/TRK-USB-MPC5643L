#ifndef SIO_SERIALIO_INCLUDED
#define SIO_SERIALIO_INCLUDED
/**
 * @file sio_serialIO.h
 * Definition of global interface of module sio_serialIO.c
 *
 * Copyright (C) 2017-2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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


/*
 * Defines
 */

/** Index of system call for writing into serial output. */
#define SIO_SYSCALL_WRITE_SERIAL    20


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */

/** This development support variable counts the number of DMA transfers intiated since
    power-up, to do the serial output */
extern volatile unsigned long sio_serialOutNoDMATransfers;

/** The ring buffer for serial output can be memntarily full. In such a case a sent message
    can be truncated (from a few bytes shortend up to entirely lost). This development
    support variable counts the number of message since power-up, which underwent
    trunction. */
extern volatile unsigned long sio_serialOutNoTruncatedMsgs;

/** The ring buffer for serial output can be memntarily full. In such a case a sent message
    can be truncated (from a few bytes shortend up to entirely lost). This development
    support variable counts the number of message since power-up, which underwent
    trunction.
      @remark Because of the race conditions between serial I/O interrupt an application
    software can not clearly relate a change of \a sio_serialOutNoTruncatedMsgs to a
    particular character or message it gets from the read functions sio_osGetChar or
    sio_osGetLine. In particular, it must not try to reset the counter prior to a read
    operation in order to establish such a relation. The application will just know that
    there are garbled messages. */
extern volatile unsigned long sio_serialOutNoLostMsgBytes;

/** The ring buffer for serial output can be memntarily full. In such a case a sent message
    can be truncated (from a few bytes shortend up to entirely lost). This development
    support variable counts the number of truncated, lost message characters since
    power-up.
      @remark See \a sio_serialOutNoTruncatedMsgs for race conditions with the input
    functions of this module's API. Just the same holds for \a
    sio_serialOutNoLostMsgBytes. */ 
extern volatile unsigned long sio_serialInLostBytes;

#ifdef DEBUG
/** Count all input characters received since last reset. This variable is support in DEBUG
    compilation only. */
extern volatile unsigned long sio_serialInNoRxBytes;
#endif


/*
 * Global prototypes
 */

/** Module initialization. Configure the I/O devices for serial output. */
void sio_initSerialInterface(unsigned int baudRate);

/** Write a character string into the serial interface. Can be called from OS context. */
unsigned int sio_osWriteSerial(const char *msg, unsigned int noBytes);

/** Application API function to read a single character from serial input. */
signed int sio_osGetChar(void);

/* Read a line of input text from the serial interface. */
char *sio_osGetLine(char buf[], unsigned int sizeOfBuf);


/*
 * Inline functions
 */

/** 
 * Principal API function for data output. A byte string is sent through the serial
 * interface. Actually, the bytes are queued for sending and the function is non-blocking. 
 *   @return
 * The number of queued bytes is returned. Normally, this is the same value as argument \a
 * noBytes. However, the byte sequence can be longer than the currently available space in
 * the send buffer. (Its size is fixed and no reallocation strategy is implemented.) The
 * tranmitted message will be truncated if the return value is less than function argument
 * \a noBytes.
 *   @param msg
 * The byte sequence to send. Note, this may be but is not necessarily a C string with zero
 * terminations. Zero bytes can be send, too.\n
 *   Note, the memory region spawn by \a msg and \a noBytes must be entirely inside the
 * used portions of RAM and ROM. Any attempt to print other data will be punished with task
 * abortion.
 *   @param noBytes
 * The number of bytes to send. For a C string, this will mostly be \a strlen(msg).
 *   @remark
 * This function must be called from the user task context only. Any attempt to use it from 
 * OS code will lead to undefined behavior.
 */
static inline unsigned int sio_writeSerial(const char *msg, unsigned int noBytes)
{
    return rtos_systemCall(SIO_SYSCALL_WRITE_SERIAL, msg, noBytes);

} /* End of sio_writeSerial */


#endif  /* SIO_SERIALIO_INCLUDED */


 
