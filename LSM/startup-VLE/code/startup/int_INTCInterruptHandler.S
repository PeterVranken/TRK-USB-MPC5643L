/**
 * @file int_INTCInterruptHandler.S
 * VLE assembler implementation of interrupt handler for external interrupts handled by
 * the INTC. Used for MPC5643L in Lockstep Mode (LSM). The code has been designed for use
 * with GCC for PowerPC EABI.
 *
 * Copyright (C) 2017 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   int_INTCInterruptHandler
 * Local functions
 */

#ifndef __VLE__
# error This file is intended for compilation for VLE instruction set only
#endif

/*
 * Include files
 */

#include "sup_settings.h"


/*
 * Defines
 */

/** Address of interrupt controller INTC0 in memory map. */
#define INTC0           0xfff48000

/** Address of INTC interrupt acknowledge register in memory map, for processor 0. */
#define INTC_IACKR_PRC0 (INTC0+0x10)

/** Address of INTC end of interrupt register in memory map, for processor 0. */
#define INTC_EOIR_PRC0  (INTC0+0x18)


/*
 * External function declarations
 */


/*
 * Data declarations
 */



/*
 * Function implementation
 */



/** The table of function pointers into the actual IRQ handlers is implemented here in the
    assembler code, where we have better control of the required alignment constraints.\n
      Note, the entries in the table are normal, proper C functions; no considerations
    about specific calling conventions (e.g. without stack frame) or according type
    decorations need to be made. */
    .section .INTCInterruptHandlerAry, "a"
    #define DEF_HDL ihw_dummyINTCInterruptHandler /* Use abbrev. for condensed source code */
    .extern DEF_HDL
    .global int_INTCInterruptHandlerAry
    .align  16      /* 16 Bit alignment required by INTC hardware. The statement makes the
                       implementation safe but does not say anything about efficient memory
                       allocation. To avoid a big waste of memory the section allocation in
                       the linker file should be made on an according memory address, too. */ 
int_INTCInterruptHandlerAry:
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    .dc.l   DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL, DEF_HDL
    #undef DEF_HDL

/* End of int_INTCInterruptHandlerAry */




/**
 *   @func sup_INTCInterruptHandler
 * This is the interrupt handler for the MCU trap #4, External Interrupts. This trap
 * relates to interrupts controlled by the INTC. The INTC provides the vector to the
 * appropriate service function and handles the preemption or mutual exclusion of
 * interrupts by providing an effective priority scheme.\n
 *   The handler implementation here will save the CPU context onto the stack, read the
 * service function pointer and call this function. It'll then acknowledge the interrupt
 * (thereby triggering the priority handling of the INTC) restore the context and return
 * from interrupt.
 *   @remark
 * This code is based on NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file
 * IntcInterrupts_p0.c, l. 102ff.
 */
    .section .text.ivor, "a"
    .globl  int_INTCInterruptHandler
    .type   int_INTCInterruptHandler, @function
    .align  4
int_INTCInterruptHandler:

    e_stwu  sp, -80(sp)         /* Create stack frame */
    
    /* Store all GPRegisters and SPR, which are volatile according to the EABI. */
    e_stmvgprw  8(sp)           /* r0, r3-r12: 11*4 Byte */
    e_stmvsprw  52(sp)          /* CR, LR, CTR, XER: 4*4 Byte */
    e_stmvsrrw  68(sp)          /* SRR0/SRR1: 2*4 Byte */

    /* Clear request to processor; r3 contains the address of the ISR */
    e_lis   r3, INTC_IACKR_PRC0@ha      /* Read pointer into ISR Vector Table
                                           int_INTCInterruptHandlerAry */
    e_lwz   r3, INTC_IACKR_PRC0@l(r3)   /* Load INTC_IACKR, which clears request to
                                           processor */
    se_lwz  r3, 0x0(r3)         /* Read ISR address from ISR Vector Table using pointer  */

    mfmsr   r4          /* Read the MSR for manipulation as appropriate for the user code. */
    
    /* For preemptable interrupt handlers set the EE bit in the MSR. The property
       preemptability of a handler is encoded as MSBit of the vector in the table. */
    e_cmpwi cr2, r3, 0
    e_bge   cr2, int_endIfPreemptable
    
    /* Relevant MSR bits. These bits are now set in the MSR for execution of the
       application interrupt handler.
         0x02000000: SPE
         0x00020000: CE
         0x00008000: EE (depends, only for preemtable handlers)
         0x00001000: ME */
    
int_isPreemptable:
    /* Clear the flag bit to get the proper address back. */
    e_rlwinm r3, r3, 0, 1, 31   /* Keep bits 0..30 in r3 */
    
    /* Enable processor recognition of interrupts. */
    e_or2i  r4, 0x8000          /* Set MSR[EE]=1 */

int_endIfPreemptable:
    /* The handler code requires the full instruction set but the SPE bit is reset on entry
       in an interrupt. Set it again prior to branching into the user code.
         There's no reason to lock the higher prior interrupts (critical and machine
       check). At minimum there's an empty trap handler, which reports the problem to the
       SW developer. */
    e_or2is r4, 0x0202
    e_or2i  r4, 0x1000

    mtmsr   r4          /* From here, other external interrupts can be enabled again. */

    /* Branch to ISR handler address from SW vector table */
    mtlr    r3                  /* Store ISR address to LR */
    se_blrl                     /* Branch to ISR, but return here */

    /* Restore those registers, which don't need interrupt lock. */
    e_lmvsprw   52(sp)          /* CR, LR, CTR, XER: 4*4 Byte */
    e_lmvsrrw   68(sp)          /* SRR0/SRR1: 2*4 Byte */
    
    /* Disable processor recognition of interrupts. */
    wrteei  0

    /* Ensure interrupt flag has finished clearing */
    /// @todo Double-check. Docu MCU says that wrteei does do instruction synchronization. Is it about memory vs. instruction synchronization?
    /// @todo Newer NXP sample code toggles the order of msync and wrteei 0
    msync

    /* Write 0 to INTC_EOIR, informing INTC to restore priority as it was on entry to this
       handler. */
    se_li   r3, 0
    e_lis   r4, INTC_EOIR_PRC0@ha   /* Load upper half of INTC_EOIR address */
    e_stw   r3, INTC_EOIR_PRC0@l(r4)/* Write 0 to INTC_EOIR */

    /* Restore GPRegisters r0, r3-r12 */
    e_lmvgprw   8(sp)           /* r0, r3-r12: 11*4 Byte */

    /* Restore space on stack */
    e_la    sp, 80(sp)

    /* End of Interrupt */
    se_rfi
    
    .size   int_INTCInterruptHandler, .-int_INTCInterruptHandler
/* End of int_INTCInterruptHandler */

    /* At the end of the code: Avoid a gap in the section. This makes the flash procedure
       in Code Warrior 10.7 fail to flash the code. */
    .align  4
