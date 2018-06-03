#ifndef LBD_SYSCALLINTERFACE_TABLEENTRIES_INCLUDED
#define LBD_SYSCALLINTERFACE_TABLEENTRIES_INCLUDED
/**
 * @file lbd_sysCallInterface.tableEntries.h
 * This file is a part of the system call interface of module lbd. It exports the indexes
 * and function table entries of all (simple) system calls, which are offered by the
 * module, as prepocessor macros. The core module sc_systemCalls will use these macros to
 * compile the one and only table of system call function pointers from all modules
 * offering simple system calls.
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

/** Safe initialization of the global table from fragments from different contributors is
    strongly supported by the designated initializers of C99. */
#if (__STDC_VERSION__)/100 < 1999
# error Need C99 or more recent language standard
#endif

/** The actual values of the simple system call indexes are compiled by module
    sc_systemCalls.h as an enumeration. Here are the system call indexes, which are
    contributed by module lbd.\n
      Note the final comma, which is needed to safely concatenate the contributions of all
    modules.\n
      Note, each of the enumerated system calls need to have an according entry in
    #LBD_SIMPLE_SYSTEM_CALLS_TABLE_ENTRIES. */
#define LBD_SIMPLE_SYSTEM_CALLS_ENUMERATION     \
            LBD_IDX_SIMPLE_SYS_CALL_SET_LED,    \
            LBD_IDX_SIMPLE_SYS_CALL_GET_BUTTON,

/** Module sc_systemCalls.c compiles an initialized, constant table of function pointers to
    the implementations of the system calls of all the contributing modules. Here is the
    contribution of module lbd.\n
      Note, there need to be one table entry for each system call enumerated in
    #LBD_SIMPLE_SYSTEM_CALLS_ENUMERATION. */
#define LBD_SIMPLE_SYSTEM_CALLS_TABLE_ENTRIES                                               \
            [LBD_IDX_SIMPLE_SYS_CALL_SET_LED] = (int_simpleSystemCallFct_t)lbd_sc_setLED,   \
            [LBD_IDX_SIMPLE_SYS_CALL_GET_BUTTON] = (int_simpleSystemCallFct_t)lbd_sc_getButton,

/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */


#endif  /* LBD_SYSCALLINTERFACE_TABLEENTRIES_INCLUDED */
