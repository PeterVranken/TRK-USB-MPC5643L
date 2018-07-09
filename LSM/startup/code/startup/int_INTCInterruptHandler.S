/**
 * @file int_INTCInterruptHandler.S
 * Book E assembler implementation of interrupt handler for external interrupts handled by
 * the INTC. Used for MPC5643L in Lockstep Mode (LSM). The code has been designed for use
 * with GCC for PowerPC EABI.
 *
 * Copyright (C) 2017-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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

#ifdef __VLE__
# error This file is intended for compilation for Book E instruction set only
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
 *   @func int_INTCInterruptHandler
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

int_prolog:
    stwu    sp, -80(sp)         /* Create stack frame */
    
    /* Store locally needed working registers. */
    stw     r0, 32(sp)
    mfcr    r0                  /* Store CR */
    stw     r0, 28(sp)
    stw     r3, 36(sp)          /* Store r3 to work with */
    stw     r4, 40(sp)          /* Store r4 to work with */
    
    /* Save SRR0 and SRR1. */
    mfsrr0  r0                  /* Store SRR0 (before enabling EE) */
    stw     r0, 8(sp)
    mfsrr1  r0                  /* Store SRR1 (before enabling EE) */
    stw     r0, 12(sp)

    /* Clear request to processor; r3 contains the address of the ISR */
    lis     r3, INTC_IACKR_PRC0@ha      /* Read pointer into ISR Vector Table
                                           int_INTCInterruptHandlerAry */
    lwz     r3, INTC_IACKR_PRC0@l(r3)   /* Load INTC_IACKR, which clears request to
                                           processor */
    lwz     r3, 0x0(r3)         /* Read ISR address from ISR Vector Table using pointer  */

    mfmsr   r4          /* Read the MSR for manipulation as appropriate for the user code. */
    
    /* For preemptable interrupt handlers set the EE bit in the MSR. The property
       preemptability of a handler is encoded as MSBit of the vector in the table. */
    cmpwi   cr7, r3, 0
    bge     cr7, int_endIfPreemptable

    /* Relevant MSR bits. These bits are now set in the MSR for execution of the
       application interrupt handler.
         0x02000000: SPE
         0x00020000: CE
         0x00008000: EE (depends, only for preemtable handlers)
         0x00001000: ME */
    
int_isPreemptable:
    /* Clear the flag bit to get the proper address back. */
    rlwinm  r3, r3, 0, 1, 31    /* Keep bits 0..30 in r3 */
    
    /* Enable processor recognition of interrupts. */
    ori     r4, r4, 0x8000      /* Set MSR[EE]=1 */
int_endIfPreemptable:
    /* The handler code requires the full instruction set but the SPE bit is reset on entry
       in an interrupt. Set it again prior to branching into the user code.
         There's no reason to lock the higher prior interrupts (critical and machine
       check). At minimum there's an empty trap handler, which reports the problem to the
       SW developer. */
    oris    r4, r4, 0x0202
    ori     r4, r4, 0x1000

    mtmsr   r4

    /* Save rest of context required by EABI */
    stw     r12, 72(sp)         /* Store r12 */
    stw     r11, 68(sp)         /* Store r11 */
    stw     r10, 64(sp)         /* Store r10 */
    stw     r9, 60(sp)          /* Store r9 */
    stw     r8, 56(sp)          /* Store r8 */
    stw     r7, 52(sp)          /* Store r7 */
    stw     r6, 48(sp)          /* Store r6 */
    stw     r5, 44(sp)          /* Store r5 */
    mfxer   r0                  /* Store XER */
    stw     r0, 24(sp)
    mfctr   r0                  /* Store CTR */
    stw     r0, 20(sp)
    mflr    r0                  /* Store LR */
    stw     r0, 16(sp)

    /* Branch to ISR handler address from SW vector table */
    mtlr    r3                  /* Store ISR address to LR */
    blrl                        /* Branch to ISR, but return here */

int_epilog:
    /* Restore context required by EABI (except still used working registers) */
    lwz     r0, 16(sp)          /* Restore LR */
    mtlr    r0
    lwz     r0, 20(sp)          /* Restore CTR */
    mtctr   r0
    lwz     r0, 24(sp)          /* Restore XER */
    mtxer   r0
    lwz     r0, 28(sp)          /* Restore CR */
    mtcr    r0
    lwz     r5, 44(sp)          /* Restore r5 */
    lwz     r6, 48(sp)          /* Restore r6 */
    lwz     r7, 52(sp)          /* Restore r7 */
    lwz     r8, 56(sp)          /* Restore r8 */
    lwz     r9, 60(sp)          /* Restore r9 */
    lwz     r10, 64(sp)         /* Restore r10 */
    lwz     r11, 68(sp)         /* Restore r11 */
    lwz     r12, 72(sp)         /* Restore r12 */

    /* Disable processor recognition of interrupts. */
    wrteei  0

    /* Ensure interrupt flag has finished clearing */
    /// @todo Double-check. Docu MCU says that wrteei does do instruction synchronization. Is it about memory vs. instruction synchronization?
    /// @todo Newer NXP sample code toggles the order of msync and wrteei 0
    msync

    /* Write 0 to INTC_EOIR, informing INTC to restore priority as it was on entry to this
       handler. */
    li      r3, 0
    lis     r4, INTC_EOIR_PRC0@ha   /* Load upper half of INTC_EOIR address */
    stw     r3, INTC_EOIR_PRC0@l(r4)/* Write 0 to INTC_EOIR */

    /* Restore Working Registers */
    lwz     r3, 36(sp)          /* Restore r3 */
    lwz     r4, 40(sp)          /* Restore r4 */

    /* Retrieve SRR0 and SRR1 */
    lwz     r0, 8(sp)           /* Restore SRR0 */
    mtsrr0  r0
    lwz     r0, 12(sp)          /* Restore SRR1 */
    mtsrr1  r0

    /* Restore remaining working register r0 */
    lwz     r0, 32(sp)          /* Restore r0 */

    /* Discard stack frame */
    addi    sp, sp, 80

    /* End of Interrupt */
    rfi

    .size   int_INTCInterruptHandler, .-int_INTCInterruptHandler
/* End of int_INTCInterruptHandler */

    /* At the end of the code: Avoid a gap in the section. This makes the flash procedure
       in Code Warrior 10.7 fail to flash the code. */
    .align  4
