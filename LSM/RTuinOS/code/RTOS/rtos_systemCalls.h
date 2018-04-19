#ifndef RTOS_SYSTEMCALLS_INCLUDED
#define RTOS_SYSTEMCALLS_INCLUDED
/**
 * @file rtos_systemCalls.h
 * Definition of system call interface of module rtos.c. To build up a centralized table of
 * system calls the compilation unit sc_systemCalls.c needs to know the prototypes of all
 * system call function implementations. These prototypes need to be public but the
 * functions must not be used by the application code and should therefore better be not
 * public. This file is a kind of compromise. It contains the needed prototypes but is
 * considered not to belong to the API of the RTOS. It must not be included by anybody other
 * than sc_systemCalls.c and the providing compilation unit itself.
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


/*
 * Global prototypes
 */

/** Post a set of events to the suspended tasks. Suspend the current task if the events
    resume another task of higher priority.
      @remark This is the implementation of a system call. Never call this function
    directly. */
bool rtos_sc_sendEvent(int_cmdContextSwitch_t *pCmdContextSwitch, uint32_t eventVec);

/** Suspend task until a combination of events appears or a timeout elapses.
      @remark This is the implementation of a system call. Never call this function
    directly. */
bool rtos_sc_waitForEvent( int_cmdContextSwitch_t *pCmdContextSwitch
                         , uint32_t eventMask
                         , bool all
                         , unsigned int timeout
                         );



#endif  /* RTOS_SYSTEMCALLS_INCLUDED */
