/**
 * @file rtos_externalInterrupt.c
 * This module holds a few routine, which are needed to configure the handling of External
 * Interrupts (IVOR #4). The interrupt controller is initialized and the operating system
 * code has the chance to register the handlers for particular I/O interrupts.\n
 *   The code in this module used to be part of the startup code in the other samples.
 *
 * Copyright (C) 2017-2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   rtos_osInitINTCInterruptController
 *   rtos_osRegisterInterruptHandler
 *   rtos_dummyINTCInterruptHandler
 * Module inline interface
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
#include "rtos.h"
#include "rtos_externalInterrupt.h"


/*
 * Defines
 */

/** Helper macro: An OS handler from the interrupt service has configurable properties,
    which are encoded in unused bits of the address of the handler function. The macro
    helps composing the needed handler representation. */
#define IRQ_HANDLER(fct, isPreemptable) (void (*)())                                \
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
void rtos_dummyINTCInterruptHandler(void);


/*
 * Data definitions
 */

/** The table of pointers to the actual IRQ service routines is implemented in the
    assembler code (There we have better control of the required alignment constraints.)\n
      Note, the specified functions are normal, proper C functions; no considerations about
    specific calling conventions (e.g. without stack frame) or according type decorations
    need to be made. */
extern rtos_interruptServiceRoutine_t rtos_INTCInterruptHandlerAry[256];


#if DEBUG
/** If an interrupt is enabled in an I/O device but there's no handler registered at the
    INTC then a dummy handler is installed, which will halt the software in an assertion
    and report the causing interrupt in this global variable.
      @remark This is a development tool only and not compiled in PRODUCTION compilation. */
volatile uint32_t SECTION(.data.OS.rtos_idxUnregisteredInterrupt)
                                                rtos_idxUnregisteredInterrupt = UINT_MAX;
#endif


/*
 * Function implementation
 */

/**
 * Dummy interrupt handler. On initialization of the INTC (see
 * rtos_osInitINTCInterruptController()), this function is put into all 256 interrupt vectors
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
 * implementation using rtos_osRegisterInterruptHandler().
 */
void rtos_dummyINTCInterruptHandler(void)
{
    /* If this assertion fired then you enabled an interrupt on hardware level (I/O device
       configuration) but you didn't use rtos_osRegisterInterruptHandler() in your code to
       install an adequate service handler for it.
         You can find the address of the interrupt vector in register INTC_IACKR_PRC0, or
       0xfff48010. Subtract the address of the SW vector table rtos_INTCInterruptHandlerAry
       (see application map file) and divide by word size 4; this yields the interrupt
       index, which can be resolved to the interrupt source with help of the MCU reference
       manual, section 28.7, table 28-4. */
#if DEBUG
    /* We put the causing interrupt into a global debug variable for convenience. */
    rtos_idxUnregisteredInterrupt =
                         (INTC.IACKR_PRC0.R - (uint32_t)&rtos_INTCInterruptHandlerAry[0]) / 4;
    assert(false);
#endif
} /* End of rtos_dummyINTCInterruptHandler */



/**
 * Initialize the interrupt controller INTC. The interrupt table with all interrupt service
 * descriptors is initialized to contain a dummy ISR for all interrupts and it is then
 * registered at the hardware device INTC for use.\n
 *   The interrupt default handler is rtos_dummyINTCInterruptHandler(). It does nothing in
 * PRODUCTION compilation but an assertion will fire in DEBUG compilation in order to
 * indicate the missing true handler for an enabled interrupt.\n
 *   Note, this function temporarily clears the enable External Interrupts bit in the
 * machine status register but doesn't have changed it on return. You will call it normally
 * at system startup time, when all interrupts are still disabled, then call
 * rtos_osRegisterInterruptHandler() repeatedly for all interrupts your code is interested in
 * and eventually enable the interrupt processing at the CPU.
 *   @remark
 * This function must be called from supervisor mode only.
 */
void rtos_osInitINTCInterruptController(void)
{
    /* Prepare the vector table with all interrupts being served by our problem reporting
       dummy handler. */
    unsigned int u;
    for(u=0; u<sizeOfAry(rtos_INTCInterruptHandlerAry); ++u)
    {
        /* Note, in DEBUG compilation we configure the dummy handler with a priority that
           will make it used; the reason is that the dummy handler - although it can't
           really do the job of interrupt servicing - can report the problem of a bad
           interrupt configuration in the user code. (It's assumed that debugger is
           available during development time.) In PRODUCTION compilation, the dummy handler
           will never serve because of the priority being zero. */
        rtos_osRegisterInterruptHandler( &rtos_dummyINTCInterruptHandler
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
    uint32_t msr = rtos_osEnterCriticalSection();

    /* Block Configuration register, INTC_BCR0
       VTES_PRC0, 0x20: 0 for 4 Byte entries, 1 for 8 Byte entries
       HVEN_PRC0, 0x1, 0: SW vector, 1: HW vector mode */
    INTC.BCR.R = 0;

    /* The address of our vector table is stored in field VTBA_PRC0. Only the most
       significant 21 Bit will matter, the reset will at run-time be replaced by the index
       of the pending interrupt. */
    assert(((uintptr_t)&rtos_INTCInterruptHandlerAry[0] & 0x7ff) == 0);
    INTC.IACKR_PRC0.R = (uint32_t)&rtos_INTCInterruptHandlerAry[0];

    /* The current priority is set to 0. */
    INTC.CPR_PRC0.B.PRI = 0;

    /* Restore the machine status register including the enable external interrupt bit.
       For the normal, intended use case this won't have an effect. */
    rtos_osLeaveCriticalSection(msr);

} /* End of rtos_osInitINTCInterruptController */




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
 * section 28.7, table 28-4. The range is 0..255, checked by assertion.
 *   @param psrPriority
 * The priority at which the interrupt is served. Range is 0..15, checked by assertion. 0
 * is useless, it would never be served, 1 is the lowest real priority and 15 the highest.
 * Preemption of a handler (if enabled), which serves an interrupt of priority n will be
 * possible only by another interrupt of priority n+1 or higher.
 *   @param isPreemptable
 * For each interrupt it can be said, whether it is preemptable by other interrupts of
 * higher priority or not. If this is \a false then the interrupt handler will always be
 * entered with the status bit EE reset in the machine status register MSR.\n
 *   Note, a handler, which has been declared non-premptable is allowed to set the EE bit
 * itself. It can thus first do some operations without any race-conditions with other
 * interrupts and then continue without further locking normal interrupt processing.
 *   @remark
 * The function can be used at any time. It is possible to exchange a handler at run-time,
 * while interrrupts are being processed. However, the normal use case will rather be to
 * call this function for all required interrupts and only then call the other function
 * rtos_osResumeAllInterrupts().\n
 *   This function must not be called for an interrupt number n from the context of that
 * interrupt n.
 *   @remark
 * This function must be called from supervisor mode only.
 */
void rtos_osRegisterInterruptHandler( const rtos_interruptServiceRoutine_t pInterruptHandler
                                    , unsigned int vectorNum
                                    , unsigned int psrPriority
                                    , bool isPreemptable
                                    )
{
    /* We permit to use this function at any time, i.e. even while interrupts may occur. We
       need to disable them shortly to avoid inconsistent states (vector and priority). */
    uint32_t msr = rtos_osEnterCriticalSection();

    assert(((uintptr_t)pInterruptHandler & 0x80000000u) == 0);
    assert(vectorNum <= 255);
    rtos_INTCInterruptHandlerAry[vectorNum] = IRQ_HANDLER(pInterruptHandler, isPreemptable);

    /* Set the PSR Priority */
    assert(psrPriority <= 15);
    INTC.PSR[vectorNum].B.PRI = psrPriority;

    rtos_osLeaveCriticalSection(msr);

} /* End of rtos_osRegisterInterruptHandler */



