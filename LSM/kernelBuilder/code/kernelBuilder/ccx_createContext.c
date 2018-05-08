/**
 * @file ccx_createContext.c
 * Support functions for using kernelBuilder: A new execution context is created. A
 * scheduler implementation can use the offered functions to create new tasks.\n
 *   Note, there's no concept of context deletion. The entire framework doesn't deal with
 * memory allocation. For example, context creation leaves it entirely to the client code
 * to ensure the availability of RAM space for the stack. It only initializes that memory
 * such that a runnable context emerges. Accordingly, there's nothing to do for context
 * deletion; from the perspective of kernelBuilder deletion of a context just means that
 * that context is never again specified for resume. Whether the client code puts the data
 * structure into a pool for later reuse or whether it uses heap operations to release the
 * memory behind for other purposes is out of scope of kernelBuilder.
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
 *   ccx_createContext
 *   ccx_createContextShareStack
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

#include "ihw_initMcuCoreHW.h"
#include "int_defStackFrame.h"
#include "int_interruptHandler.h"
#include "sc_systemCalls.h"
#include "ccx_startContext.h"
#include "ccx_createContext.h"


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
 * Create a new execution context. A scheduler implementation can use this function to
 * create a new task or to re-initialize an existing task with a new task function (support
 * of task pooling to avoid dynamic memory allocation).
 *   @param pContextSaveDesc
 * The caller provides the location of the context save descriptor for the newly created
 * context. This context save descriptor can then be used by a scheduler to command resume
 * and suspend of the new context.
 *   @param stackPointer
 * The initial value of the stack pointer. The client code will allocate sufficient stack
 * memory. This pointer will usually point at the first address beyond the allocated memory
 * chunk; our stacks grow downward to lower addresses.\n
 *   Note, each preemption of a context by an asynchronous External Interrupt requires
 * about 170 Byte of stack space. Another about 100 Byte need to be reserved for the system
 * call interrupt. If your application makes use of all interrupt priorities then you need
 * to have 15*170+100 Byte as a minimum of stack space for safe operation, not yet counted
 * the stack consumption of your application itself.\n
 *   Note, this lower bounds even holds if you apply the implementation of the priority
 * ceiling protocol from the startup code to mutually exclude sets of interrupts from
 * preempting one another, see https://community.nxp.com/message/993795 for details.\n
 *   The passed address needs to be 8 Byte aligned; this is double-checked by assertion.
 *   @param fctEntryIntoContext
 * A C function, which is the entry point into the new execution context.
 *   @param privilegedMode
 * The newly created context can be run either in user mode or in privileged mode.\n
 *   This argument doesn't care if \a fctEntryIntoContext is NULL: For contexts, which
 * are started on-the-fly, the mode is specified at run-time, when starting the context.\n
 *   Note, the user mode should be preferred but can generally be used only if the whole
 * system design supports this. All system level functions (in particular the I/O drivers)
 * need to have an API, which is based on system calls. Even the most simple functions
 * which access I/O registers or protected CPU registers, like ihw_suspendAllInterrupts()
 * and ihw_resumeAllInterrupts(), are not permitted in user mode.
 */
void ccx_createContext( int_contextSaveDesc_t *pContextSaveDesc
                      , void *stackPointer
                      , int_fctEntryIntoContext_t fctEntryIntoContext
                      , bool privilegedMode
                      )
{
    /* The alignment matters. EABI requires 8 Byte alignment. */
    assert(stackPointer != NULL  &&  ((uint32_t)stackPointer & 0x7) == 0
           &&  fctEntryIntoContext != NULL
          );

    uint32_t *sp = (uint32_t*)stackPointer;

#define IDX(OFFSET)                                                                         \
    ({ _Static_assert(((OFFSET) & 0x3) == 0, "Bad stack word offset");                  \
       (OFFSET)/sizeof(uint32_t);                                                       \
    })

    /* The topmost word is not used. We require it for the eight byte alignment rule. */
    * --sp = 0xffffffff;

    /* The next word in the stack frame of the hypothetic parent function of our
       assembler written start function is reserved for the storage of the LR for its
       children functions. The value is filled below. */
    -- sp;

    /* The next word is were the parent function of our assembler written start
       function would have stored its stack pointer value on function entry. We don't
       have such a parent and write a dummy value. */
    * --sp = 0xffffffff;

    /* Now we see the stack pointer value as it were on entry in our assembler written
       start function. The value is needed for proper build up of its stack frame, see
       below. */
    uint32_t * const spOnEntryIntoStartContext = sp;

    /* The stack frame of our assembler written context start function is not created
       by that function itself but prepared here. This gives us the chance to provide
       it with the needed information. */
    _Static_assert((S_StCtxt_StFr & 0x7) == 0, "Bad stack frame size");
    sp -= IDX(S_StCtxt_StFr);
    *sp = (uint32_t)spOnEntryIntoStartContext;

    /* The word above the stack frame is were the prologue of any EABI function would
       place the return address. We put the address of the guard function in order to
       jump there if the entry function of the new context is left. */
    sp[IDX(4+S_StCtxt_StFr)] = (uint32_t)&int_fctOnContextEnd;

    /* The stack frame of our assembler written context start function contains the
       address of the entry function of the new context. This value is read and used
       for a branch by our start function. */
    sp[IDX(O_StCtxt_CTXT_ENTRY)] = (uint32_t)fctEntryIntoContext;

    /* Down here, the stack frame is prepared in the stack that contains the CPU
       context as it should be on entry into the start function. To facilitate
       maintenance of the code we implement the C operations to fill the stack frame
       similar to the assembly code for context save and restore. */
    uint32_t * const spOnEntryIntoExecutionEntryPoint = sp;

    _Static_assert((S_SC_StFr & 0x7) == 0, "Bad stack frame size");
    sp -= IDX(S_SC_StFr);
    *sp = (uint32_t)spOnEntryIntoExecutionEntryPoint;

    /* We initialize the non volatile registers to zero. This is not really necessary
       and even inconsistent with the on-the-fly start of contexts from on return from
       a kernel interrrupt. The justification is that the on-the-fly start is an
       operation, which needs to be speed optimized while the operation here is a
       static, one time initialization, where execution speed doesn't matter.
         @todo Consider removing this code block. */
    sp[IDX(O_SC_R14)] = 0;
    sp[IDX(O_SC_R15)] = 0;
    sp[IDX(O_SC_R16)] = 0;
    sp[IDX(O_SC_R17)] = 0;
    sp[IDX(O_SC_R18)] = 0;
    sp[IDX(O_SC_R19)] = 0;
    sp[IDX(O_SC_R20)] = 0;
    sp[IDX(O_SC_R21)] = 0;
    sp[IDX(O_SC_R22)] = 0;
    sp[IDX(O_SC_R23)] = 0;
    sp[IDX(O_SC_R24)] = 0;
    sp[IDX(O_SC_R25)] = 0;
    sp[IDX(O_SC_R26)] = 0;
    sp[IDX(O_SC_R27)] = 0;
    sp[IDX(O_SC_R28)] = 0;
    sp[IDX(O_SC_R29)] = 0;
    sp[IDX(O_SC_R30)] = 0;
    sp[IDX(O_SC_R31)] = 0;

    /* Address to return to at the end of the kernel interrupt, which will start this
       context the first time. */
    sp[IDX(O_SRR0)] = (uint32_t)ccx_startContext;

    /* The machine status is set once for the context and always restored after any future
       system call or interrupt. Here, we decide once forever whether the context is
       executed in user or priviledged mode. */
    sp[IDX(O_SRR1)] = 0x00029000ul    /* MSR: External, critical and machine check
                                         interrupts enabled, SPE is not set, ... */
                      | (privilegedMode? 0x00000000: 0x00004000ul);  /* ... PR depends */

    /* The next four settings can be omitted if execution speed should matter. */
    sp[IDX(O_RET_RC)] = 0;      /* Tmp. value to return from system call, doesn't care */
    sp[IDX(O_RET_pSCSD)] = 0;   /* Tmp. pointer to context save data of suspended context,
                                   doesn't care */
    sp[IDX(O_RET_pRCSD)] = 0;   /* Tmp. pointer to context save data of resumed context,
                                   doesn't care */
#undef IDX
    
    /* The newly created context is still suspended. We save the information, which is
       required for later resume in the aimed context save descriptor. This is mainly the
       stack pointer value and the kind of continued context: Suspended by External
       Interrupt or by system call. */
    pContextSaveDesc->pStack = sp;
#if INT_USE_SHARED_STACKS == 1
    pContextSaveDesc->ppStack = &pContextSaveDesc->pStack;
    
    /* If we set pStackOnEntry then we can uses the termination functionality for this
       context and later reuse the same context save descriptor for on-the-fly started new
       contexts. */
    pContextSaveDesc->pStackOnEntry = stackPointer;
#endif
    
    /* We use the system call suspended kind of context, which is a bit more efficient. (On
       cost of giving less control on the initial CPU register values.) This is expressed
       by a non negative system call index. The actual number is meaningless. */
    pContextSaveDesc->idxSysCall = 0;

    /* Store the context entry function. Note, a context, which is created using this
       function is normally not started on the fly and this field would be unused. However,
       on-the-fly start becomes an option if we set this field nonetheless. */
    pContextSaveDesc->fctEntryIntoContext = fctEntryIntoContext;
    
    /* Store the execution mode of the context. Note, a context, which is created using this
       function is normally not started on the fly and this field would be unused. However,
       on-the-fly start becomes an option if we set this field nonetheless. */
    pContextSaveDesc->privilegedMode = privilegedMode;
    
} /* End of ccx_createContext */



#if INT_USE_SHARED_STACKS == 1
/**
 * Create a new execution context for on-the-fly start and which shares the stack with
 * another context. A scheduler implementation can use this function to create a new
 * on-the-fly task (usually a single shot task).\n
 *   Use this function instead of ccx_createContext() if you create a context, which should
 * share the stack with another, already created context.\n
 *   Note, a context, which is created with this function can only be started on the fly,
 * using flag \a int_rcIsr_createEnteredContext (see enum \a int_retCodeKernelIsr_t) on
 * return from a kernel interrupt.
 *   @param pNewContextSaveDesc
 * The caller provides the location of the context save descriptor for the newly created
 * context. This context save descriptor can then be used by a scheduler to command resume
 * and suspend of the new context. The rules for safe stack sharing need of course to be
 * obeyed.
 *   @param pPeerContextSaveDesc
 * The context save descripto of the other context, which the new one will share the stack
 * with, is provided by reference. This context
 *   - needs to be already created
 *   - can have been created with either ccx_createContext() or ccx_createContextShareStack()
 *   @param fctEntryIntoOnTheFlyStartedContext
 * A C function, which is the entry point into the new execution context. This function
 * will be called later, when an interrupt handler commands on return the start of a new
 * context (on-the-fly start of a context).
 *   @param privilegedMode
 * The newly created context can be run either in user mode or in privileged mode.\n
 *   Note, the user mode should be preferred but can generally be used only if the whole
 * system design supports this. All system level functions (in particular the I/O drivers)
 * need to have an API, which is based on system calls. Even the most simple functions
 * which access I/O registers or protected CPU registers, like ihw_suspendAllInterrupts()
 * and ihw_resumeAllInterrupts(), are not permitted in user mode.
 */
void ccx_createContextShareStack
                ( int_contextSaveDesc_t *pNewContextSaveDesc
                , const int_contextSaveDesc_t *pPeerContextSaveDesc
                , int_fctEntryIntoContext_t fctEntryIntoOnTheFlyStartedContext
                , bool privilegedMode
                )
{
    /* The new stack references the same stack pointer save variable as the other one. Both
       contexts save the stack pointer on suspend and on termination at the same memory
       location. */
    pNewContextSaveDesc->ppStack = pPeerContextSaveDesc->ppStack;
    
    /* The storage of the stack pointer value is not used. We reference the according
       variable from our peer, we share the stack with. */
    pNewContextSaveDesc->pStack = NULL;
    
    /* Store the context entry function for later on-the-fly start of the context. */
    pNewContextSaveDesc->fctEntryIntoContext = fctEntryIntoOnTheFlyStartedContext;
    
    /* The context can be started in user of in privileged mode. */
    pNewContextSaveDesc->privilegedMode = privilegedMode;
    
    /* The remaining fields don't care. They will be written on start and maybe later on
       suspend of this context. */
    pNewContextSaveDesc->pStackOnEntry = NULL;
    pNewContextSaveDesc->idxSysCall = 0;

} /* End of ccx_createContextShareStack */
#endif