#ifndef LFD_LINFLEXDRIVER_INCLUDED
#define LFD_LINFLEXDRIVER_INCLUDED
/**
 * @file lfd_linFlexDriver.h
 * Definition of global interface of module lfd_linFlexDriver.c
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

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>


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

/** This development support variable counts the number of DMA transfers intiated since
    power-up, to do the serial output */
extern volatile unsigned long lfd_serialOutNoDMATransfers;

/** The ring buffer for serial output can be memntarily full. In such a case a sent message
    can be truncated (from a few bytes shortend up to entirely lost). This development
    support variable counts the number of message since power-up, which underwent
    trunction. */
extern volatile unsigned long lfd_serialOutNoTruncatedMsgs;

/** The ring buffer for serial output can be memntarily full. In such a case a sent message
    can be truncated (from a few bytes shortend up to entirely lost). This development
    support variable counts the number of truncated, lost message characters since
    power-up. */ 
extern volatile unsigned long lfd_serialOutNoLostMsgBytes;



/*
 * Global prototypes
 */

void ldf_initSerialInterface(unsigned int baudRate);
signed int lfd_writeSerial(const void *msg, size_t noBytes);

#endif  /* LFD_LINFLEXDRIVER_INCLUDED */


 
