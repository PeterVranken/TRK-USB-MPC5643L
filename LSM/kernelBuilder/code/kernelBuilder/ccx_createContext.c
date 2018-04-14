/**
 * @file ccx_createContext.c
 * This is the implementation of a system call, which will be useful in most environment
 * that apply kernelBuilder: A new execution context is created. A scheduler implementation
 * can use this system call to let the application code create a new task.\n
 *   The system call will prepare the new execution context and offers to either return to
 * the caller or to do a context switch to the new context and immediately run it.\n
 *   If a scheduler implementation wants to make use of this system call it just have to
 * put it somewhere in the table of system calls owned by the scheduler.\n
 *   Note, there's no concept of context deletion. The entire framework doesn't deal with
 * memory allocation. For example, context creation leaves it entirely to the client code
 * to ensure the availability of RAM space for the stack. It only initializes that memory
 * such that a runnable context emerges. Accordingly, there's nothing to do for context
 * deletion; from the perspective of kernelBuilder deletion of a context just means that
 * that context is never again specified for resume. Whether the client code puts the
 * data structure into a pool for later reuse or whether it uses heap operations to release
 * the memory behind for other purposes is out of scope of kernelBuilder.
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
 *   ccx_sc_createContext
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
 
/** Prototype of assembler function, which branches into the new context and which can make
    it on exit return to a predetermined other function to notify the
    end-of-execution-context to whom it may concern. */
extern void ccx_startContext(uint32_t contextParam);
 

/*
 * Data definitions
 */
 
 
/*
 * Function implementation
 */

/**
 * This is the implementation of a system call that creates a new execution context. It may
 * start it, too. A scheduler implementation can use this system call to create a new task
 * or to re-initialize an existing task with a new task function (support of task pooling
 * to avoid dynamic memory allocation).
 *   @return
 * \a true, if \a runImmediately is \a true, else \a false.
 *   @param pCmdContextSwitch
 * Interface with the assembly code that implements the IVOR #8 handler.\n
 *   If the system call returns to a context, which had suspended in a system call (this
 * one or another one) then it can put the value to be returned to that context in \a
 * *pCmdContextSwitch.\n
 *   If the function returns \a true to request a context switch then it'll write
 * references to the descriptors of the suspended and resumed contexts into the same data
 * structure.\n
 *   This system requests a context switch if and only if \a runImmediately is \a true.
 *   @param pNewContextDesc
 * The specification of the new execution context to be created.
 *   @param onExitGuard
 * The address of a function, which is invoked if the new execution context ends, i.e. when
 * the function (*pNewContextDesc->executionEntryPoint)() is left. The return value of
 * (*pNewContextDesc->executionEntryPoint)() is function argument of (*onExitGuard)().
 *   @param runImmediately
 * If \a true the new context is not only created in suspended state but the calling
 * context is suspended in favor of the new one.
 *   @param initialData
 *   If the new context is immediately started then this value is passed to the entry
 * function (*pNewContextDesc->executionEntryPoint)() of that context as only argument.\n
 *   This argument doesn't care if \a runImmediately is false. If \a runImmediately is
 * false then the argument of the entry function into the new context will be provided by
 * the system call that awakes the new context the very first time.
 *   @param pNewContextSaveDesc
 * The caller provides the location of the context save descriptor for the newly created
 * context. This context save descriptor can then be used by a scheduler to command resume
 * and suspend of the new context.
 *   @param pThisContextSaveDesc
 * The caller provides the location of the context save descriptor for the calling
 * context.\n
 *   This argument doesn't care if \a runImmediately is false. This, the invoking context is
 * not disrupted and there's no need to save it.
 *   @note
 * This is the implementation of a system call. It must only be called from an interrupt
 * context. Never invoke this function directly.
 */
uint32_t ccx_sc_createContext( int_cmdContextSwitch_t *pCmdContextSwitch
                             , const ccx_contextDesc_t *pNewContextDesc
                             , void (*onExitGuard)(uint32_t)
                             , bool runImmediately
                             , uint32_t initialData
                             , int_contextSaveDesc_t *pNewContextSaveDesc
                             , int_contextSaveDesc_t *pThisContextSaveDesc
                             )
{
#define IDX(OFFSET)                                                                         \
        ({ _Static_assert(((OFFSET) & 0x3) == 0, "Bad stack word offset");                  \
           (OFFSET)/sizeof(uint32_t);                                                       \
        })

    /* The alignment matters. EABI requires 8 Byte alignment. */
    assert(((uint32_t)pNewContextDesc->stackPointer & 0x7) == 0);

    uint32_t *sp = (uint32_t*)pNewContextDesc->stackPointer;

    /* The topmost word is not used. We require it for the eight byte alignment rule. */
    * --sp = 0xffffffff;

    /* The next word in the stack frame of the hypothetic parent function of our assembler
       written start function is reserved for the storage of the LR for its children
       functions. The value is filled below. */
    -- sp;
    
    /* The next word is were the parent function of our assembler written start function
       would have stored its stack pointer value on function entry. We don't have such a
       parent and write a dummy value. */
    * --sp = 0xffffffff;
    
    /* Now we see the stack pointer value as it were on entry in our assembler written
       start function. The value is needed for proper build up of its stacjk frame, see
       below. */
    uint32_t * const spOnEntryIntoStartContext = sp;
    
    /* The stack frame of our assembler written context start function is not created by
       that function itself but prepared here. This gives us the chance to provide it with
       the needed information. */
    _Static_assert((S_StCtxt_StFr & 0x7) == 0, "Bad stack frame size");
    sp -= IDX(S_StCtxt_StFr);
    *sp = (uint32_t)spOnEntryIntoStartContext;
    
    /* The word above the stack frame is were the prologue of any EABI function would place
       the return address. We put the address of the guard function in order to jump there
       if the entry function of the new context is left. */
    sp[IDX(4+S_StCtxt_StFr)] = (uint32_t)onExitGuard;

    /* The stack frame of our assembler written context start function contains the address
       of the entry function of the new context. This value is read and used for a branch
       by our start function. */
    sp[IDX(O_StCtxt_CTXT_ENTRY)] = (uint32_t)pNewContextDesc->executionEntryPoint;

    /* Down here, the stack frame is prepared in the stack that contains the CPU context as
       it should be on entry into the start function. To facilitate maintenance of the code
       we implement the C operations to fill the stack frame similar to the assembly code
       for context save and restore. */
    uint32_t * const spOnEntryIntoExecutionEntryPoint = sp;

    _Static_assert((S_SC_StFr & 0x7) == 0, "Bad stack frame size");
    sp -= IDX(S_SC_StFr);
    *sp = (uint32_t)spOnEntryIntoExecutionEntryPoint;
    
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
    
    /* Address to return to at end of interrupt */
    sp[IDX(O_SRR0)] = (uint32_t)ccx_startContext;
    
    /* The machine status is set once for the context and always restored after any future
       system call or interrupt. Here, we decide once forever whether the context is
       executed in user or priviledged mode. */
    sp[IDX(O_SRR1)] = 0x00029000ul    /* MSR: External, critical and machine check
                                         interrupts enabled, SPE is not set, ... */
                      | (pNewContextDesc->privilegedMode  /* ... PR depends */
                         ? 0x00000000
                         : 0x00004000ul
                        );

    sp[IDX(O_RET_RC)] = 0;      /* Tmp. value to return from system call, doesn't care */
    sp[IDX(O_RET_pSCSD)] = 0;   /* Tmp. pointer to context save data of suspended context,
                                   doesn't care */
    sp[IDX(O_RET_pRCSD)] = 0;   /* Tmp. pointer to context save data of resumed context,
                                   doesn't care */
#undef IDX
    
    /* The newly created context is still suspended. We save the information, which is
       required for later resume in the aimed context save descriptor.\n
         Note, it depends on the integrating client code, which system call number the
       operation gets. The entry has to be made by the calling code. We can only double
       check that it is not a negative number, which is reserved to interrupts and which
       would make the code crash. (Actually, the number doesn't really matter if it is only
       not negative.) */
    pNewContextSaveDesc->pStack = sp;
    pNewContextSaveDesc->idxSysCall = SC_IDX_SYS_CALL_CREATE_NEW_CONTEXT;
    assert(pNewContextSaveDesc->idxSysCall >= 0
           &&  pNewContextSaveDesc->idxSysCall < (signed int)int_noSystemCalls
          );

    if(runImmediately)
    {
        /* Provide our function argument to the C function, which defines the entry into
           the new context. */
        pCmdContextSwitch->retValSysCall = initialData;
        pCmdContextSwitch->pSuspendedContextSaveDesc = pThisContextSaveDesc;
        pCmdContextSwitch->pResumedContextSaveDesc = pNewContextSaveDesc;
        return true;
    }
    else
    {
        /* This system call has no result for the invoking function. We simply return
           there. */
        return false;
    }
} /* End of ccx_sc_createContext */



