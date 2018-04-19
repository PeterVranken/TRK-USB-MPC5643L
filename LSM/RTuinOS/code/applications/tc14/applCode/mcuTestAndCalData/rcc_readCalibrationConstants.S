/**
 * @file rcc_readCalibrationConstants.S
 * Read the calibration constants of the MPC5643L into RAM. The operation needs to be
 * implemented in assembler since normal operation of the MCU is temporarily impossible;
 * the instruction flash is not accessible during the operation. We need to prepare some
 * location independent code, copy it to RAM and execute it there.
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
 * Local functions
 */

/*
 * Include files
 */


/*
 * Defines
 */

/** The base address of device System Status and Configuration Module, SSCM. */
#define SSCM    0xc3fd8000

/** The address of register SCTR in the SSCM. This register is used to switch the flash ROM
    from normal instruction ROM to test and calibration data. */
#define SCTR    ((SSCM)+0x24)


/*
 * External function declarations
 */


/*
 * Data declarations
 */


/*
 * Function implementation
 */


/**
 *   @func copyData
 * This is the location independent implementation of the copy data operation from adress 0
 * to the destination address in r3.
 *   @param pDest
 * r3 holds the target address of the operation. The MCU calibration data is placed there.
 * It needs to be 16 Bit aligned.
 *   @param noUInt16Words
 * r4 holds the non-zero number of 16 Bit words to copy. (The test and calibration data is
 * organized in words of this size.)
 *   @remark
 * This function can be excuted exactely once per operation cycle. The flash ROM can not
 * be switched from normal operation to test data content more than once after reset. The
 * behavior is undefined for subsequent calls of this function.
 */
    .type   copyData, @function
    .align 3
copyData:

#ifdef __VLE__
    /* Switch the flash ROM to show the MCU test or calibration data. The data is a 4k
       page, which is shown at every multiple of 4k. */
    e_lis   r5, SCTR@ha
    se_li   r6, 4           /* Test flash enable is the only used bit in SCTR, no
                               read-modify-write required to set it. */
    e_stw   r6, SCTR@l(r5)  /* From now on, we don't see an instruction ROM any more. */

    /* Copy the data to the target RAM address. */
    mtctr   r4              /* Load count register with the number of words to copy. */
    se_li   r6, 0           /* Load source address pointer. */
cd_loopCopyByte:
    se_lhz  r7, 0(r6)
    se_sth  r7, 0(r3)
    e_la    r6, 2(r6)       /* Advance source pointer. */
    e_la    r3, 2(r3)       /* Advance destination pointer. */
    e_bdnz  cd_loopCopyByte /* Branch on counter register not zero */
    
    /* Restore the normal flash operation. r5 still holds the register pointer. */
    se_li   r6, 0           /* Bit TFE back to zero. */
    e_stw   r6, SCTR@l(r5)  /* From now on, we see the instruction ROM forever. */

    se_blr
#else
    /* Switch the flash ROM to show the MCU test or calibration data. The data is a 4k
       page, which is shown at every multiple of 4k. */
    lis     r5, SCTR@ha
    li      r6, 4           /* Test flash enable is the only used bit in SCTR, no
                               read-modify-write required to set it. */
    stw     r6, SCTR@l(r5)  /* From now on, we don't see an instruction ROM any more. */

    /* Copy the data to the target RAM address. */
    mtctr   r4              /* Load count register with the number of words to copy. */
    li      r6, 0           /* Load source address pointer. */
cd_loopCopyByte:
    lhz     r7, 0(r6)
    sth     r7, 0(r3)
    la      r6, 2(r6)       /* Advance source pointer. */
    la      r3, 2(r3)       /* Advance destination pointer. */
    bdnz    cd_loopCopyByte /* Branch on counter register not zero */
    
    /* Restore the normal flash operation. r5 still holds the register pointer. */
    li      r6, 0           /* Bit TFE back to zero. */
    stw     r6, SCTR@l(r5)  /* From now on, we see the instruction ROM forever. */

    blr
#endif    

    /* We put a label at the end of the routine in order to know its size and to be able to
       copy the entire routine to RAM. Using an 8 Byte alignment for start and end helps
       meeting the EABI constraint for stack sizes. */
    .align 3
endOfCopyData:

    .size   copyData, .-copyData
/* End of copyData */



/**
 *   @func
 * Copy the MCU's individual test and calibration data into RAM.
 *   @param pDest
 * r3 holds the target address of the operation. It needs to be a valid, 16 Bit aligned RAM
 * address. The MCU calibration data is placed there.
 *   @param noUInt16Words
 * r4 holds the number of 16 Bit words to copy. (The test and calibration data is organized
 * in words of this size.)
 *   @remark
 * This function can be excuted exactely once per operation cycle. The flash ROM can not
 * be switched from normal operation to test data content more than once after reset. The
 * behavior is undefined for subsequent calls of this function.
 */
    .section .text
    .global rcc_readTestData
    .type   rcc_readTestData, @function
    
#define SIZE_OF_STACKFRAME  (8+endOfCopyData-copyData)
#define OFFSET_CODE         8

rcc_readTestData:

#ifdef __VLE__
    e_cmplwi cr0, r4, 0     /* Skip operation if user says zero words. */
    se_ble  rtd_nop
    
    e_stwu  sp, -SIZE_OF_STACKFRAME(sp)
    mflr    r0
    e_stw   r0, (SIZE_OF_STACKFRAME+4)(sp)
    
    /* Copy the code of ROM copy routine into the stack frame. */
    e_li    r5, (endOfCopyData-copyData)>>2
    mtctr   r5              /* Load count register with number of words to copy. */
    e_lis   r5, copyData@ha
    e_la    r5, copyData@l(r5)  /* Load r5 with source address of code. */
    e_la    r6, OFFSET_CODE(sp) /* Load r6 with destination address of code. */
    mtlr    r6                  /* Later we are going to branch there. */
rtd_loopCopyWord:
    se_lwz  r7, 0(r5)
    se_stw  r7, 0(r6)
    e_la    r5, 4(r5)       /* Advance source pointer. */
    e_la    r6, 4(r6)       /* Advance destination pointer. */
    e_bdnz  rtd_loopCopyWord   /* Branch on counter register not zero */
    
    /* Call the mem-copy routine in RAM. */
    se_blrl
    
    /* Destroy stack frame and return. */
    e_lwz   r0, (SIZE_OF_STACKFRAME+4)(sp)
    mtlr    r0
    e_la    sp, SIZE_OF_STACKFRAME(sp)
rtd_nop:
    se_blr

#else
    cmplwi  cr0, r4, 0      /* Skip operation if user says zero words. */
    ble     rtd_nop
    
    stwu    sp, -SIZE_OF_STACKFRAME(sp)
    mflr    r0
    stw     r0, (SIZE_OF_STACKFRAME+4)(sp)
    
    /* Copy the code of ROM copy routine into the stack frame. */
    li      r5, (endOfCopyData-copyData)>>2
    mtctr   r5              /* Load count register with number of words to copy. */
    lis     r5, copyData@ha
    la      r5, copyData@l(r5)  /* Load r5 with source address of code. */
    la      r6, OFFSET_CODE(sp) /* Load r6 with destination address of code. */
    mtlr    r6                  /* Later we are going to branch there. */
rtd_loopCopyWord:
    lwz     r7, 0(r5)
    stw     r7, 0(r6)
    la      r5, 4(r5)       /* Advance source pointer. */
    la      r6, 4(r6)       /* Advance destination pointer. */
    bdnz    rtd_loopCopyWord    /* Branch on counter register not zero */
    
    /* Call the mem-copy routine in RAM. */
    blrl
    
    /* Destroy stack frame and return. */
    lwz     r0, (SIZE_OF_STACKFRAME+4)(sp)
    mtlr    r0
    la      sp, SIZE_OF_STACKFRAME(sp)
rtd_nop:
    blr
#endif

#undef SIZE_OF_STACKFRAME
    .size   rcc_readTestData, .-rcc_readTestData
/* End of rcc_readTestData */



