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
#include "int_interruptHandler.config.h"


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


/** This macro supports safe implementation of client code of the IVOR handlers. It
    tests the binary build-up of the interface with the assembly code. The assembler does
    not double-check the data types and code maintenance is not safely possible without
    these compile time tests.\n
      @todo You need to put an instance of this macro somewhere in your compiled C code. It
    is a pure compile time test and does not consume any CPU time. */
#if INT_USE_SHARED_STACKS == 1
#define INT_STATIC_ASSERT_INTERFACE_CONSISTENCY_C2AS                                        \
    _Static_assert( sizeof(int_cmdContextSwitch_t) == 12                                    \
                    &&  offsetof(int_cmdContextSwitch_t, signalToResumedContext) == 0       \
                    &&  offsetof(int_cmdContextSwitch_t, pSuspendedContextSaveDesc) == 4    \
                    &&  offsetof(int_cmdContextSwitch_t, pResumedContextSaveDesc) == 8      \
                    &&  sizeof(int_contextSaveDesc_t) == 24                                 \
                    &&  offsetof(int_contextSaveDesc_t, idxSysCall) == 0                    \
                    &&  sizeof(((int_contextSaveDesc_t*)NULL)->idxSysCall)                  \
                        == sizeof(uint32_t)                                                 \
                    &&  offsetof(int_contextSaveDesc_t, ppStack) == 4                       \
                    &&  sizeof(((int_contextSaveDesc_t*)NULL)->ppStack) == sizeof(uint32_t) \
                    &&  offsetof(int_contextSaveDesc_t, pStack) == 8                        \
                    &&  sizeof(((int_contextSaveDesc_t*)NULL)->pStack) == sizeof(uint32_t)  \
                    &&  offsetof(int_contextSaveDesc_t, pStackOnEntry) == 12                \
                    &&  sizeof(((int_contextSaveDesc_t*)NULL)->pStackOnEntry)               \
                        == sizeof(uint32_t)                                                 \
                    &&  offsetof(int_contextSaveDesc_t, fctEntryIntoContext) == 16       \
                    &&  sizeof(((int_contextSaveDesc_t*)NULL)->fctEntryIntoContext)      \
                        == sizeof(uint32_t)                                                 \
                    &&  offsetof(int_contextSaveDesc_t, privilegedMode) == 20               \
                    &&  sizeof(((int_contextSaveDesc_t*)NULL)->privilegedMode)              \
                        == sizeof(uint8_t)                                                  \
                  , "Interface between C and assembler code inconsistently defined"         \
                  );
#else
#define INT_STATIC_ASSERT_INTERFACE_CONSISTENCY_C2AS                                        \
    _Static_assert( sizeof(int_cmdContextSwitch_t) == 12                                    \
                    &&  offsetof(int_cmdContextSwitch_t, signalToResumedContext) == 0       \
                    &&  offsetof(int_cmdContextSwitch_t, pSuspendedContextSaveDesc) == 4    \
                    &&  offsetof(int_cmdContextSwitch_t, pResumedContextSaveDesc) == 8      \
                    &&  sizeof(int_contextSaveDesc_t) == 16                                 \
                    &&  offsetof(int_contextSaveDesc_t, idxSysCall) == 0                    \
                    &&  sizeof(((int_contextSaveDesc_t*)NULL)->idxSysCall)                  \
                        == sizeof(uint32_t)                                                 \
                    &&  offsetof(int_contextSaveDesc_t, pStack) == 4                        \
                    &&  sizeof(((int_contextSaveDesc_t*)NULL)->pStack) == sizeof(uint32_t)  \
                    &&  offsetof(int_contextSaveDesc_t, fctEntryIntoContext) == 8        \
                    &&  sizeof(((int_contextSaveDesc_t*)NULL)->fctEntryIntoContext)      \
                        == sizeof(uint32_t)                                                 \
                    &&  offsetof(int_contextSaveDesc_t, privilegedMode) == 12               \
                    &&  sizeof(((int_contextSaveDesc_t*)NULL)->privilegedMode)              \
                        == sizeof(uint8_t)                                                  \
                  , "Interface between C and assembler code inconsistently defined"         \
                  );
#endif


/*
 * Global type definitions
 */

/** A new context is started by entering a C function. It receives a single 32 Bit value as
    function argument and may return a 32 Bit value on exit.\n
      The function argument is the value of \a signalToResumedContext (see struct \a
    int_cmdContextSwitch_t), when commanding the switch to a new context the very first
    time.\n
      The function return value is passed as only function argument to the on-exit-guard
    function int_fctOnContextEnd(). */
typedef uint32_t (*int_fctEntryIntoContext_t)(uint32_t);


/** The bits of the return value of kernel interrupts. By return value the interrupt
    handler controls, whether or not to switch to another context. Whether to newly create
    the aimed context or to terminate the left context.\n
      An interrupt handler can return a combination of the enumerated values. (Combination:
    sum or binary OR.) The receiving assembly code mainly looks at zero or not, no context
    switch or context switch, respectively.\n
      \a int_rcIsr_doNotSwitchContext, i.e. zero, must not be combined with any of the
    other bits.\n
      \a int_rcIsr_switchContext, which already makes the value non zero, can be combined
    with any possible combination out of \a int_rcIsr_terminateLeftContext and \a
    int_rcIsr_createEnteredContext.\n
      If \a int_rcIsr_terminateLeftContext is part of the returned value then the assembly
    code will not store the current stack pointer value in the context save area of the
    left context but the very value it had had at the time of on-the-fly creation of this
    context. The assumption is that this is the final suspension of the context and that
    it'll never be resumed again. By restoring the stack pointer it is ensured that other
    contexts, which share the stack with the terminating context can safely be resumed
    again. They see the stack pointer value they expect. Therefore, the use of this flag is
    restricted to applications, which make use of stack sharing (see
    #INT_USE_SHARED_STACKS).\n
      Note, the context save information of a context, which flag \a
    int_rcIsr_terminateLeftContext had been applied to, must never be use again.\n
      Note, as a rule of thumb, the flag \a int_rcIsr_terminateLeftContext can safely be
    applied to contexts only, which had been created earlier by using of the other flag \a
    int_rcIsr_createEnteredContext.\n
      If \a int_rcIsr_createEnteredContext is part of the returned value then the assembly
    code will not simply resume the entered context from its context save information.
    Instead it starts a new context. The C function is entered, which is specified as field
    \a fctEntryIntoContext in the context save descriptor of the entered task. The initial
    stack pointer value is taken from the same object and this can involve stack sharing
    with other (currently suspended or not yet created) contexts. If stack sharing is
    enabled then the initial stack pointer value is stored in the context save descriptor
    to enable and prepare a later use of flag \a int_rcIsr_terminateLeftContext. */
typedef enum int_retCodeKernelIsr_t
{
    /** The ISR returns without context switch. The preempted context (External Interrupt)
        or calling context (system call) is continued after return from the ISR. */
    int_rcIsr_doNotSwitchContext = 0u,
    
    /** The ISR demands a context switch on return. The aimed context is an already created
        but currently resume context. */
    int_rcIsr_switchContext = 0x2u,
    
    /** The ISR demands a context switch on return. The aimed context is a new, on the fly
        created context. */
    int_rcIsr_createEnteredContext = 0x80000000u,
    
#if INT_USE_SHARED_STACKS == 1
    /** The ISR demands a context switch on return. The suspended context is terminated.
        (The aimed context is either a suspended or a new, on the fly created context. This
        is controlled by \a int_rcIsr_switchContext or \a int_rcIsr_createNewContext.) */
    int_rcIsr_terminateLeftContext = 0x1u,
#endif
} int_retCodeKernelIsr_t;


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

#if INT_USE_SHARED_STACKS == 1
    /** The value of the stack pointer at suspension of the context is stored in or
        retrieved from the memory location this pointer points to. Normally, it points to
        the other field \a pStack of the same object but if several contexts share a stack
        then all of them let their \a ppStack point to one and the same \a pStack.\n
          This field is written by the assembly code at suspension of a context. It must
        not be touched by the scheduler code. */
    void * volatile * ppStack;
#endif

    /** The value of the stack pointer at suspension of the context.\n
          This field is written by the assembly code at suspension of a context. It must
        not be touched by the scheduler code (besides context initialization). */
    void * volatile pStack;

#if INT_USE_SHARED_STACKS == 1
    /** With stack sharing, we need to restore the initial stack pointer value on context
        termination so that another context which continues uing the same stack will see
        its last value again on resume. This variable is written once on context creation
        and read once when terminating that context again. */
    void * volatile pStackOnEntry;
#endif

    /** If the switch to an on the fly created, new context is demanded on exit from an
        interrupt handler then the entry point into that function is this C function
        pointer. */
    int_fctEntryIntoContext_t fctEntryIntoContext;

    /** The on-the-fly started context can be run either in user mode or in privileged
        mode. Assign \a true to this field for the latter.\n
          Note, the user mode should be preferred but can generally be used only if the
        whole system design supports this. All system level functions (in particular the
        I/O drivers) need to have an API, which is based on system calls. Even the most
        simple functions which access I/O registers or protected CPU registers, like
        ihw_suspendAllInterrupts() and ihw_resumeAllInterrupts(), are not permitted in user
        mode. */
    bool privilegedMode;

} int_contextSaveDesc_t;


/** The return value of an interrupt handler, which interacts with the scheduler and which
    can demand a context switch by means of its Boolean function return value. */
typedef struct int_cmdContextSwitch_t
{
    /** A value can be signalled to the continued context if it is in the state to receive
        such a signal:\n
          If the context to switch to is a new conext, which is started or resumed the very
        first time then the signalled value is the value of the function argument of the
        context entry function.\n
          If the resumed context had suspended in a system call then the signalled value is
        the return code from that system call. The same holds if we return without context
        switch to a context, which had done a system call.\n
          If the resumed context had been preempted by an External Interrupt then it is
        continued where it had been preempted and \a signalToResumedContext is ignored. */
    uint32_t signalToResumedContext;

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



/** The asynchronous interrupt, which does not interact with the scheduler and which cannot
    provoke or command a context switch needs to be implemented by a function of this
    type.
      @remark Handlers of this kind are installed by the application code using function
    ihw_installINTCInterruptHandler(). */
typedef void (*int_ivor4SimpleIsr_t)(void);


/** The asynchronous External Interrupt, which interacts with the scheduler and which
    decides whether or not a context switch results from its execution needs to be
    implemented by a function of this type.
      @return
    At return, the interrupt handler can decide whether to return to the interrupted
    context (\a false) or whether to suspend this context and to resume another one (\a
    true).
      @remark Handlers of this kind are installed for External Interrupts by the
    application or scheduler code using function ihw_installINTCInterruptHandler(). */
typedef uint32_t (*int_ivor4KernelIsr_t)(int_cmdContextSwitch_t *pCmdContextSwitch);


/** The API ihw_installINTCInterruptHandler() to install an ISR for External Interrupts
    accepts both types of handlers, with and without the option to switch the context on
    return. Here's a union to combine both function pointer types for the prototype of that
    function. */
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
    The C signature is formally not correct. The assembly code only supports function
    arguments in CPU registers, which limits the total number to eight. The ... stands for
    0..7 arguments of up to 32 Bit. If a system call function has more arguments or if it
    are 64 Bit arguments then the assembly code will not propagate all arguments properly
    to the system call function and the behavior will be undefined! */
typedef int_retCodeKernelIsr_t (*int_systemCallFct_t)
                                    ( int_cmdContextSwitch_t *pCmdContextSwitch
                                    , ...
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
 * Global static inline functions
 */


/*
 * Global prototypes
 */

/**
 * This is the common guard function of the context entry functions: When a function, which
 * had been been specified as context entry function is left with return then program flow
 * goes into this guard function.
 *   @param retValOfContext
 * The guard function receives the return value of the left context entry function as
 * parameter.
 *   @remark
 * Note, the guard function has no calling parent function. Any attempt to return from this
 * function will surely lead to a crash. The normal use case is to do a system call in the
 * guard function's implementation, which notifies the situation about the terminating
 * context to the scheduler. On return, the system call implementation will surely not use
 * the option \a int_rcIsr_doNotSwitchContext and control will never return to the guard.
 */
_Noreturn void int_fctOnContextEnd(uint32_t retValOfContext);


/**
 * System call; entry point into operating system function for user code.
 *   @return
 * The return value depends on the system call.
 *   @param idxSysCall
 * Each system call is identified by an index. The index is a non negative integer.\n
 *   The further function arguments depend on the system call.
 *   @remark
 * The C signature for system calls is formally not correct. The assembly code only
 * supports function arguments in CPU registers, which limits the total number to eight.
 * The ... stands for 0..7 arguments of up to 32 Bit. If a system call function has more
 * arguments or if it are 64 Bit arguments then the assembly code will not propagate all
 * arguments properly to the system call function and the behavior will be undefined!
 */
uint32_t int_systemCall(int32_t idxSysCall, ...);


#endif  /* INT_INTERRUPTHANDLER_INCLUDED */
