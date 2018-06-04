/**
 * @file int_interruptHandler.S
 * Book E assembler implementation of interrupt handlers for External Interrupts handled by
 * the INTC and for software interrupts. This module can be used as building block for an
 * operating system kernel for an e200z4 core equiped with an INTC interrupt controller.
 * The assembly code in this module implements the required context switches and offers a C
 * API, which enables implementing the scheduler in C without the need for further (inline)
 * assembler. The code has been designed for use with GCC for PowerPC EABI.\n
 *   This module is based on and replaces the interrupt handler for External Interrupts
 * from the standard startup code. This explains the re-use of the same external symbols
 * (handler function names and software vector table). The API from the startup code to
 * define the ISRs for External Interrupts can still be used; it has only a minor extension
 * to make the distinction between simple and kernel relevant interrupts.
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
 *   int_simpleSystemCall
 *   int_systemCallHandler
 *   int_fctOnContextEnd (weak default implementation)
 *   int_systemCall
 * Local functions
 */

#ifndef __VLE__

/*
 * Include files
 */

#include "int_interruptHandler.config.h"
#include "int_defStackFrame.h"


/*
 * Defines
 */


/** Address of interrupt controller INTC0 in memory map. */
#define INTC0           0xfff48000

/** Address of INTC current priority register in memory map, for processor 0. */
#define INTC_CPR_PRC0   (INTC0+0x8)

/** Address of INTC interrupt acknowledge register in memory map, for processor 0. */
#define INTC_IACKR_PRC0 (INTC0+0x10)

/** Address of INTC end of interrupt register in memory map, for processor 0. */
#define INTC_EOIR_PRC0  (INTC0+0x18)

/** Set this macro to 1 in order to enable the compilation of some self-test code in DEBUG
    compilation. */
#define DEBUG_CNT_CONTEXT_SWITCHES  1

/** The highest interrupt priority, which is system wide used for those interrupts, which
    are kernel relevant (i.e. which can do system calls and/or can provoke context
    switches).
      @remark The initial intention of allowing kernel interrupts at different priorities
    didn't work out. The priority ceiling mechanism of the INTC as proposed by NXP e.g. in
    the MCU reference manual doesn't work in the context of a context switching kernel. It
    cannot avoid that different interrupts preempt each other. (It can only ensure that this
    doesn't occur at code locations, where data coherency would be harmed.) See
    https://community.nxp.com/message/993795 for more.\n
      Caution, for this reason the definition must never be changed; all kernel interrupts
    have the INTC priority one. */
#define INT_MAX_KERNEL_IRQ_PRIO     1


#ifndef INT_USE_SHARED_STACKS
# error Bad configuration, INT_USE_SHARED_STACKS is undefined
#endif

/*
 * External function declarations
 */


/*
 * Data declarations
 */

#ifdef DEBUG
    .section .text
int_fileName:
    .asciz  "int_interruptHandler.S"
#endif

#if DEBUG_CNT_CONTEXT_SWITCHES == 1  && defined(DEBUG)
    .section .sdata
    .global int_cntRetFromIsr, int_cntIsrToSc, int_cntIsrToIsr, int_cntIsrToNewCtxt
    .global int_cntRetFromSc, int_cntScToSc, int_cntScToIsr, int_cntScToNewCtxt
    .global int_cntRetFromSimpleSc
int_cntRetFromIsr:
    .dc.l   0
int_cntIsrToSc:
    .dc.l   0
int_cntIsrToIsr:
    .dc.l   0
int_cntIsrToNewCtxt:
    .dc.l   0
int_cntRetFromSc:
    .dc.l   0
int_cntScToSc:
    .dc.l   0
int_cntScToIsr:
    .dc.l   0
int_cntScToNewCtxt:
    .dc.l   0
int_cntRetFromSimpleSc:
    .dc.l   0
#endif


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
 * This is the interrupt handler for the CPU interrupt IVOR #4, External Interrupts. IVOR #4
 * relates to interrupts controlled by the INTC. The INTC provides the vector to the
 * appropriate service function and handles the preemption or mutual exclusion of
 * interrupts by providing an effective priority scheme.\n
 *   The handler implementation here will save the CPU context onto the stack and read the
 * service function pointer and call this function. It distinguishes between simple and
 * kernel relevant service handlers. The former will acknowledge the interrupt
 * (thereby triggering the priority handling of the INTC), restore the context and return
 * from interrupt. The latter can do the same but on demand they will switch to another
 * execution context and return from interrupt in the new context.
 */
    .section .text.ivor, "a"
    .globl  int_INTCInterruptHandler
    .type   int_INTCInterruptHandler, @function
    .align  4
int_INTCInterruptHandler:

    /* Create stack frame */
    stwu    sp, -S_I_StFr(sp)

    /* Save locally needed working registers. */
    stw     r0, O_I_R00(sp)     /* Store r0 prior to modification */
    mfcr    r0                  /* Store CR prior to possible modification */
    stw     r0, O_I_CR(sp)
    mflr    r0                  /* Save LR before reusing it for call of ISR */
    stw     r0, O_I_LR(sp)
    stw     r3, O_I_R03(sp)     /* Store r3 prior to modification */

    /* Save SRR0 and SRR1. This needs to be done before enabling EE in order to enable
       nested interupts. */
    mfsrr0  r0
    stw     r0, O_SRR0(sp)
    mfsrr1  r0
    stw     r0, O_SRR1(sp)

    /* Clear request to processor; r3 contains the address of the ISR */
    lis     r3, INTC_IACKR_PRC0@ha      /* Read pointer into ISR Vector Table
                                           int_INTCInterruptHandlerAry */
    lwz     r3, INTC_IACKR_PRC0@l(r3)   /* Load INTC_IACKR, which clears request to
                                           processor */
    lwz     r3, 0x0(r3)         /* Read ISR address from ISR Vector Table using pointer  */

    /* For preemptable interrupt handlers set the EE bit in the MSR. The property
       preemptability of a handler is encoded as bit 31 (0x1) of the vector in the table. */
    rlwinm  r0, r3, 0, 31, 31   /* Move bit 31 of r3 to r0 and set CR1 */
    cmplwi  cr1, r0, 0

    /* Kernel relevant ISR's have bit 30 (0x2) set in the address. */
    rlwinm. r0, r3, 31, 31, 31  /* Move bit 30 of r3 to r0 and set CR0 */

    /* Clear the flag bits to get the proper address back. Do this without modifying CR. */
    rlwinm  r3, r3, 0, 0, 29    /* Keep bits 0..29 in r3 */
    mtlr    r3                  /* Put true ISR address in LR */

    /* Note: Do not change cr0 and cr1 in the next lines, we need the decisions later, when
       actually branching. */

#ifdef DEBUG
    beq     cr0, int_eeh_endCheckINTCCurPrio    /* Branch on simple ISR */

    /* An assertion ensures that the priority of this kernel interrupt has the only
       permitted value 1. */
    lis     r3, INTC_CPR_PRC0@ha
    lwz     r0, INTC_CPR_PRC0@l(r3)             /* Read current INTC prio */
    cmplwi  cr7, r0, INT_MAX_KERNEL_IRQ_PRIO    /* Compare with highest kernel prio */
    bgt-    cr7, int_eeh_errorBadKernelIrqPrio  /* Error if current > highest kernel prio */
int_eeh_endCheckINTCCurPrio:
#endif

    /* Relevant MSR bits. These bits are now modified in the MSR for execution of the
       application interrupt handler.
         Note, we must not set the MSR[SPE] bit. The 64 Bit SPE instructions are generally
       unwanted. Once we start using them, we need to save our CPU contexts on interrupts
       as full 64 Bit, although only occasionally used by the compiler. (Not setting SPE
       requires not using -mspe on the compiler command line.)
         0x02000000: SPE
         0x00020000: CE
         0x00008000: EE
         0x00004000: PR
         0x00001000: ME */

    /* Now reenable servicing of External Interrupts of priority above kernel prio 1. */
    beq     cr1, int_eeh_endIfPreemptable
    wrteei  1
int_eeh_endIfPreemptable:

    /* If we get here, and in case of a preemptable ISR, the handling of External
       Interrupts is re-enabled and we can become preempted by kernel irrelevant
       interrupts. */

    /* To minimize the interrupt lock time we save the rest of context required by EABI
       only after we have released the handling of further External Interrupts (of higher
       priority). */
    stw     r12, O_I_R12(sp)    /* Store r12 */
    stw     r11, O_I_R11(sp)    /* Store r11 */
    stw     r10, O_I_R10(sp)    /* Store r10 */
    stw     r9, O_I_R09(sp)     /* Store r9 */
    stw     r8, O_I_R08(sp)     /* Store r8 */
    stw     r7, O_I_R07(sp)     /* Store r7 */
    stw     r6, O_I_R06(sp)     /* Store r6 */
    stw     r5, O_I_R05(sp)     /* Store r5 */
    stw     r4, O_I_R04(sp)     /* Store r4 */
    mfctr   r0                  /* Store CTR */
    stw     r0, O_I_CTR(sp)
    mfxer   r0                  /* Store XER */
    stw     r0, O_I_XER(sp)

    /* Decide whether the servive handler in the application code can yield a context
       switch (kernel relevant handler) or not (simple handler). The calling signature is
       different. */
    beq     cr0, int_eeh_callSimpleIsr  /* Branch if bit 0 was not set in ISR address */

int_eeh_callOsIsr:
    /* Branch to kernel interrupt handler address from vector table. This is the signature of
       such a handler:
         int_retCodeKernelIsr_t osInterruptHandler(int_cmdContextSwitch_t *pCmdContextSwitch);
         The returned data has a size of 16 Byte and is placed in the stack. We pass the
       pointer to the C code. */
    la      r3, O_RET(sp)
    blrl                        /* Branch to handler, but return here */

    /* Evaluate returned data to see if we
       - simply return from ISR and continue the same context (like the simple ISR always
         does)
       - suspend this context and resume another, where we either
         - return from asynchronous, External Interrupt
         - return from system call */

    /* The C function return code in r3 is a bit set saying whether (!= 0) or not (== 0) to
       switch the context, whether to terminate the left context (LSB==1) and whether to
       create the context we switch to (MSB==1). */
    cmpwi   cr1, r3, 0
    beq     cr1, int_eeh_endCallSimpleIsr  /* Return as from simple ISR */
#if INT_USE_SHARED_STACKS == 1
    rlwinm. r3, r3, 0, 31, 31   /* Filter bit 0 (terminate) and set CR0 */
#endif

    /* If we get here we need to switch to another execution context. This means that we
       have to save all non-volatile registers, too. We do this in the stack frame of the
       left context. We save GPR 13 in order to become later able to restore all registers
       at once using lmw. */
    stmw    r13, O_I_R13(sp)    /* Write registers 13-31 */

    /* Disable processor recognition of interrupts. */
    wrteei  0
    // Elder NXP samples place a memory barrier here before doing the interrupt acknowledge
    // in the INTC, newer NXP sample toggle the order of wrteei and msync. We believe that
    // no memory barrier is required here. Successful processing of the code below does not
    // depend on the completion of the stores before and the order of EE=0 and write to
    // INTC is guaranteed by the instruction synchronizing character of wrteei
    /// @todo Intensive testing with commented msync
    msync

    /* Write to INTC_EOIR to make the INTC pop the priority of the now ending ISR from the
       INTC's priority stack.
         We could save the load zero instruction since any register content can be written
       but MCU reference manual, section 28.4.6, p. 919 recomments to write zero, "for
       future compatibility". */
    li      r0, 0
    lis     r3, INTC_EOIR_PRC0@ha       /* Load upper half of INTC_EOIR address */
    stw     r0, INTC_EOIR_PRC0@l(r3)    /* Write 0 to INTC_EOIR */

    /* The new current priority in CPR is read and saved in the left context. The priority
       ceiling protocol implemented in the INTC and applied by the system call handler
       permits the handling of interrupts, which asserted before the CPR has been raised.
       If this happened the current priority register will show the same value
       INT_MAX_KERNEL_IRQ_PRIO as before. On later return to the left context we need to
       restore that value. */
    lwz     r14, INTC_CPR_PRC0@l(r3)    /* INTC_EOIR_PRC0@ha == INTC_CPR_PRC0@ha */
    stw     r14, O_I_CPR(sp)

    /* Before we switch the context, we need to undo a PCP lock possibly done by a system
       call or user task code. The lock of kernel relevant interrupts must not persist in
       the new context. This would lock the entire system. */
    stw     r0, INTC_CPR_PRC0@l(r3)    /* INTC_EOIR_PRC0@ha == INTC_CPR_PRC0@ha */

    /* Switching context and resuming the targeted context is the same as for system
       calls. We need to inform that code that the suspended context was an asynchronous
       ISR. This is encoded by -1 in r14 as system call index. */
    /// @todo Can we use r11 or r12 for this purpose and avoid one register to be saved in case normal-return-without-context-switch?
    li      r14, -1

    /* The handler may have demanded to resume an existing or to create a new context.
         The equal bit of cr0 holds the information "terminate left context". */
    bgt     cr1, int_sch_doContextSwitch
    b       int_sch_startNewContext

int_eeh_callSimpleIsr:
    /* Branch to ISR handler address from SW vector table */
    blrl                        /* Branch to ISR, but return here */

int_eeh_endCallSimpleIsr:
    /* After return from application owned service code restore context required by EABI
       (except still used working registers) while handling of interrupts of higher
       priority is still enabled. */
    lwz     r0, O_I_CR(sp)      /* Restore CR */
    mtcr    r0
    lwz     r0, O_I_LR(sp)      /* Restore LR */
    mtlr    r0
    lwz     r0, O_I_CTR(sp)     /* Restore CTR */
    mtctr   r0
    lwz     r0, O_I_XER(sp)     /* Restore XER */
    mtxer   r0
    lwz     r4, O_I_R04(sp)     /* Restore r4 */
    lwz     r5, O_I_R05(sp)     /* Restore r5 */
    lwz     r6, O_I_R06(sp)     /* Restore r6 */
    lwz     r7, O_I_R07(sp)     /* Restore r7 */
    lwz     r8, O_I_R08(sp)     /* Restore r8 */
    lwz     r9, O_I_R09(sp)     /* Restore r9 */
    lwz     r10, O_I_R10(sp)    /* Restore r10 */
    lwz     r11, O_I_R11(sp)    /* Restore r11 */
    lwz     r12, O_I_R12(sp)    /* Restore r12 */

    /* Disable processor recognition of interrupts. */
    wrteei  0
    // Elder NXP samples place a memory barrier here before doing the interrupt acknowledge
    // in the INTC, newer NXP sample toggle the order of wrteei and msync. We believe that
    // no memory barrier is required here. Successful processing of the code down here does
    // not depend on the completion of the loads before and the order of EE=0 and write to
    // INTC is guaranteed by the instruction synchronizing character of wrteei
    /// @todo Intensive testing with commented msync
    msync

    /* Write to INTC_EOIR to make the INTC restore the priority as it was on entry to this
       handler. We could save the load zero instruction since any register content can be
       written but MCU reference manual, section 28.4.6, p. 919 recomments to write zero,
       "for future compatibility". */
    li      r0, 0
    lis     r3, INTC_EOIR_PRC0@ha       /* Load upper half of INTC_EOIR address */
    stw     r0, INTC_EOIR_PRC0@l(r3)    /* Write 0 to INTC_EOIR, pop priority */

#if DEBUG_CNT_CONTEXT_SWITCHES == 1  && defined(DEBUG)
    lwz     r3, int_cntRetFromIsr@sda21(%r0)
    addi    r3, r3, 1
    stw     r3, int_cntRetFromIsr@sda21(%r0)
#endif

    /* Restore r3 after last use */
    lwz     r3, O_I_R03(sp)     /* Restore r3 */

    /* Retrieve SRR0 and SRR1 */
    lwz     r0, O_SRR0(sp)      /* Restore SRR0 */
    mtsrr0  r0
    lwz     r0, O_SRR1(sp)      /* Restore SRR1 */
    mtsrr1  r0

    /* Restore remaining working register r0 */
    lwz     r0, O_I_R00(sp)       /* Restore r0 */

    /* Discard stack frame */
    la      sp, S_I_StFr(sp)

    /* End of interrupt */
    rfi

    /* Error trap: The current interrupt priority on entry into a kernel relvant ISR was
       found to be greater than the priority #INT_MAX_KERNEL_IRQ_PRIO, which has been
       specified as highest priority of all such ISRs. This can happen if a kernel
       relevant ISR is installed with a priority above #INT_MAX_KERNEL_IRQ_PRIO, which is
       strictly forbidden. */
int_eeh_errorBadKernelIrqPrio:
    wrteei  0
    b       .

    .size   int_INTCInterruptHandler, .-int_INTCInterruptHandler
/* End of int_INTCInterruptHandler */



/**
 *   @func int_simpleSystemCall
 * System calls are basically intended for operating system services, which can normaly
 * lead to some context switch decisions. This requires some overhead for handling of race
 * conditions and an API needs to be provided for commanding context switches. This
 * overhead is undesired for simple services, which do not interfere with kernel
 * operations. If such a servive requires the execution in supervisor mode then it can be
 * implemented as "simple system call". Simple system calls have positive indexes
 * (including zero) and their execution is implemented in this function.
 */
    .section .text.ivor
    .type   int_simpleSystemCall, @function
    .extern int_simpleSystemCallHandlerAry, int_noSimpleSystemCalls
    .align  2
int_simpleSystemCall:

    stwu    sp, -S_SSC_StFr(sp)    /* Create stack frame */

    /* Save SRR0 and SRR1. This needs to be done prior to re-enabling the External
       Interrupt handling. */
    mfsrr0  r0
    stw     r0, O_SRR0(sp)
    mfsrr1  r0
    stw     r0, O_SRR1(sp)

    /* After saving SRRi we can re-enable the handling of External Interrupts. */
    wrteei 1

    /* Check index of system call. */
#ifdef DEBUG
    mr      r0, r3          /* r0 must not be used for indexed lwz */
    lis     r3, int_noSimpleSystemCalls@ha
    lwz     r3, int_noSimpleSystemCalls@l(r3)
    cmplw   cr0, r0, r3
    bge     cr0, int_ssc_errorSysCallIndexOutOfRange
    mr      r3, r0
#endif

    /* Read ISR address from the simple system call vector table using the index in r3. */
    slwi    r3, r3, 2
    lwz     r3, int_simpleSystemCallHandlerAry(r3)
    mtlr    r3                  /* Put handler address in LR */

    /* Branch to simple system call handler address read from vector table. This is the
       signature of such a handler:
         uint32_t simpleSysCallHandler(uint32_t * const pMSR, ...); */
    la      r3, O_SRR1(sp)  /* Load pMSR into first function argument */
    blrl                    /* Branch to handler, but return here */

    /* System call result is in r3 and can be returned just like that. */

#if DEBUG_CNT_CONTEXT_SWITCHES == 1  && defined(DEBUG)
    lwz     r4, int_cntRetFromSimpleSc@sda21(%r0)
    addi    r4, r4, 1
    stw     r4, int_cntRetFromSimpleSc@sda21(%r0)
#endif

    /* Disable handling of External Interrupts is required for savely accessing the SSRi */
    wrteei  0
    // Elder NXP samples place a memory barrier here before interacting with the INTC,
    // newer NXP sample toggle the order of wrteei and msync. We believe that no memory
    // barrier is required here. Successful processing of the code down here does not
    // depend on the completion of the loads and stores before and the order of EE=0 and
    // writes to the SSRi is guaranteed by the instruction synchronizing character of
    // wrteei
    /// @todo Intensive testing with commented msync
    msync

    /* Restore SRR0 but set SRR1 - the value of the MSR may have been changed by the
       interrupt handler in the C code. */
    lwz     r0, O_SRR0(sp)      /* Prepare PC to go to */
    mtsrr0  r0
    lwz     r0, O_SRR1(sp)      /* Prepare value of MSR after return */
    mtsrr1  r0

    /* Discard stack frame */
    la      sp, S_SSC_StFr(sp)

    /* End of software interrupt */
    rfi

#ifdef DEBUG
    /* Error trap: The index of the simple system call is out of range. */
int_ssc_errorSysCallIndexOutOfRange:
    b       .
#endif

    .size   int_simpleSystemCall, .-int_simpleSystemCall
/* End of int_simpleSystemCall */



/**
 *   @func int_systemCallHandler
 * This is the interrupt handler for the CPU interrupt IVOR #8, System Calls. IVOR #8
 * relates to software interrupts intended to invoke operation system function that are
 * executed in supervisor mode.\n
 *   The handler implementation here will save the CPU context onto the stack, read the
 * system function pointer and call this function. It'll then restore the context and
 * return from interrupt.
 *   The handler implementation here will save the CPU context onto the stack and read the
 * system call function pointer and call this function. On return, the function can either
 * simply return or switch to another execution context and return from interrupt in the
 * new context.
 */
    .section .text.ivor
    .globl  int_systemCallHandler
    .extern int_systemCallHandlerAry, int_noSystemCalls
    .type   int_systemCallHandler, @function
    .align  4
int_systemCallHandler:

    /* We provide an execution time efficient support of kernel-irrelated system calls.
       They can be used for operations, which require the supervisor mode but don't compete
       with OS operations like context switches. These operations should not run through
       the priority ceiling protocol code and they should not offer the context switch API.
       To make the operation as efficient as possible we specify the negative system call
       number range to these operations. */
    cmpwi   cr0, r3, 0
    bge     cr0, int_simpleSystemCall

    stwu    sp, -S_SC_StFr(sp)     /* Create stack frame */

    /* A part of CR, cr2-4, is non-volatile. If we don't make use of these
       sub-registers then we don't need to save CR. */

    /* Save SRR0 and SRR1. This needs to be done prior to re-enabling the External
       Interrupt handling. */
    mfsrr0  r0
    stw     r0, O_SRR0(sp)
    mfsrr1  r0
    stw     r0, O_SRR1(sp)

    /* Kernel relevant system calls use negative indexes. We need the positive number for
       address computation. We use the one's complement to map the first negative index,
       -1, on the first positive index, which is 0. */
    not     r3, r3

    stw     r14, O_SC_R14(sp)   /* Save r3 = sys call index for later use */
    mr      r14, r3             /* Do this in a later anyway saved register */

    /* Check index of system call. */
#ifdef DEBUG
    lis     r3, int_noSystemCalls@ha
    lwz     r3, int_noSystemCalls@l(r3)
    cmplw   cr0, r14, r3
    bge     cr0, int_sch_errorSysCallIndexOutOfRange
#endif

    /* The kernel code is not reentrant. We use the INTC's current priority register to
       raise the External Interrupt level such that no kernel relevant interrupt level will
       be served. */
    lis     r3, INTC_CPR_PRC0@ha
#ifdef DEBUG
    lwz     r0, INTC_CPR_PRC0@l(r3)             /* Read current INTC prio */
    cmplwi  cr0, r0, 0                          /* Sys call not allowed from ISR or sys call */
    bne     cr0, int_sch_errorPrioLowered       /* Error if current > 0 */
#endif
    li      r0, INT_MAX_KERNEL_IRQ_PRIO         /* Store highest permitted kernel prio */
    stw     r0, INTC_CPR_PRC0@l(r3)             /* in INTC's current prio register */

    /* We put a memory barrier between the modification of the INTC's priority register and
       the re-enabling of the External Interrupt handling by the CPU. */
    mbar

    /* Relevant MSR bits:
         0x02000000: SPE
         0x00020000: CE
         0x00008000: EE
         0x00004000: PR
         0x00001000: ME
         Note, we must not set the MSR[SPE] bit. The 64 Bit SPE instructions are generally
       unwanted. Once we start using them, we need to save our CPU contexts on interrupts
       as full 64 Bit, although only occasionally used by the compiler. (Not setting SPE
       requires not using -mspe on the compiler command line.)
         The CE and ME (critical and machine check) have not be cleared/altered on entry
       into the handler. There's no reason to lock these higher prior interrupts. At
       minimum there's an empty trap handler, which reports the problem to the SW
       developer. */
    wrteei 1

    /* We will see preemptions even by kernel relevant interrupts (raising CPR doesn't
       un-assert pending interrupts from immediately before). We add an isync to be sure
       that these interrupts are all handled before we execute the code inside the critical
       section. */
    isync

    /* If we get here, the handling of External Interrupts is re-enabled and we can become
       preempted, but only by none kernel relevant interrupts. */

    /* Read ISR address from system call vector table using the index in r14. Use last
       volatile function argument register in order to not overwrite first system call
       argument and to still enable as many arguments as easily possible. */
    slwi    r3, r14, 2
    lwz     r3, int_systemCallHandlerAry(r3)

    /* Branch to system call handler address from vector table. This is the signature of
       such a handler:
         int_retCodeKernelIsr_t sysCallHandler(int_cmdContextSwitch_t *pCmdContextSwitch, ...);
         The returned data has a size of 16 Byte and is placed in the stack. We pass the
       pointer to the C code. */
    mtlr    r3                  /* Put handler address in LR */
    la      r3, O_RET(sp)
    blrl                        /* Branch to handler, but return here */

    /* Disable handling of External Interrupts prior to touching the INTC's priority
       register. */
    wrteei  0
    // Elder NXP samples place a memory barrier here before interacting with the INTC,
    // newer NXP sample toggle the order of wrteei and msync. We believe that no memory
    // barrier is required here. Successful processing of the code down here does not
    // depend on the completion of the loads and stores before and the order of EE=0 and
    // writes to INTC and SSRi is guaranteed by the instruction synchronizing character of
    // wrteei
    /// @todo Intensive testing with commented msync
    msync

    /* Undo priority ceiling, which had been applied in order to enable at least the none
       kernel relevant interrupts during the execution of the system call. */
    lis     r4, INTC_CPR_PRC0@ha
#ifdef DEBUG
    lwz     r0, INTC_CPR_PRC0@l(r4)             /* Read current INTC prio */
    cmplwi  cr0, r0, INT_MAX_KERNEL_IRQ_PRIO
    bne     cr0, int_sch_errorPrioAltered       /* Error if current != kernel level */
#endif
    li      r0, 0                               /* Restore prio to zero */
    stw     r0, INTC_CPR_PRC0@l(r4)

    /* Evaluate returned data to see if we
       - simply return and end the system call
       - suspend this context and resume another, where we
         - return from interrupt
         - return from system call */

    /* The C function return code in r3 is a bit set saying whether (!= 0) or not (== 0) to
       switch the context, whether to terminate the left context (LSB==1) and whether to
       create the context we switch to (MSB==1). */
    cmpwi   cr1, r3, 0
    beq     cr1, int_sch_endSystemCall
#if INT_USE_SHARED_STACKS == 1
    rlwinm. r3, r3, 0, 31, 31   /* Filter bit 0 (terminate) and set CR0 */
#endif

    /* If we get here we need to switch from the system calling context to another
       execution context. This means that we have to save all non-volatile registers. We do
       this in the stack frame of the left context. Note, r14 had already been been saved
       as work register. */
    stmw    r15, O_SC_R15(sp)   /* Write registers 15-31 */

    /* The handler may have demanded to create a new context. */
    blt     cr1, int_sch_startNewContext

    /*  If we get here then we do a context switch to a previously suspended context.
          This label is shared with the IVOR #4 handler. If a kernel interrupt demands a
       context switch then we jump here. Preconditions:
       - The left context is saved
       - External Interrupt handling is disabled
       - the INTC is satisfied (interrupt acknowledged, priority stack cleared)
       - The equal bit of cr0 holds the information "terminate left context"
       - r14 contains the system call index (or -1 if the left context is an IVOR #4) */
int_sch_doContextSwitch:

    /* The actual context switch is the exchange of old and new stack pointer. This needs
       to be done under global lock of all interrupts. The data returned from the system
       call service routine tells where to find the new and where to place the old stack
       pointer. Furthermore, we save for the suspended context that this is a suspension
       due to a system call.
         Our code environment doesn't implement IVOR handlers for Machine Check and
       Critical and Debug Interrupt. The simple failure reporting dummy handlers we have
       for the first two do not depend on whether to use the old or the new stack. So we
       must not take much care for these IVOR interrupts and can selectively disable the
       External Interrupts only.
         @todo The MSR will likely need modification if you use true IVOR handlers for
       these interrupts and if your handlers share the stack with the rest of the code. */

#if DEBUG_CNT_CONTEXT_SWITCHES == 1  && defined(DEBUG)
    mr      r15, r14
#endif
    lwz     r3, O_RET_pSCSD(sp) /* Load pointer to context save data of suspending ctxt. */
#if INT_USE_SHARED_STACKS == 1
    beq     cr0, int_sch_swCtxt_saveCtxt

    /* Context termination: Don't store current sp in save variable but the initial value
       it had at context creation time. This leaves the stack ready to use for other
       contexts, which share the stack. */
    lwz     r14, 12(r3)         /* Load initial value of context's stack pointer */
    lwz     r3, 4(r3)           /* Load address of stack pointer save variable */
    stw     r14, 0(r3)          /* and make stack reusable for the other sharing contexts */
    b       int_sch_swCtxt_endSaveCtxt

int_sch_swCtxt_saveCtxt:
    stw     r14, 0(r3)          /* Store system call index at offset 0 in the struct */
    lwz     r3, 4(r3)           /* Load address of stack pointer save variable. */
    stw     sp, 0(r3)           /* Store current sp of left context */
int_sch_swCtxt_endSaveCtxt:
#else
    stw     r14, 0(r3)          /* Store system call index at offset 0 in the struct */
    stw     sp, 4(r3)           /* Store current sp of left context at 4 in the struct */
#endif

    lwz     r3, O_RET_RC(sp) /* Load result of sys call now ending in the resumed context. */
    lwz     r14, O_RET_pRCSD(sp)/* Load pointer to context save data of resumed context */
    lwz     sp, 4(r14)          /* Load sp (or address of) from descriptor of resumed ctxt */
#if INT_USE_SHARED_STACKS == 1
    lwz     sp, 0(sp)           /* Load stack pointer from its save variable. */
#endif

    /* The next decision is whether to return in the resumed context from a system call or
       from an asynchronous External Interrupt. This information had been stored in the
       context save data. */
    lwz     r14, 0(r14)
    cmpwi   cr0, r14, 0         /* Negative means ISR, >=0 means system call index */
    blt     cr0, int_sch_retFromIvor4

    /* If we get here the context switch ends with the return from a system call. We
       only restore the non-volatile registers. From the perspective of the calling code
       all other registers may be changed by the ended function. */

#if DEBUG_CNT_CONTEXT_SWITCHES == 1  && defined(DEBUG)
    cmpwi   cr0, r15, 0         /* Test flag: return from ISR of system call */
    blt     cr0, int_sch_cntIsrToSc     /* -1: we return from ISR */

    lwz     r15, int_cntScToSc@sda21(%r0)
    addi    r15, r15, 1
    stw     r15, int_cntScToSc@sda21(%r0)
    b       int_sch_endCntIsrToSc

int_sch_cntIsrToSc:
    lwz     r15, int_cntIsrToSc@sda21(%r0)
    addi    r15, r15, 1
    stw     r15, int_cntIsrToSc@sda21(%r0)

int_sch_endCntIsrToSc:
#endif

    lmw     r14, O_SC_R14(sp)

    /* The wanted return value of the function we return from had been returned by the
       system call handler and is now in r3. */

    /* Restore SRR0 and SRR1 from the new context in order to continue it with an rfi
       instruction. */
    lwz     r0, O_SRR0(sp)      /* Restore SRR0 */
    mtsrr0  r0
    lwz     r0, O_SRR1(sp)      /* Restore SRR1 */
    mtsrr1  r0

    /* Discard stack frame */
    la      sp, S_SC_StFr(sp)

    /* End of interrupt */
    rfi

int_sch_retFromIvor4:

    /* If we get here the context switch is the return from a context, which had been
       pre-empted by an asynchronous, external Interrupt. We can ignore the system call
       result value got from the system call handler but need to restore all registers. */

#if DEBUG_CNT_CONTEXT_SWITCHES == 1  && defined(DEBUG)
    cmpwi   cr0, r15, 0         /* Test flag: return from ISR or system call */
    blt     cr0, int_sch_cntIsrToIsr    /* -1: we return from ISR */

    lwz     r15, int_cntScToIsr@sda21(%r0)
    addi    r15, r15, 1
    stw     r15, int_cntScToIsr@sda21(%r0)
    b       int_sch_endCntIsrToIsr

int_sch_cntIsrToIsr:
    lwz     r15, int_cntIsrToIsr@sda21(%r0)
    addi    r15, r15, 1
    stw     r15, int_cntIsrToIsr@sda21(%r0)

int_sch_endCntIsrToIsr:
#endif

    /* Pre-emptable ISRs require to restore the current priority in the INTC. This step is
       required only in the rare situation when a kernel relevant interrupt had preempted
       a system call but it doesn't harm in all other cases. */
    lwz     r0, O_I_CPR(sp)
    lis     r3, INTC_CPR_PRC0@ha
    stw     r0, INTC_CPR_PRC0@l(r3)

    lwz     r0, O_I_CR(sp)      /* Restore CR */
    mtcr    r0
    lwz     r0, O_I_LR(sp)      /* Restore LR */
    mtlr    r0
    lwz     r0, O_I_CTR(sp)     /* Restore CTR */
    mtctr   r0
    lwz     r0, O_I_XER(sp)     /* Restore XER */
    mtxer   r0

    lmw     r3, O_I_R03(sp)     /* Restore GPR 3 till 31 */

    /* Retrieve SRR0 and SRR1 */
    lwz     r0, O_SRR0(sp)      /* Restore SRR0 */
    mtsrr0  r0
    lwz     r0, O_SRR1(sp)      /* Restore SRR1 */
    mtsrr1  r0

    /* Restore remaining working register r0 */
    lwz     r0, O_I_R00(sp)     /* Restore r0 */

    /* Discard stack frame */
    la      sp, S_I_StFr(sp)

    /* End of interrupt */
    rfi


    /* If we get here, we create a new context on the fly and switch to it.
         This label is shared with the IVOR #4 handler. If a kernel interrupt demands to
       create a new context then we jump here, too. Preconditions:
       - The context switch is done
       - External Interrupt handling is disabled
       - the INTC is satisfied (interrupt acknowledged, priority stack cleared)
       - The equal bit of cr0 holds the information "terminate left context"
       - r14 contains the system call index (or -1 if the left context is an IVOR #4) */
int_sch_startNewContext:
    lwz     r3, O_RET_pSCSD(sp) /* Load pointer to context save data of suspending ctxt. */
#if INT_USE_SHARED_STACKS == 1
    beq     cr0, int_sch_newCtxt_saveCtxt

    /* Context termination: Don't store current sp in save variable but the initial value
       it had at context creation time. This leaves the stack ready to use for other
       contexts, which share the stack. */
    lwz     r15, 12(r3)         /* Load initial value of context's stack pointer */
    lwz     r3, 4(r3)           /* Load address of stack pointer save variable. */
    stw     r15, 0(r3)          /* and make stack reusable for sharing contexts */
    b       int_sch_newCtxt_endSaveCtxt

int_sch_newCtxt_saveCtxt:
    stw     r14, 0(r3)          /* Store system call index at offset 0 in the struct */
    lwz     r3, 4(r3)           /* Load address of stack pointer save variable. */
    stw     sp, 0(r3)           /* Store current sp of left context */
int_sch_newCtxt_endSaveCtxt:
#else
    stw     r14, 0(r3)          /* Store system call index at offset 4 in the struct */
    stw     sp, 4(r3)           /* Store current sp of left context at 0 in the struct */
#endif

#if DEBUG_CNT_CONTEXT_SWITCHES == 1  && defined(DEBUG)
    cmpwi   cr0, r14, 0         /* Test flag: return from ISR or system call */
    blt     cr0, int_sch_cntIsrToNewCtxt     /* -1: we return from ISR */

    lwz     r14, int_cntScToNewCtxt@sda21(%r0)
    addi    r14, r14, 1
    stw     r14, int_cntScToNewCtxt@sda21(%r0)
    b       int_sch_endCntIsrToNewCtxt

int_sch_cntIsrToNewCtxt:
    lwz     r14, int_cntIsrToNewCtxt@sda21(%r0)
    addi    r14, r14, 1
    stw     r14, int_cntIsrToNewCtxt@sda21(%r0)

int_sch_endCntIsrToNewCtxt:
#endif

    lwz     r14, O_RET_pRCSD(sp)/* Load pointer to context save data of new context */
#if INT_USE_SHARED_STACKS == 1
    lwz     r3, 16(14)          /* Load entry function of new context into R3 */
#else
    lwz     r3, 8(14)           /* Load entry function of new context into R3 */
#endif
    mtsrr0  r3                  /* Store entry function of new context for rfi in SRR0 */
#if INT_USE_SHARED_STACKS == 1
    lbz     r3, 20(14)          /* Load field priviledgedMode */
#else
    lbz     r3, 12(14)          /* Load field priviledgedMode */
#endif
    cmplwi  cr0, r3, 0          /* Load Bit priviledgedMode into CR0, positive */
    lwz     r3, O_RET_RC(sp)    /* Load signal from ISR, is function arg of new context */
    lwz     sp, 4(r14)          /* Load address of stack pointer save variable */
#if INT_USE_SHARED_STACKS == 1
    lwz     sp, 0(sp)           /* Load stack pointer from its save variable */
    stw     sp, 12(r14)         /* Save initial stack pointer for later context termination */
#endif

    /* Stack pointer has been exchanged (but can still be the same if both save area
       descriptors reference the same stack pointer save variable), srr0 is prepared for
       entry function call, r3 holds function argument. We just need to prepare the stack
       and some registers. */
    stwu    sp, -8(sp)          /* Leave room for link, consider 8 Byte alignment */
    lis     r14, 0x00029000@h   /* MSR: External, critical and machine check */
    bgt     cr0, int_sch_privilegedMode

    ori     r14, r14, 0x0002d000@l  /* MSR: Plus user mode */
    b       int_sch_endPrivilegedMode
int_sch_privilegedMode:
    ori     r14, r14, 0x00029000@l  /* MSR: Plus supervisor mode */
int_sch_endPrivilegedMode:

    mtsrr1  r14
    lis     r14, int_fctOnContextEnd@ha     /* Load function, where we jump to on context */
    la      r14, int_fctOnContextEnd@l(r14) /* termination into LR */
    mtlr    r14

    /* End of interrupt */
    rfi


int_sch_endSystemCall:
    /* If we get here then we simply return from system call. */
#if DEBUG_CNT_CONTEXT_SWITCHES == 1  && defined(DEBUG)
    lwz     r3, int_cntRetFromSc@sda21(%r0)
    addi    r3, r3, 1
    stw     r3, int_cntRetFromSc@sda21(%r0)
#endif

    /* Load system call result into r3 and restore non-volatile r14. */
    lwz     r3, O_RET_RC(sp)
    lwz     r14, O_SC_R14(sp)

    /* Restore SRR0 and SRR1 to their value on interrupt entry. */
    lwz     r0, O_SRR0(sp)      /* Prepare PC to go to */
    mtsrr0  r0
    lwz     r0, O_SRR1(sp)      /* Prepare value of MSR after return */
    mtsrr1  r0

    /* Discard stack frame */
    la      sp, S_SC_StFr(sp)

    /* End of software interrupt */
    rfi

#ifdef DEBUG
    /* Error trap: The (negative) index of the kernel relevant system call is out of
       range. */
int_sch_errorSysCallIndexOutOfRange:
    b       .
#endif

    /* Error trap: The priority ceiling applied to protect the execution of system calls
       without interference of interrupts had failed. On exit from the system call the
       current priority had been unexpectedly altered. */
int_sch_errorPrioAltered:
    b       .

    /* Error trap: The current interrupt priority on entry into a system was found to be
       positive, while 0 is expected. This can happen if a system call is done from an ISR
       context, which is strictly forbidden. */
int_sch_errorPrioLowered:
    b       .

    .size   int_systemCallHandler, .-int_systemCallHandler
/* End of int_systemCallHandler */


/**
 *   @func int_fctOnContextEnd
 * Default implementation of guard function, which is invoked when a context entry function
 * is left with return. The default function is meant an assertion, it's just spinning
 * forever.\n
 *   The function is declared weak so that the application code can overload it.
 */
    .section .text
    .globl  int_fctOnContextEnd
    .weak   int_fctOnContextEnd
    .type   int_fctOnContextEnd, @function
    .align  2
int_fctOnContextEnd:
    wrteei  0
    b       .

    .size   int_fctOnContextEnd, .-int_fctOnContextEnd
/* End of int_fctOnContextEnd */


/**
 *   @func int_systemCall
 * System call; entry point into operating system function for user code.
 */
    .section .text
    .globl  int_systemCall
    .type   int_systemCall, @function
    .align  2
#define SIZE_OF_STACKFRAME  8

int_systemCall:
    stwu    sp, -SIZE_OF_STACKFRAME(sp)
    mflr    r0
    stw     r0, (SIZE_OF_STACKFRAME+4)(sp)

    /* Raise a software interrupt. The contents of the GPR are not changed, i.e. all the
       (eight supported) function arguments in GPR 3..10 go to the system call
       implementation. */
    sc

    /* Destroy stack frame and return. */
    lwz     r0, (SIZE_OF_STACKFRAME+4)(sp)
    mtlr    r0
    la      sp, SIZE_OF_STACKFRAME(sp)
    blr

#undef SIZE_OF_STACKFRAME
    .size   int_systemCall, .-int_systemCall
/* End of int_systemCall */

#endif /* !defined(VLE), i.e. Book E */
