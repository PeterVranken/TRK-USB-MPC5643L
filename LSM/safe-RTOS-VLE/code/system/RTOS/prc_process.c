/**
 * @file prc_process.c
 * @todo Document
 *
 * Copyright (C) 2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   prc_initINTCInterruptController
 *   prc_installINTCInterruptHandler
 *   prc_dummyINTCInterruptHandler
 *   prc_scSmplHdlr_suspendProcess
 * Local functions
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "MPC5643L.h"

#include "typ_types.h"
#include "prc_process.h"
#include "prc_process_defSysCalls.h"
#include "ihw_initMcuCoreHW.h"
#include "sc_systemCall.h"
#include "ivr_ivorHandler.h"


/*
 * Defines
 */

/** Companion of C's offsetof: The size of a field inside a struct. */
#define sizeoffield(type, fieldName) (sizeof(((type*)0)->fieldName))

#define IVR_CHECK_INTERFACE_S2C                                                             \
    static_assert( sizeof(prc_userTaskConfig_t) == SIZE_OF_TASK_CONF                        \
                   &&  offsetof(prc_userTaskConfig_t, taskFct) == O_TCONF_pFct              \
                   &&  offsetof(prc_userTaskConfig_t, taskFct) == 0                         \
                   &&  sizeoffield(prc_userTaskConfig_t, taskFct) == 4                      \
                   &&  offsetof(prc_userTaskConfig_t, tiTaskMax) == O_TCONF_tiMax           \
                   &&  sizeoffield(prc_userTaskConfig_t, tiTaskMax) == 4                    \
                   &&  offsetof(prc_userTaskConfig_t, PID) == O_TCONF_pid                   \
                   &&  sizeoffield(prc_userTaskConfig_t, PID) == 1                          \
                 , "struct prc_userTaskConfig_t: Inconsistent interface between"            \
                   " assembler and C code"                                                  \
                 );                                                                         \
    static_assert( sizeof(prc_processDesc_t) == SIZE_OF_PROCESS_DESC                        \
                   &&  offsetof(prc_processDesc_t, userSP) == O_PDESC_USP                   \
                   &&  O_PDESC_USP == 0                                                     \
                   &&  offsetof(prc_processDesc_t, cntTotalTaskFailure) == O_PDESC_CNTTOT   \
                   &&  sizeoffield(prc_processDesc_t, cntTotalTaskFailure) == 4             \
                   &&  offsetof(prc_processDesc_t, cntTaskFailureAry) == O_PDESC_CNTTARY    \
                   &&  sizeoffield(prc_processDesc_t, cntTaskFailureAry)                    \
                       == IVR_NO_CAUSES_TASK_ABORTION*4                                     \
                 , "struct prc_processDesc_t: Inconsistent interface between"               \
                   " assembler and C code"                                                  \
                 );                                                                         \
    static_assert( sizeof(sc_systemCallDesc_t) == SIZE_OF_SC_DESC                           \
                   &&  offsetof(sc_systemCallDesc_t, addressOfFct) == O_SCDESC_sr           \
                   &&  sizeoffield(sc_systemCallDesc_t, addressOfFct) == 4                  \
                   &&  offsetof(sc_systemCallDesc_t, conformanceClass) == O_SCDESC_confCls  \
                   &&  sizeoffield(sc_systemCallDesc_t, conformanceClass) == 4              \
                 , "struct sc_systemCallDesc_t: Inconsistent interface between"             \
                   " assembler and C code"                                                  \
                 );


/** Helper macro: An OS handler from the interrupt service has choseable properties, which
    are encoded in unused bits of the address of the handler function. The macro helps
    composing the needed handler representation. */
#define PRC_IRQ_HANDLER(fct, isPreemptable) (void (*)())                                \
                                            (isPreemptable? (uintptr_t)(fct)+(1u<<31u)  \
                                                          : (uintptr_t)(fct)            \
                                            )

/*
 * Local type definitions
 */


/*
 * Local prototypes
 */

/** The empty default interrupt service routine. */
void prc_dummyINTCInterruptHandler(void);


/*
 * Data definitions
 */

/** The table of pointers to the actual IRQ service routines is implemented in the
    assembler code (There we have better control of the required alignment constraints.)\n
      Note, the specified functions are normal, proper C functions; no considerations about
    specific calling conventions (e.g. without stack frame) or according type decorations
    need to be made. */
extern prc_interruptServiceRoutine_t ivr_INTCInterruptHandlerAry[256];


#if DEBUG
/** If an interrupt is enabled in an I/O device but there's no handler registered at the
    INTC then a dummy handler is installed, which will halt the software in an assertion
    and report the causing interrupt in this global variable.
      @remark This is a development tool only and not compiled in PRODUCTION compilation. */
volatile uint32_t prc_idxUnregisteredInterrupt SECTION(.data.OS.prc_idxUnregisteredInterrupt) =
                                                                                    UINT_MAX;
#endif

/** Forward declaration of pointers to boundaries of stack areas, which are actually
    defined in the linker script. Used to initialize the global process structure. */
extern uint32_t ld_stackEndP1[0], ld_stackEndP2[0], ld_stackEndP3[0], ld_stackEndP4[0];

/** Array holding run-time data for all processes. Note the process IDs have a one based
    index (as 0 is reserved for the kernel process) but this is a normal array with zero
    based index. Use PID-1 as index into the array. */
prc_processDesc_t prc_processAry[PRC_NO_PROCESSES] SECTION(.data.OS.prc_processAry) =
{
    /** Process 1. */
    [0] = { .userSP = (uint32_t)ld_stackEndP1 - 16
            , .state = 0
            , .cntTotalTaskFailure = 0
            , .cntTaskFailureAry = {[0 ... (IVR_NO_CAUSES_TASK_ABORTION-1)] = 0}
          }

    /** Process 2. */
    , [1] = { .userSP = (uint32_t)ld_stackEndP2 - 16
              , .state = 0
              , .cntTotalTaskFailure = 0
              , .cntTaskFailureAry = {[0 ... (IVR_NO_CAUSES_TASK_ABORTION-1)] = 0}
            }

    /** Process 3. */
    , [2] = { .userSP = (uint32_t)ld_stackEndP3 - 16
              , .state = 0
              , .cntTotalTaskFailure = 0
              , .cntTaskFailureAry = {[0 ... (IVR_NO_CAUSES_TASK_ABORTION-1)] = 0}
            }

    /** Process 4. */
    , [3] = { .userSP = (uint32_t)ld_stackEndP2 - 16
              , .state = 0
              , .cntTotalTaskFailure = 0
              , .cntTaskFailureAry = {[0 ... (IVR_NO_CAUSES_TASK_ABORTION-1)] = 0}
            }
};


/*
 * Function implementation
 */

/**
 * Dummy interrupt handler. On initialization of the INTC (see
 * prc_initINTCInterruptController()), this function is put into all 256 interrupt vectors
 * in the table.\n
 *   The dummy handler can't reasonably service the interrupt. It would need to know the
 * source of interrupt to acknowledge the interrupt there (mostly the interrupt bit in the
 * status word of an I/O device needs to be cleared). Without doing this acknowledge, the
 * same interrupt would be served immediately again after return from the handler. This is
 * effectively an infinite loop. Better to report this as a problem - in DEBUG compilation
 * an assertion fires. In PRODUCTION compilation it does nothing and returns but the
 * initialization has given it a priority that will make the interrupt never be served at
 * all.\n
 *   To implement a real service, you would replace the default handler by your service
 * implementation using prc_installINTCInterruptHandler().
 */
void prc_dummyINTCInterruptHandler(void)
{
    /* If this assertion fired then you enabled an interrupt on hardware level (I/O device
       configuration) but you didn't use prc_installINTCInterruptHandler() in your code to
       install an adequate service handler for it.
         You can find the address of the interrupt vector in register INTC_IACKR_PRC0, or
       0xfff48010. Subtract the address of the SW vector table ivr_INTCInterruptHandlerAry
       (see application map file) and divide by word size 4; this yields the interrupt
       index, which can be resolved to the interrupt source with help of the MCU reference
       manual, section 28.7, table 28-4. */
#if DEBUG
    /* We put the causing interrupt into a global debug variable for convenience. */
    prc_idxUnregisteredInterrupt =
                         (INTC.IACKR_PRC0.R - (uint32_t)&ivr_INTCInterruptHandlerAry[0]) / 4;
    assert(false);
#endif
} /* End of prc_dummyINTCInterruptHandler */



/**
 * Initialize the interrupt controller INTC. The interrupt table with all interrupt service
 * descriptors is initialized to contain a dummy ISR for all interrupts and it is then
 * registered at the hardware device INTC for use.\n
 *   The interrupt default handler is prc_dummyINTCInterruptHandler(). It does nothing in
 * PRODUCTION compilation but an assertion will fire in DEBUG compilation in order to
 * indicate the missing true handler for an enabled interrupt.\n
 *   Note, this function locally sets but does not touch the enable external interrupts bit
 * in the machine status register. You will call it normally at system startup time, when
 * all interrupts are still disabled, then call prc_installINTCInterruptHandler()
 * repeatedly for all interrupts your code is interested in and eventually enable the
 * interrupt processing at the CPU.
 */
void prc_initINTCInterruptController(void)
{
    IVR_CHECK_INTERFACE_S2C

    /* Here, where we have one time called code, we can double-check some static
       constraints of the assembler implementation.
         The configuration table sc_systemCallDescAry is addressed with a short
       instruction, which requires that it resides at a 15 Bit address. This should
       normally be ensured by the linker script. */
    assert((uintptr_t)sc_systemCallDescAry < 0x8000);

    /* Prepare the vector table with all interrupts being served by our problem reporting
       dummy handler. */
    unsigned int u;
    for(u=0; u<sizeOfAry(ivr_INTCInterruptHandlerAry); ++u)
    {
        /* Note, in DEBUG compilation we configure the dummy handler with a priority that
           will make it used; the reason is that the dummy handler - although it can't
           really do the job of interrupt servicing - can report the problem of a bad
           interrupt configuration in the user code. (It's assumed that debugger is
           available during development time.) In PRODUCTION compilation, the dummy handler
           will never serve because of the priority being zero. */
        prc_installINTCInterruptHandler( &prc_dummyINTCInterruptHandler
                                       , /* vectorNum */ u
#ifdef DEBUG
                                       , /* psrPriority */ 1
#else
                                       , /* psrPriority */ 0
#endif
                                       , /* isPreemptable */ false
                                       );
    } /* for(All entries in the INTC vector table) */

    /* Normally, this function should always be called at the very first beginning, when
       all interrupts are still globally disabled at the CPU. However, we make it safe
       against deviating code constructs if we locally disable all interrupts. */
    uint32_t msr = ihw_enterCriticalSection();

    /* Block Configuration register, INTC_BCR0
       VTES_PRC0, 0x20: 0 for 4 Byte entries, 1 for 8 Byte entries
       HVEN_PRC0, 0x1, 0: SW vector, 1: HW vector mode */
    INTC.BCR.R = 0;

    /* The address of our vector table is stored in field VTBA_PRC0. Only the most
       significant 21 Bit will matter, the reset will at run-time be replaced by the index
       of the pending interrupt. */
    assert(((uintptr_t)&ivr_INTCInterruptHandlerAry[0] & 0x7ff) == 0);
    INTC.IACKR_PRC0.R = (uint32_t)&ivr_INTCInterruptHandlerAry[0];

    /* The current priority is set to 0. */
    INTC.CPR_PRC0.B.PRI = 0;

    /* Restore the machine status register including the enable external interrupt bit.
       For the normal, intended use case this won't have an effect. */
    ihw_leaveCriticalSection(msr);

} /* End of prc_initINTCInterruptController */




/**
 * This function can be used to install an interrupt service for a given I/O device.
 * It will also set the Priority Select Register for the I/O device's IRQ to the desired
 * value.
 *   @param pInterruptHandler
 * The interrupt service routine by reference.
 *   @param vectorNum
 * All possible external interrupt sources are hardwired to the interrupt controller. They
 * are indentified by index. The table, which interrupt source (mostly I/O device) is
 * connected to the controller at which index can be found in the MCU reference manual,
 * section 28.7, table 28-4.
 *   @param psrPriority
 * The priority at which the interrupt is served. 0..15. 0 is useless, it would never be
 * served, 1 is the lowest real priority and 15 the highest. Preemption of a handler (if
 * enabled), which serves an interrupt of priority n will be possible only by another
 * interrupt of priority n+1 or higher.
 *   @param isPreemptable
 * For each interrupt it can be sayed, whether it is preemptable by other interrupts of
 * higher priority or not. If this is \a false then the interrupt handler will always be
 * entered with the status bit EE reset in the machine status register MSR.\n
 *   Note, a handler, which has been declared non-premptable is allowed to set the EE bit
 * itself. It can thus first do some operations without any race-conditions with other
 * interrupts and then continue without further locking normal interrupt processing.
 *   @remark
 * The function can be used at any time. It is possible to exchange a handler at run-time,
 * while interrrupts are being processed. However, the normal use case will rather be to
 * call this function for all required interrupts and only then call the other function
 * ihw_resumeAllInterrupts().\n
 *   This function must not be called for an interrupt number n from the context of that
 * interrupt n.
 */
void prc_installINTCInterruptHandler( const prc_interruptServiceRoutine_t pInterruptHandler
                                    , unsigned short vectorNum
                                    , unsigned char psrPriority
                                    , bool isPreemptable
                                    )
{
    /* We permit to use this function at any time, i.e. even while interrupts may occur. We
       need to disable them shortly to avoid inconsistent states (vector and priority). */
    uint32_t msr = ihw_enterCriticalSection();

    ivr_INTCInterruptHandlerAry[vectorNum] = PRC_IRQ_HANDLER(pInterruptHandler, isPreemptable);

    /* Set the PSR Priority */
    INTC.PSR[vectorNum].B.PRI = psrPriority;

    ihw_leaveCriticalSection(msr);

} /* End of prc_installINTCInterruptHandler */



/**
 * Initialize the data structure with all process descriptors. This mainly means to
 * initialize the stack memory.
 *   @return
 * The function returns \a false if a configuration error is detected. The software must
 * not start up in this case. Normally, you will always receive a \a true. Since it is only
 * about static configuration, the returned error may be appropriately handled by an
 * assertion.
 *   @param isProcessConfiguredAry
 * This array contains an entry for each of the supported processes. After return, the
 * entry with index i tells, whether the process with PID i is configured for use. (Which
 * mainly relates to some stack space configured or not in the linker script.)\n
 *   Note, isProcessConfiguredAry[0] relates to the OS and will always be \a true.
 */
bool prc_initProcesses(bool isProcessConfiguredAry[1+PRC_NO_PROCESSES])
{
    bool isConfigOk = true;
    
    /* If the kernel process wouldn't be configured correctly then we would never get here. */
    isProcessConfiguredAry[0] = true;
    
    /* Fill all process stacks with the empty-pattern, which is applied for computing the
       stackusage. */
    extern uint32_t ld_stackStartP1[], ld_stackStartP2[], ld_stackStartP3[], ld_stackStartP4[]
                  , ld_stackEndP1[], ld_stackEndP2[], ld_stackEndP3[], ld_stackEndP4[];
    uint32_t * const stackStartAry[PRC_NO_PROCESSES] = { [0] = ld_stackStartP1
                                                       , [1] = ld_stackStartP2
                                                       , [2] = ld_stackStartP3
                                                       , [3] = ld_stackStartP4
                                                       };
    const uint32_t * const stackEndAry[PRC_NO_PROCESSES] = { [0] = ld_stackEndP1
                                                           , [1] = ld_stackEndP2
                                                           , [2] = ld_stackEndP3
                                                           , [3] = ld_stackEndP4
                                                           };
    unsigned int idxP;
    for(idxP=0; idxP<PRC_NO_PROCESSES; ++idxP)
    {
        /* Disable the process by default. */
        prc_processAry[idxP].state = 0;
        isProcessConfiguredAry[idxP+1] = false;
        
        /* Stack size: May be zero if process is not used at all. Otherwise we demand a
           reasonable minimum stack size - anything else is almost certain a configuration
           error. */
        const unsigned int sizeOfStack = (unsigned int)stackEndAry[idxP]
                                         - (unsigned int)stackStartAry[idxP];
        if(sizeOfStack > 0)
        {
            if(sizeOfStack >= 256  &&  sizeOfStack <= 0x100000
               &&  ((uintptr_t)stackStartAry[idxP] & 0x7) == 0
               &&  (sizeOfStack & 0x7) == 0
              )
            {
                memset(stackStartAry[idxP], /* c */ 0xa5, sizeOfStack);
                stackStartAry[idxP][sizeOfStack/4 - 4] = 0u;
                stackStartAry[idxP][sizeOfStack/4 - 3] = 0xffffffffu;
                stackStartAry[idxP][sizeOfStack/4 - 2] = 0xffffffffu;
                stackStartAry[idxP][sizeOfStack/4 - 1] = 0xffffffffu;
                prc_processAry[idxP].userSP = (uint32_t)stackEndAry[idxP] - 16;
                prc_processAry[idxP].state = 1;
                
                /* Stack alright, process may be used. */
                isProcessConfiguredAry[idxP+1] = true;
            }
            else
                isConfigOk = false;
        }
        else
            prc_processAry[idxP].userSP = 0;

        prc_processAry[idxP].cntTotalTaskFailure = 0;
        memset( &prc_processAry[idxP].cntTaskFailureAry[0]
              , /* c */ 0
              , sizeOfAry(prc_processAry[idxP].cntTaskFailureAry)
              );
    } /* End for(All processes) */

    return isConfigOk;

} /* End of prc_initProcesses */


/**
 * System call implementation to suspend a process. All currently running tasks belonging
 * to the process are aborted and the process is stopped forever (i.e. there won't be
 * further task starts or I/O driver callback invocations).\n
 *   Suspending a process of PID i is permitted only to processes of PID j>i.
 *   @param pidOfCallingTask
 * Process ID of calling user task.
 *   @param PID
 * The ID of the process to suspend. Needs to be not zero (OS process) and lower than the
 * ID of the calling process. Otherwise the calling task is aborted with exception
 * #IVR_CAUSE_TASK_ABBORTION_SYS_CALL_BAD_ARG.
 *   @remark
 * The system call is the implementation of a system call of conformance class "simple".
 * Such a system call can be implemented in C but it needs to be run with all interrupts
 * suspended. It cannot be preempted. Suitable for short running services only.\n
 *   @remark
 * Tasks of the suspended process can continue running for a short while until their abort
 * conditions are checked the next time. The likelihood of such a continuation is little
 * and the duration is in the magnitude of a Millisecond.
 *   @remark
 * This function must never be called directly. The function is only made for placing it in
 * the global system call table.
 */
void prc_scSmplHdlr_suspendProcess(uint32_t pidOfCallingTask, uint32_t PID)
{
    if(PID != 0  &&  pidOfCallingTask > PID)
        prc_processAry[PID].state = 0;
    else
    {
        /* The calling process doesn't have enough privileges to suspend the aimed process.
           This is a severe user code error, which is handled with an exception, task abort
           and counted error.
             Note, this function does not return. */
        ivr_systemCallBadArgument();
    }
} /* End of prc_scSmplHdlr_suspendProcess */
