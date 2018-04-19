#ifndef INT_INTERRUPTHANDLER_INCLUDED
#define INT_INTERRUPTHANDLER_INCLUDED
/**
 * @file int_interruptHandler.h
 * Definition of global interface of module int_interruptHandler.c
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

#include "typ_types.h"


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

/** The assembly code to switch a CPU execution context interfaces with the C code that
    implements an actual scheduler with this data structure. It contains the information
    about a suspended context, which is written on suspend and read on later resume of the
    same context.
      @remark The assembler doesn't know or use this typedef. Instead it assumes all
    contained values to be 32 Bit words at aligned, subsequent 4 Byte boundaries. The C
    code using this type definition needs to contain a _Static_assert to check this
    condition at compile time. */
typedef struct int_contextSaveDesc_t
{
    /** The value of the stack pointer at suspension of the context.\n
          This field is written by the assembly code at suspension of a context. It must
        not be touched by the scheduler code. */
    uint32_t * volatile pStack;

    /** A context switch is possible from two types of interrupts. Both require different
        handling and therefore the kind of interrupt needs to be saved at suspension time
        for later context resume.\n
          The supported interrupts are asynchronous External Interrupt (IVOR #4) and
        synchronous system call software interrupt (IVOR #8). For External Interrupts this
        field will contain -1 and for system calls it'll contain the system call index,
        which is greater or equal to zero.\n
          This field is written by the assembly code at suspension of a context. It must
        not be touched by the scheduler code. */
    volatile int32_t idxSysCall;

} int_contextSaveDesc_t;


/** The asynchronous interrupt, which does not interact with the scheduler and which cannot
    provoke or command a context switch needs to be implemented by a function of this
    type.
      @remark Handlers of this kind are installed by the application code using function
    ihw_installINTCInterruptHandler(). */
typedef void (*int_ivor4SimpleIsr_t)(void);

/** The return value of an interrupt handler, which interacts with the scheduler and which
    can demand a context switch by means of its Boolean function return value. */
typedef struct int_cmdContextSwitch_t
{
    /** The return value of a system call. This field needs to be set by the interrupt or
        system call handler only if the interrupt returns to a system call. This can be the
        immediate, context switch free return from an system call interrupt or the resume
        of a context, which had formerly been suspended by a system call. */
    uint32_t retValSysCall;

    /** A context switch is demanded if the service handler for either a kernel relevant
        External Interrupt or for a system call returns \a true. Now, the pointer
        references the location in memory where the context save information of the
        suspended context has to be saved for later resume of the context. Otherwise the
        value doesn't care. */
    int_contextSaveDesc_t *pSuspendedContextSaveDesc;

    /** A context switch is demanded if the service handler for either a kernel relevant
        External Interrupt or for a system call returns \a true. Now, this pointer
        references the location in memory where the context save information of the resumed
        context is found. */
    const int_contextSaveDesc_t *pResumedContextSaveDesc;

} int_cmdContextSwitch_t;



/** The asynchronous External Interrupt, which interacts with the scheduler and which
    decides whether or not a context switch results from its execution needs to be
    implemented by a function of this type.
      @return
    At return, the interrupt handler can decide whether to return to the interrupted
    context (\a false) or whether to suspend this context and to resume another one (\a
    true).
      @remark Handlers of this kind are installed for External Interrupts by the
    application or scheduler code using function ihw_installINTCInterruptHandler(). */
typedef bool (*int_ivor4KernelIsr_t)(int_cmdContextSwitch_t *pCmdContextSwitch);


/** The API ihw_installINTCInterruptHandler() to install an ISR for External Interrupts
    accepts both types of handlers, with and without the option to switch the context on
    return. Here's a union to combine both function pointer types. */
/// @todo Consider using anonymous unions as type. This is likely more user friendly and intuitive. The only location where this is used should be the register function in module IHW
typedef union int_externalInterruptHandler_t
{
    /** Use this field to store a simple ISR, which can't interact with the operating
        system. */
    int_ivor4SimpleIsr_t simpleIsr;

    /** Use this field to store an ISR, which can interact with the operating system and
        which can demand context switches at return. */
    int_ivor4KernelIsr_t kernelIsr;

} int_externalInterruptHandler_t;


/** Each system call needs to be implemented by a function of this type.
      @remark
    The C signature is formally not correct. The assembly code only
    supports function arguments in CPU registers, which limits the total number to eight.
    The ... stands for 0..7 arguments of up to 32 Bit. If a system call function has more
    arguments or if it are 64 Bit arguments then the assembly code will not propagate all
    arguments properly to the system call function and the behavior will be undefined! */
typedef bool (*int_systemCallFct_t)( int_cmdContextSwitch_t *pCmdContextSwitch
                                   , ...
                                   );

/** This macro supports safe implementation of client code of the IVOR handlers. It
    tests the binary build-up of the interface with the assembly code. The assembler does
    not double-check the data types and code maintenance is not safely possible without
    these compile time tests.\n
      @todo You need to put an instance of this macro somewhere in your compiled C code. It
    is a pure compile time test and does not conume any CPU time. */
#define INT_STATIC_ASSERT_INTERFACE_CONSISTENCY_C2AS                                        \
    _Static_assert( sizeof(int_cmdContextSwitch_t) == 12                                    \
                    &&  offsetof(int_cmdContextSwitch_t, retValSysCall) == 0                \
                    &&  offsetof(int_cmdContextSwitch_t, pSuspendedContextSaveDesc) == 4    \
                    &&  offsetof(int_cmdContextSwitch_t, pResumedContextSaveDesc) == 8      \
                    &&  sizeof(int_contextSaveDesc_t) == 8                                  \
                    &&  offsetof(int_contextSaveDesc_t, pStack) == 0                        \
                    &&  sizeof(((int_contextSaveDesc_t*)NULL)->pStack) == sizeof(uint32_t)  \
                    &&  offsetof(int_contextSaveDesc_t, idxSysCall) == 4                    \
                  , "Interface between C and assembler code inconsistently defined"         \
                  );


/*
 * Global data declarations
 */


/** The behavior of the system calls are implemented in the C implementation of the
    scheduler/kernel. The assembly code implements the call of these functions as a
    software interrupt. The interface between assembler and C code is a table of function
    pointers, which is declared by and extern to the assembler code. The actual scheduler
    implementation in C will decide, which and how many system calls are needed and define
    and fill the table accordingly.\n
      Note, the entries in the table are normal, proper C functions; no considerations
    about specific calling conventions or according type decorations need to be made.\n
      Note, we place the table into the IVOR ROM, which enables a single instruction load
    of the function pointer. */
extern const SECTION(.rodata.ivor) int_systemCallFct_t int_systemCallHandlerAry[];

#ifdef DEBUG
/** The number of entries in the table of system calls. Only required for boundary check in
    DEBUG compilation.\n
      The variable is read by the assembler code but needs to be defined in the scheduler
    implementation. */
extern const uint32_t int_noSystemCalls;
#endif


/*
 * Global prototypes
 */

/** System call; entry point into operating system function for user code.
      @return
    The return value depends on the system call.
      @param idxSysCall
    Each system call is identified by an index. The index is a non negative integer.\n
      The further function arguments depend on the system call.
      @remark
    The C signature for system calls is formally not correct. The assembly code only
    supports function arguments in CPU registers, which limits the total number to eight.
    The ... stands for 0..7 arguments of up to 32 Bit. If a system call function has more
    arguments or if it are 64 Bit arguments then the assembly code will not propagate all
    arguments properly to the system call function and the behavior will be undefined! */
uint32_t int_systemCall(int32_t idxSysCall, ...);


#endif  /* INT_INTERRUPTHANDLER_INCLUDED */
