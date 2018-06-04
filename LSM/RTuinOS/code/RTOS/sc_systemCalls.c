/**
 * @file sc_systemCalls.c
 * The list of system calls.
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
 * Local functions
 */

/*
 * Include files
 */

#include <assert.h>

#include "typ_types.h"
#include "int_interruptHandler.h"
#include "ccx_createContext.h"
#include "rtos.h"
#include "rtos_systemCalls.h"
#include "sc_systemCalls.h"


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
 
/** The behavior of the kernel system calls are implemented in the C implementation of the
    scheduler/kernel. The assembly code implements the call of these functions as a
    software interrupt. The interface between assembler and C code is a table of function
    pointers, which is declared by and extern to the assembler code. The actual scheduler
    implementation in C will decide, which and how many system calls are needed and define
    and fill the table accordingly.\n
      Note, the entries in the table are normal, proper C functions; no considerations
    about specific calling conventions or according type decorations need to be made.\n
      Note, there are two tables, one for kernel relevant system calls, which can yield a
    context switch and one for kernel unrelated system calls, which just require code
    execution in supervisor mode. This is the table for kernel relevant system calls.\n
      Note the one's complement in the relationship between system call indexes and array
    indexes: kernel relevant system calls use the negative index range.\n
      Note, we place the table into the IVOR ROM, which enables a single instruction load
    of the function pointer. */
const SECTION(.rodata.ivor) int_systemCallFct_t int_systemCallHandlerAry[SC_NO_SYSTEM_CALLS] =
    { [~SC_IDX_SYS_CALL_WAIT_FOR_EVENT] = (int_systemCallFct_t)rtos_sc_waitForEvent
    , [~SC_IDX_SYS_CALL_SEND_EVENT] = (int_systemCallFct_t)rtos_sc_sendEvent
    };

/** The table of C functions, which implement the simple system calls (i.e. the kernel
    unrelated system calls). */
const SECTION(.rodata.ivor) int_simpleSystemCallFct_t
    int_simpleSystemCallHandlerAry[SC_NO_SIMPLE_SYSTEM_CALLS] =
    { 	// (no simple system calls in use so far)
    };


#ifdef DEBUG
/** The number of entries in the table of system calls. Only required for boundary check in
    DEBUG compilation.\n
      The variable is read by the assembler code but needs to be defined in the scheduler
    implementation. */
const uint32_t int_noSystemCalls = sizeOfAry(int_systemCallHandlerAry);

/** The number of entries in the table of simple (i.e. kernel unrelated) system calls. Only
    required for boundary check in DEBUG compilation.\n
      The variable is read by the assembler code but needs to be defined in the scheduler
    implementation. */
const uint32_t int_noSimpleSystemCalls = sizeOfAry(int_simpleSystemCallHandlerAry);
#endif
 
 
/*
 * Function implementation
 */

