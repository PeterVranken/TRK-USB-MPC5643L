/**
 * @file ccx_startContext.S
 * Creating a new execution context is supported by a small assembler function to enter the
 * new context. The entry point of the new context is considered a normal C function with
 * stack frame build up and the EABI way to receive arguments and to return a result. Such
 * code makes it difficult to determine the return behavior from that function: The return
 * address is defined by register LR on entry but not normally part of the saved context of
 * a system call and therefore arbitrarily set when resuming the new context the first
 * time.\n
 *   Here is an assembler function, which implements the epilogue of an EABI function but
 * doesn't have the prologue. We use it as agrred starting point of the new execution
 * context. The missing prologue is replaced by the code in C function
 * ccx_sc_createContext(), which prepares the stack frame as if a normal EABI prologue had
 * been executed. This way we can set the LR (and any number of local variables) from the C
 * function ccx_sc_createContext(). The LR can be set to determine to where the code
 * branches when the entry function of the new context is left and the local variables
 * could be applied to state additional arguments for the new context entry function or to
 * specify what should be returned on exiting from it. (Note, the current implementation
 * doesn't make use of the local variables.)\n
 *   Two major use cases exist for a well defined return behavior from a new context. The
 * first one is the installation of a guard function, which would detect and report the
 * exit from a context in environments, where this is not permitted.\n
 *   The second use case is the task termination. The function to return to would contain a
 * system call into the scheduler, which notifies the no longer existing context to it. The
 * scheduler can decide to delete the context or to return it to a pool, whatever.
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

#ifdef __VLE__
# error This file is intended for compilation for Book E instruction set only
#endif

/*
 * Include files
 */

#include "int_defStackFrame.h"
#include "ccx_startContext.h"


/*
 * Defines
 */



/*
 * External function declarations
 */


/*
 * Data declarations
 */


/*
 * Function implementation
 */



/**
 *   @func ccx_startContext
 * Start a new context. On return from the context entry function branch to a
 * pre-determined guard function.
 *   @param contextParam
 * On entry, r3 contains the first and only function argument of the entry function into
 * the started context.
 */
    .section .text
    .globl  ccx_startContext
    .type   ccx_startContext, @function
    .align  4
#define SIZE_OF_STACKFRAME  S_StCtxt_StFr
#define O_CTXT_ENTRY        O_StCtxt_CTXT_ENTRY

ccx_startContext:

    /* The speciality of this function is that it doesn't have any code for stack frame
       creation. Instead, it expects the stack frame already existing. This way, the C code
       to prepare the initial stack contents can determine all data needed by the function
       - particularly the address of the context entry function and the guard function. */

    /* Load address of context entry function and branch there with link to execute it. */
    lwz     r0, O_CTXT_ENTRY(sp)
    mtlr    r0
    blrl
    
    /* On return from the context entry function, we can branch to the guard function by a
       normal EABI function epilogue; the stack had been prepared by the C code
       accordingly. If the context entry function returns a value in r3 then it becomes the
       function argument of the guard function. */
    lwz     r0, (SIZE_OF_STACKFRAME+4)(sp)
    mtlr    r0
    la      sp, SIZE_OF_STACKFRAME(sp)
    blr

#undef SIZE_OF_STACKFRAME
#undef O_CTXT_ENTRY
    .size   ccx_startContext, .-ccx_startContext
/* End of ccx_startContext */
