/**
 * @file prf_printf.c
 * The compilation of this module together with sio_serialIO.c connects the GCC's C library
 * to the serial output over RS 232. The functions for formatted output through stdout and
 * stderr can be used to write to a terminal, which is running on the host machine. For the
 * TRK-USB_MPC5643L evaluation board it means to use its normal USB connection for printing
 * massages.\n
 *   The connection of the serial input interface to the C library was not possible. The
 * way, how the library functions request more chunks of data from the stream does not fit
 * to the character of a serial input, which can be temporarily exhausted but which will
 * have new data some time later. We couldn't find a way to satisfy the interface of the C
 * library (mainly through function read()) without unacceptable blocking states. As far as
 * input is concerned, you will have to build your application directly on the API of
 * module sio_serialIO.c.
 *   @remark Note, this function does not provide any directly used function or data
 * object. Just compile and link it and successfully use \a printf, \a fprintf, \a put,
 * \a fputs, \a putchar.
 *   @remark By using \a printf or one of the other functions in your application you will
 * get a significant additional RAM and ROM consumption. Particular the RAM consumption
 * will raise even more, if \a printf makes use of a floating point formatting character,
 * like in "%.3f".
 *   @remark Using \a printf with floating point formatting characters is a very expensive
 * operation in term of CPU load, too. The C library has not been compiled with
 * -fshort-double and does perform the real 64 Bit operations as required by the C
 * standard. All of this is done by the emulation library since there is no hardware support
 * for 64 Bit operations in the MPC5643L.
 *   @remark The memory allocation concept of \a printf and else is untransparent. We
 * implemented a primitive substitute for the required function \a sbrk, which basically
 * works but which is not safe. We didn't have a true specification of the behavior of this
 * function and can't guarantee that it is working fully as expected. Furthermore, the maximum
 * space for this function needs to be reserved at compile time and we don't want to
 * reserve more than useful. This means difficult trade off between likelihood of out
 * of memory errors at run time and waste of expensive RAM.
 *   @remark For several reasons, and particularly because of the last two remarks, the use
 * of \a printf and else must never be considered in production code. (Whereas it is fine to
 * use the API of sio_serialIO.c in production code.) As a rule of thumb, all occurrences of
 * #include "f2d_float2Double.h" and of \a printf, \a fprintf, etc. must be surrounded by
 * preprocessor conditions, that permit their compilation only in DEBUG configuration.
 *
 * Copyright (C) 2017 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   sbrk
 *   fstat
 *   isatty
 *   close
 *   lseek
 *   write
 *   read
 * Local functions
 */

/*
 * Include files
 */

#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "typ_types.h"
#include "sio_serialIO.h"
#include "f2d_float2Double.h"


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

/** Debug support for adjusting the memory allocation to your needs. The number of
    invokations of the low level memmory allocation function \a sbrk are counted. */
unsigned long prf_sbrk_noRequests = 0;
 
/** Debug support for adjusting the memory allocation to your needs. The total number of
    requested bytes of RAM in all invokations of the low level memmory allocation function
    \a sbrk is recorded. (This includes those requests, which could not be satisfied.) */
unsigned long prf_sbrk_totalIncrement = 0;


/*
 * Function implementation
 */


/**
 * Implementation of function sbrk, a function which is required if printf is used, but
 * which is not implemented in the stdlib -- there's no low level memory allocation
 * implemented in the library.\n
 *   This function provides memory to \a printf and else as working areas of the text
 * formatting operations.\n
 *   A linker provided chunk of reserved memory is return to the requesting caller piece by
 * piece. In the first call the pointer to the beginning of the reserved memory is
 * returned. In the second call the pointer advanced by the value of \a increment from the
 * first call is returned and so on. No alignment adjustment is done as in malloc. Only the
 * first returned pointer is guaranteed to be properly aligned.
 *   @return
 * The pointer to next chunk of free memory, which can be used by the caller. The pointer
 * point to a chunk of \a increment Byte.
 *   @param increment
 * The number of requested Byte of memory.
 *   @remark
 * Refer to https://github.com/eblot/newlib/blob/master/newlib/libc/reent/sbrkr.c,
 * https://en.wikipedia.org/wiki/Sbrk and
 * https://www.gnu.org/software/libc/manual/html_node/Resizing-the-Data-Segment.html#Resizing-the-Data-Segment.
 */
void *sbrk(ptrdiff_t increment)
{
    /* Record the use of this function. The information retrieved from these variables may
       help adjusting the size of the linker reserved memory chunk. */
    ++ prf_sbrk_noRequests;
    prf_sbrk_totalIncrement += (unsigned long)increment;
    
    /* Shape access to the linker provided reserved memory for sbrk. */
    extern uint8_t ld_sbrkStart[0]
                 , ld_sbrkEnd[0];
                 
    /* Check alignment, should be no less than double alignment. Correct linker file if
       this assertion fires. */
    assert(((uintptr_t)ld_sbrkStart & (8-1)) == 0);
    
    static uint8_t *pNextChunk_ = ld_sbrkStart;
    if(pNextChunk_ + increment <= ld_sbrkEnd)
    {
        void *result = (void*)pNextChunk_;
        pNextChunk_ += increment;
        return result;
    }
    else
    {
        assert(false);
        return (void*)-1;
    }
} /* End of sbrk */



/**
 * Stub function for fstat, a function which is required if printf is used, but which is
 * not implemented in the stdlib -- there's no file system in the library and no low level
 * binding of the standard streams to some I/O.\n
 *   All function arguments are ignored.
 *   @return 
 * The stub always returns the error code of the C stdlib.
 *   @remark
 * Refer to https://github.com/eblot/newlib/blob/master/newlib/libc/reent/fstatr.c
 */
int fstat(int fildes ATTRIB_UNUSED, struct stat *buf ATTRIB_UNUSED)
{
    return -1;
    
} /* End of fstat */
    
    

/**
 * Stub function for isatty, a function which is required by the linker (but not invoked at
 * runtime) if printf is used, but which is not implemented in the stdlib -- there's no
 * file system in the library and no low level binding of the standard streams to some
 * I/O.\n
 *   All function arguments are ignored.\n
 *   An assertion will fire on unexpected invocation.
 *   @return 
 * The stub always returns true.
 *   @remark
 * Refer to https://github.com/eblot/newlib/blob/master/newlib/libc/reent/isattyr.c
 */
int isatty(int fildes ATTRIB_UNUSED)
{
    assert(false);
    return 1;
    
} /* End of isatty */

    
    
/**
 * Stub function for close, a function which is required by the linker (but not invoked at
 * runtime) if printf is used, but which is not implemented in the stdlib -- there's no
 * file system in the library and no low level binding of the standard streams to some
 * I/O.\n
 *   All function arguments are ignored.\n
 *   An assertion will fire on unexpected invocation.
 *   @return 
 * The stub always returns zero, not indicating a problem.
 *   @remark
 * Refer to https://github.com/eblot/newlib/blob/master/newlib/libc/reent/closer.c
 */
int close(int fildes ATTRIB_UNUSED)
{
    assert(false);
    return 0;
    
} /* End of close */
    
    
    
/**
 * Stub function for lseek, a function which is required by the linker (but not invoked at
 * runtime) if printf is used, but which is not implemented in the stdlib -- there's no
 * file system in the library and no low level binding of the standard streams to some
 * I/O.\n
 *   All function arguments are ignored.\n
 *   An assertion will fire on unexpected invocation.
 *   @return 
 * The stub always returns the error code of the C stdlib.
 *   @remark
 * Refer to https://github.com/eblot/newlib/blob/master/newlib/libc/reent/lseekr.c
 */
off_t lseek(int fildes ATTRIB_UNUSED, off_t offset ATTRIB_UNUSED, int whence ATTRIB_UNUSED)
{
    assert(false);
    return -1;
    
} /* End of lseek */



/**
 * Implementation of function write, a function which is required if printf is used, but
 * which is not implemented in the stdlib -- there's no file system in the library and no
 * low level binding of the standard streams to some I/O.\n
 *   This function connects the stream outout of the C library, which goes through stdout
 * or stderr to our own implementation of a serial I/O channel.
 *   @return 
 * The actual number of written characters. Normally the same as \a noBytes, but can be
 * less if the serial send buffer is temporarily full. The difference \a noBytes - returned
 * value is the count of lost characters, these characters will not appear in the serial
 * output.
 *   @param file
 * The file stream ID from the stdlib. We only support \a stdout and \a stderr, the two
 * always open output streams. Both are redirected into our serial output. No characters
 * are written for other stream IDs.
 *   @param msg
 * The character string to be written. Although formally a pointer to void \a msg is
 * understood as pointer to bytes. \a msg is not a C string, zero bytes will be sent as
 * any other bytes.
 *   @param noBytes
 * The number of bytes to be sent.
 *   @remark
 * Other streams than stdout and stderr (i.e. files from the stdlib) would also work but
 * they are not supported by this interface as it is useless and would require open and
 * close functionality.
 *   @remark
 * Refer to https://github.com/eblot/newlib/blob/master/newlib/libc/reent/writer.c
 */
signed int write(int file, const void *msg, size_t noBytes)
{

    if(noBytes == 0  ||  msg == NULL  ||  (file != stdout->_file  &&  file != stderr->_file))
        return 0;
    else
    {
        /// @todo Do we need EOL conversion?
        return (signed int)sio_writeSerial((const char*)msg, (unsigned int)noBytes);
    }
} /* End of write */


/**
 * Stub function for read, a function which is required by the linker (but not invoked at
 * runtime) if printf is used, but which is not implemented in the stdlib -- there's no
 * file system in the library and no low level binding of the standard streams to some
 * I/O.\n
 *   Note, this function would be invoked when the stdin stream would be use, e.g. by
 * calling \a scanf or \a getchar. It was not possible to make this function fit to the
 * behavior of our serial input stream and stdin must therefore never be used. An assertion
 * will fire on unexpected invocation.\n
 *   All function arguments are ignored.
 *   @return
 * The stub always returns the error code of the C stdlib.
 *   @remark
 * Refer to https://github.com/eblot/newlib/blob/master/newlib/libc/reent/readr.c
 */
ssize_t read(int fildes ATTRIB_UNUSED, void *buf ATTRIB_UNUSED, size_t nbytes ATTRIB_UNUSED)
{
    assert(false);
    return -1;

} /* End of read */