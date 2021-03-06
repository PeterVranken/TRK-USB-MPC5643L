/**
 * @file tcx_testContext.S
 * Test file: A task that sets most of the CPU user registers and later checks the contents
 * to see if they are still okay after a context switch.
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
 *   tcx_testContext
 * Local functions
 *   fillRegisters
 *   testRegisters
 */

#ifndef __VLE__

/*
 * Include files
 */


/*
 * Defines
 */
 
/** The enumeration of system call indexes.\n
      Caution, this enumeration needs to be always in sync with the table of function
    pointers!
      @todo This enumeration is a copy from the C header file sc_systemCalls.h. If this
    were not only a small test we would have to split the C header and separate this
    information into a sub-header, which is compatible with the assembler syntax. For now,
    keep this table in sync with the C header! */
#define SC_IDX_SYS_CALL_TEST_AND_DECREMENT  0
#define SC_IDX_SYS_CALL_INCREMENT           1
#define SC_IDX_SYS_CALL_SWITCH_CONTEXT      2

/** The number of system calls. */
#define SC_NO_SYSTEM_CALLS                  3


/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
 
/*
 * Data definitions
 */
 
    .section .sdata
    .global tcx_cntTestLoops
tcx_cntTestLoops:
    .dc.l   0
 
 
/*
 * Function implementation
 */


/**
 *   @func fillRegisters
 * Fill all accessible registers with the value in r3.
 */
    .section .text
    .type   fillRegisters, @function
    .align  2
#define SIZE_OF_STACKFRAME  0

fillRegisters:
    mr      r0, r3
    addi    r3, r3, 1
    mtxer   r3
    addi    r3, r3, 1
    mtctr   r3
    addi    r3, r3, 1
    mr      r5, r3
    addi    r3, r3, 1
    mr      r6, r3
    addi    r3, r3, 1
    mr      r7, r3
    addi    r3, r3, 1
    mr      r8, r3
    addi    r3, r3, 1
    mr      r9, r3
    addi    r3, r3, 1
    mr      r10, r3
    addi    r3, r3, 1
    mr      r11, r3
    addi    r3, r3, 1
    mr      r12, r3
    addi    r3, r3, 1
    mr      r14, r3
    addi    r3, r3, 1
    mr      r15, r3
    addi    r3, r3, 1
    mr      r16, r3
    addi    r3, r3, 1
    mr      r17, r3
    addi    r3, r3, 1
    mr      r18, r3
    addi    r3, r3, 1
    mr      r19, r3
    addi    r3, r3, 1
    mr      r20, r3
    addi    r3, r3, 1
    mr      r21, r3
    addi    r3, r3, 1
    mr      r22, r3
    addi    r3, r3, 1
    mr      r23, r3
    addi    r3, r3, 1
    mr      r24, r3
    addi    r3, r3, 1
    mr      r25, r3
    addi    r3, r3, 1
    mr      r26, r3
    addi    r3, r3, 1
    mr      r27, r3
    addi    r3, r3, 1
    mr      r28, r3
    addi    r3, r3, 1
    mr      r29, r3
    addi    r3, r3, 1
    mr      r30, r3
    addi    r3, r3, 1
    mr      r31, r3
    addi    r3, r3, 1
    
    blr

    .size   fillRegisters, .-fillRegisters
/* End of fillRegisters */




/**
 *   @func testRegisters
 * Compare all accessible registers with the value in r3. Any deviation is reported as an
 * error. Error reporting is done by entering an infinit loop.
 */
    .section .text
    .globl  tcx_testFailed
    .type   testRegisters, @function
    .align  2
#define SIZE_OF_STACKFRAME  0

testRegisters:
    cmplw   cr0, r0, r3
    bne     tcx_testFailed
    addi    r3, r3, 1
    
    mfxer   r0
    xor     r0, r0, r3
    rlwinm. r0, r0, 0, 25, 2    /* Mask all bits in use (0xe000007f) and compare with zero */
    bne     tcx_testFailed
    addi    r3, r3, 1

    mfctr   r0
    cmplw   cr0, r0, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r5, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r6, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r7, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r8, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r9, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r10, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r11, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r12, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r14, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r15, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r16, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r17, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r18, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r19, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r20, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r21, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r22, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r23, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r24, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r25, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r26, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r27, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r28, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r29, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r30, r3
    bne     tcx_testFailed
    addi    r3, r3, 1

    cmplw   cr0, r31, r3
    bne     tcx_testFailed
    addi    r3, r3, 1
    
    blr

tcx_testFailed:
    wrteei  0               /* LED stops blinking and makes failure apparent */
    b       tcx_testFailed

#undef SIZE_OF_STACKFRAME
    .size   testRegisters, .-testRegisters
/* End of testRegisters */




/**
 *   @func tcx_testContext
 * This function writes specific values to most accessible registers of the CPU, does busy
 * wait for a while - to let the scheduler make a context switch - and checks the register
 * contents. This is done in an endless loop, the function doesn't return.
 */
    .section .text
    .extern int_systemCall
    .globl  tcx_testContext
    .type   tcx_testContext, @function
    .align  2
#define SIZE_OF_STACKFRAME  16
#define O_R3                8
tcx_testContext:
    stwu    sp, -SIZE_OF_STACKFRAME(sp)
    mflr    r0
    stw     r0, (SIZE_OF_STACKFRAME+4)(sp)
    
    /* To give time for a context switch we use del_delayMicroseconds as an external
       funtion. It clobbers the volatile GPR 3,4, the LR and CR7. These registers must
       therefore not be tested. We can use them as work registers, too. */
    lis     r3, 0xffff0000@h    /* Because of upper XER bits we don't start with 0 */
    stw     r3, O_R3(sp)
    
tcx_endlessLoop:
    lwz     r3, O_R3(sp)
    bl      fillRegisters
    
    lis     r3, 10000@h
    ori     r3, r3, 10000@l
    bl      del_delayMicroseconds

    lwz     r3, O_R3(sp)
    bl      testRegisters
    stw     r3, O_R3(sp)
    
    /* Do a system call. We use a quite useless one which always returns without demanding
       a context switch. */
    li      r3, SC_IDX_SYS_CALL_TEST_AND_DECREMENT  /* idxSysCall */
    li      r4, 1       /* idxSemaphore. Don't use 0, which is in use for LED access control */
    bl      int_systemCall
    
    /* Now do a system call, which doesn't return immediately, but which demands a context
       switch. */
    li      r3, SC_IDX_SYS_CALL_SWITCH_CONTEXT  /* idxSysCall: cxs_switchContext */
    lis     r4, -2@h    /* signalToResumedContext */
    ori     r4, r4, -2@l/* signalToResumedContext */
    bl      int_systemCall
    
    /* Cycle completed, ok, count it as success report. */
    lwz     r3, tcx_cntTestLoops@sda21(%r0)
    addi    r3, r3, 1
    stw     r3, tcx_cntTestLoops@sda21(%r0)
    
    b       tcx_endlessLoop
    
    /* Destroy stack frame and return. */
    lwz     r0, (SIZE_OF_STACKFRAME+4)(sp)
    mtlr    r0
    la      sp, SIZE_OF_STACKFRAME(sp)
    blr

#undef O_R3
#undef SIZE_OF_STACKFRAME
    .size   tcx_testContext, .-tcx_testContext
/* End of tcx_testContext */

#endif // Book E only
