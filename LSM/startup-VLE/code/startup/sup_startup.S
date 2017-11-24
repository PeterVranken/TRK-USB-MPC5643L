/**
 * @file sup_startup.S
 * Book E assembler implementation of startup code for MPC5643L in Lockstep Mode (LSM). The
 * code has been designed for use with GCC for PowerPC EABI.
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
 *   sub_startup
 *   __eabi
 *   sup_IVOR0trap .. sup_IVOR15trap
 *   sup_IVOR32trap .. sup_IVOR34trap
 * Local functions
 *   zeroizeRegisters
 *   initExceptionHandlers
 *   initMMU
 *   initTimeBase
 *   zeroizeSRAM
 *   disableWatchdog
 *   initFlash
 */

#ifndef __VLE__
# error This file is intended for compilation for VLE instruction set only
#endif

/*
 * Include files
 */

#include "sup_settings.h"
#include "mmu_mmuRegions.h"


/*
 * Defines
 */

/* The definition of the boot sector (at 0x00000000). The data contents are defined here
   and the location of the boot sector is defined in the linker control file.
     See MCU reference manual, 5.3, p.126ff */
#define RCHW_VLE        0x0100      /* Optional: Enable VLE instruction set */
#define RCHW_BOOT_ID    0x005A      /* Fixed, hard-coded value */
#define RCHW_VALUE      (RCHW_VLE|RCHW_BOOT_ID)


/*
 * External function declarations
 */

.extern main

/*
 * Data declarations
 */

/** Linker provided symbol, which has the address of the end of the stack area
    (effectively the bottom of the stack, it grows downwards). */
.extern ld_memStackStart

/** Linker provided symbols that hold end of stack area and its size in Byte. */
.extern ld_memStackEnd, ld_memStackSize

/** Linker provided symbols, which indicate where initialized data begins and ends in RAM
    and where its initial data is located in ROM. */
.extern ld_dataStart, ld_dataEnd, ld_dataMirrorStart


/*
 * Function implementation
 */


/**
 * The boot sector contents are defined as first element of sector .boot.
 */
    .section .bamData, "a"
    .global sup_bamData         /* Nice to have this in the map file */ 
sup_bamData:
    .dc.w RCHW_VALUE, 0
    .dc.l sup_startUp
    .align  4


/**
 *   @func sup_startUp
 * Reset address: All machine code execution starts here. The label sup_startUp is
 * specified in the BAM data block in the boot sector to make this happen after reset.
 */
    .section .boot, "a"
    .global sup_startUp
    .type   sup_startUp, @function
sup_startUp:
    e_bl    zeroizeRegisters        /* Set MSR[SPE], clear all GPR and all relevant SPR */
    e_bl    initExceptionHandlers   /* Set all IVOR registers of the MCU */
    e_bl    initMMU                 /* Make MMU allow all memory accesses */
    
    /* Exception handlers are installed, memory is accessible - there's no reason to wait
       any longer with enabling the higher prior interrupts (critical and machine
       check). We have the minimum functionality of an empty trap handler, which reports
       the problem to the SW developer.
         Note, the external interrupts are not enabled yet. They are entirely managed by
       the application software. */
    mfmsr   r0
    e_or2is r0, 0x0002              /* CE: Critical interrupts */
    e_or2i  r0, 0x1000              /* ME: Machine check */
    mtmsr   r0

    e_bl    initTimeBase            /* Start the MCU's high resolution counter */
    e_bl    zeroizeSRAM             /* Clear all RAM */

    /* The watchdog is disabled now. Most available sample code code does do this as C
       function at the beginning of main, however, if we are in lockstep mode and use the
       full available RAM of 128k then it takes much too long to copy the data section and
       to zeroize the bss section and the watchdog bites. */
    e_bl      disableWatchdog

   /* The initial setting of the stack pointer. The stack rises towards lower addresses so
      the initial value is the end of the reserved RAM area. The value is provided by the
      linker, which is in charge to do the reservation but 16 Bytes are subtracted in order
      to implements some EABI requirements concerning the bottom of the stack.
        Note, there are some strict alignment constraints (16 Byte aligned values needed),
      which need to be ensured in the linker script file. They are not checked here and
      will in case cause an exception. */
    e_lis   sp, (ld_memStackEnd-16)@h
    e_or2i  sp, (ld_memStackEnd-16)@l

    /* Select appropriate conditions for the operation of the flash (waitstates, caching,
       etc.) It is essential to do this prior to rising the CPU clock rate by configuring
       the PLL. */
    e_bl    initFlash

    /* Fill the stack with a byte pattern. This pattern is used later to measure the
       current stack usage. */
    e_lis   r8, ld_memStackStart@h
    e_or2i  r8, ld_memStackStart@l
    e_lis   r9, ld_memStackSize@h
    e_or2i  r9, ld_memStackSize@l
    srawi   r9, r9, 0x2   /* Divide length in Byte by 2^2=4 */
    mtctr   r9
    e_lis   r9, 0xa5a5a5a5@h    /* Load pattern into r9 */
    e_or2i  r9, 0xa5a5a5a5@l
su_loopFillStack:
    e_stw   r9, 0(r8)           /* Write the four pattern bytes in r9 */
    e_la    r8, 4(r8)           /* Increment write pointer */
    e_bdnz  su_loopFillStack    /* Branch on counter register not zero */

    /* Taken from NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file __start.c, l. 186ff:
       "Prepare a terminating stack record."
         Note, we'd set the stack pointer 16 Bytes below the end of the RAM area, so we
       have enough head room to do so.
         @todo To be check if we misunderstood something. The standard stack frame of the
       entry into main will already overwrite the word 4(sp) with the saved link register
       and our -1 has no effect. */
    e_lis   r8, 0              /* Clear r8. */
    e_stw   r8, 0(sp)          /* SysVr4 EABI Supp indicated that initial back chain word
                                  should be null. */
    e_addi  r8, r8, -1         /* Load 0xffffffff into r8. */
    e_stw   r8, 4(sp)          /* Make an illegal return address of 0xffffffff.  */
    
    /* Copy the mirror of the initialized RAM sections from flash ROM to the RAM - all
       global C data will have the wanted initial values. */
    e_lis   r8, ld_dataMirrorStart@h    /* r8 points into ROM mirror of RAM, moving */
    e_or2i  r8, ld_dataMirrorStart@l
    e_lis   r9, ld_dataEnd@h            /* r9 is const pointer to the end of the RAM */
    e_or2i  r9, ld_dataEnd@l
    e_lis   r10, ld_dataStart@h         /* r10 is moving pointer to RAM write location */
    e_or2i  r10, ld_dataStart@l

    cmplw   cr0, r9, r10        /* The .data section may by empty */
    e_beq   su_noInitData

su_loopInitData:
    e_lwz   r11, 0(r8)          /* Move next word from ROM to .data section in RAM */
    e_la    r8, 4(r8)
    e_stw   r11, 0(r10)
    e_la    r10, 4(r10)
    cmplw   cr0, r10, r9
    e_blt   su_loopInitData
    msync                       /* Complete the copy operation */
su_noInitData:

    /* GCC calls a routine, that is expected to prepare the eabi register configuration as
       a callback on entry into main. Other compilers don't do so. For those, we call the
       routine directly prior to main. */
#ifndef __GNUC__
    e_bl    __eabi
#endif
    /* Calling main: Load EABI function argument registers with argc=0, argv=NULL. */
    se_li   r3, 0
    se_mr   r4, r3
    e_bl    main

    /* We enter an infinite loop if someone should try to leave main - which is not
       intended. The external interrupts are disables so that possible RTOS code would not
       make some other portions of code become active any more. */
    wrteei  0
su_waitForever:
    e_b     su_waitForever

    .size   sup_startUp, .-sup_startUp
/* End of sup_startUp */



/**
 *   @func zeroizeRegisters
 * First step of MCU startup is to bring all relevant registers into a
 * defined state. We set all of them to zero. The a bit hidden 64 Bit
 * character of the general purpose registers is considered, the never
 * used upper 32 Bit are cleared.
 *   @remark This code is based on NXP sample
 * MPC5643L-LINFlex-UART-DMA-CW210, file MPC5643L_LSM_Startup.c, l. 24ff
 */
    .type   zeroizeRegisters, @function
zeroizeRegisters:

    /* Set the MSR[SPE] bit so code can use "evmergelo" instruction. */
    e_lis   r1, 0
    e_or2is r1, 0x0200
    mtmsr   r1
    se_isync

    /* Initialize the 64-bits of GPR0 to 0 */
    e_lis     r1, 0         /* Initialize the lower 32-bits of GPR1 to 0. */
    evmergelo r0, r1, r1    /* Write the lower 32-bits of GPR1 into both halfs of GPR0. */
    evmergelo r1, r0, r0    /* Clear 64 Bit of r1, r2 ... */
    evmergelo r2 ,r0 ,r0
    evmergelo r3 ,r0 ,r0
    evmergelo r4, r0, r0
    evmergelo r5, r0, r0
    evmergelo r6, r0, r0
    evmergelo r7, r0, r0
    evmergelo r8, r0, r0
    evmergelo r9, r0, r0
    evmergelo r10, r0, r0
    evmergelo r11, r0, r0
    evmergelo r12, r0, r0
    evmergelo r13, r0, r0
    evmergelo r14, r0, r0
    evmergelo r15, r0, r0
    evmergelo r16, r0, r0
    evmergelo r17, r0, r0
    evmergelo r18, r0, r0
    evmergelo r19, r0, r0
    evmergelo r20, r0, r0
    evmergelo r21, r0, r0
    evmergelo r22, r0, r0
    evmergelo r23, r0, r0
    evmergelo r24, r0, r0
    evmergelo r25, r0, r0
    evmergelo r26, r0, r0
    evmergelo r27, r0, r0
    evmergelo r28, r0, r0
    evmergelo r29, r0, r0
    evmergelo r30, r0, r0
    evmergelo r31, r0, r0   /* ... r31 */

    mtcr    r29

    mtspr   284, r29    /* TBL     */
    mtspr   285, r29    /* TBU     */
    mtspr   272, r29    /* SPRG0-7 */
    mtspr   273, r29
    mtspr   274, r29
    mtspr   275, r29
    mtspr   276, r29
    mtspr   277, r29
    mtspr   278, r29
    mtspr   279, r29
    mtspr   604, r29    /* SPRG8-9 */
    mtspr   605, r29
    mtspr   26, r29     /* SRR0-1  */
    mtspr   27, r29
    mtspr   58, r29     /* CSRR0-1 */
    mtspr   59, r29
    mtspr   63, r29     /* IVPR    */
    mtspr   61, r29     /* DEAR    */
    mtspr   62, r29     /* ESR     */
    mtspr   570, r29    /* MCSRR0  */
    mtspr   571, r29    /* MCSRR1  */
    mtspr   340, r29    /* TCR     */
    mtspr   512, r29    /* SPEFSCR */
    mtspr   1, r29      /* XER     */
    mtspr   256, r29    /* USPRG0  */
    mtspr   9, r29      /* CTR     */
    mtspr   308, r29    /* DBCR0-6 */
    mtspr   309, r29
    mtspr   310, r29
    mtspr   561, r29
    mtspr   563, r29
    mtspr   564, r29
    mtspr   603, r29
    mtspr   574, r0     /* DSRR0-1 */
    mtspr   575, r0     /* DSRR1 */
    mtspr   400, r0     /* IVOR0-ICOR15 */
    mtspr   401, r0
    mtspr   402, r0
    mtspr   403, r0
    mtspr   404, r0
    mtspr   405, r0
    mtspr   406, r0
    mtspr   407, r0
    mtspr   408, r0
    mtspr   409, r0
    mtspr   410, r0
    mtspr   411, r0
    mtspr   412, r0
    mtspr   413, r0
    mtspr   414, r0
    mtspr   415, r0
    mtspr   528, r0     /* IVOR32-IVOR34 */
    mtspr   529, r0
    mtspr   530, r0
    mtspr   573, r0     /* MCAR  */
    mtspr   22, r0      /* DEC   */
    mtspr   54, r0      /* DECAR */
    mtspr   318, r0     /* DVC1  */
    mtspr   319, r0     /* DVC2  */
    mtspr   562, r0     /* DBCNT */
    mtspr   624, r0     /* MAS0 */
    mtspr   625, r0     /* MAS1 */
    mtspr   626, r0     /* MAS2 */
    mtspr   627, r0     /* MAS3 */
    mtspr   628, r0     /* MAS4 */
    mtspr   630, r0     /* MAS6 */

    evmra   r0, r4      /* ACC */

    se_blr
.size   zeroizeRegisters, .-zeroizeRegisters

/* End of zeroizeRegisters */



/**
 *   @func initExceptionHandlers
 * Set up the MCU's interrupt table. This relates to the 16 built-in exception handlers,
 * not to the external interrupts, the application code will mainly use. (The external
 * interrupts are one of the 16 exceptions set here.) We assign empty handlers to most
 * exceptions. These handler loop infinitely so that you will see the exception in the
 * debugger.
 */
    .type   initExceptionHandlers, @function
    .extern int_INTCInterruptHandler
initExceptionHandlers:
    
    /* Write the common most significant 16 bits of all the exception handler addresses. */
    e_lis   r0, sup_IVOR0trap@h
    mtspr   63, r0      /* IVPR */
    
    /* Write the individual bits 4..15 of the all the exception handler addresses. It
       doesn't matter to write all bits, later at exception handling time they will
       beignored or read zero. Therefore, we need to ensure that bits 0..3 of the actual
       addresses are surely zero. This is done below by assembler statement .align. */
    e_li    r0, sup_IVOR0trap@l
    mtspr   400, r0     /* IVOR0 Critical input */
    e_li    r0, sup_IVOR1trap@l
    mtspr   401, r0     /* IVOR1 Machine check (non-maskable interrupt) */
    e_li    r0, sup_IVOR2trap@l
    mtspr   402, r0     /* IVOR2 Data storage */
    e_li    r0, sup_IVOR3trap@l
    mtspr   403, r0     /* IVOR3 Instruction storage */
    
/// @todo Find out why the next load fails to get the right address if code is not in boot
// sector:
// e_li    r0, int_INTCInterruptHandler@l
// doesn't work depending on address, okay if int_INTCInterruptHandler is in boot sector,
// not if behind. The next statements substitute the short form by using the full address
// (and shifting). The Book E specification explains @l to be the lower 16 Bit of the word,
// normally used signed extended. e_li seems to take a 17 Bit signed operand. We would
// expect no problem but it doesn't work. Does VLE define @l differently?
//   The other traps are handled correct. Maybe because it are locally defined symbols,
// the distance is known at compile time and another reclocation entry can be used.
    e_lis    r0, int_INTCInterruptHandler@ha
    e_la    r0, int_INTCInterruptHandler@l(r0)
    e_rlwinm r0, r0, 0, 16, 31

    mtspr   404, r0     /* IVOR4 External input */
    e_li    r0, sup_IVOR5trap@l
    mtspr   405, r0     /* IVOR5 Alignment */
    e_li    r0, sup_IVOR6trap@l
    mtspr   406, r0     /* IVOR6 Program */
    e_li    r0, sup_IVOR7trap@l
    mtspr   407, r0     /* IVOR7 Floating-point unavailable */
    e_li    r0, sup_IVOR8trap@l
    mtspr   408, r0     /* IVOR8 System call */
    e_li    r0, sup_IVOR9trap@l
    mtspr   409, r0     /* IVOR9 AP unavailable (not used by this core) */
    e_li    r0, sup_IVOR10trap@l
    mtspr   410, r0     /* IVOR10 Decrementer */
    e_li    r0, sup_IVOR11trap@l
    mtspr   411, r0     /* IVOR11 Fixed-interval timer */
    e_li    r0, sup_IVOR12trap@l
    mtspr   412, r0     /* IVOR12 Watchdog timer */
    e_li    r0, sup_IVOR13trap@l
    mtspr   413, r0     /* IVOR13 Data TLB error */
    e_li    r0, sup_IVOR14trap@l
    mtspr   414, r0     /* IVOR14 Instruction TLB error */
    e_li    r0, sup_IVOR15trap@l
    mtspr   415, r0     /* IVOR15 Debug */
                 
    e_li    r0, sup_IVOR32trap@l
    mtspr   528, r0      /* IVOR32 SPE unavailable */
    e_li    r0, sup_IVOR33trap@l
    mtspr   529, r0      /* IVOR33 SPE data exception */
    e_li    r0, sup_IVOR34trap@l
    mtspr   530, r0      /* IVOR34 SPE round exception */

    se_blr

    .size   initExceptionHandlers, .-initExceptionHandlers
/* End of initExceptionHandlers */


/**
 *   @func sup_IVOR0trap
 * Exception handler IVOR0 for the Critical Input interrupt.
 */
    .global sup_IVOR0trap   /* Note, it is not required to make this symbol global. However
                               it is handy to see the traps in the application map file and
                               this is why we do so for all traps. */
    .type   sup_IVOR0trap, @function
    .align  4
sup_IVOR0trap:
    se_b    sup_IVOR0trap   /* Critical Input interrupt handler */ 
    .size   sup_IVOR0trap, .-sup_IVOR0trap


/**
 *   @func sup_IVOR1trap
 * Exception handler IVOR1 for the Machine check / (NMI) interrupt.
 */
    .global sup_IVOR1trap
    .type   sup_IVOR1trap, @function
    .align  4
sup_IVOR1trap:
    se_b    sup_IVOR1trap   /* Machine check / (NMI) interrupt handler */
    .size   sup_IVOR1trap, .-sup_IVOR1trap


/**
 *   @func sup_IVOR2trap
 * Exception handler IVOR2 for the Data Storage interrupt. This trap is taken if the MMU
 * signals an unpermitted data access.
 */
    .global sup_IVOR2trap
    .type   sup_IVOR2trap, @function
    .align  4
sup_IVOR2trap:
    se_b    sup_IVOR2trap   /* Data Storage interrupt handler */
    .size   sup_IVOR2trap, .-sup_IVOR2trap


/**
 *   @func sup_IVOR3trap
 * Exception handler IVOR3 for the Instruction Storage interrupt. This trap is taken if
 * the MMU signals an unpermitted instruction fetch.
 */
    .global sup_IVOR3trap
    .type   sup_IVOR3trap, @function
    .align  4
sup_IVOR3trap:
    se_b    sup_IVOR3trap   /* Instruction Storage interrupt handler */
    .size   sup_IVOR3trap, .-sup_IVOR3trap


/* Note, there's no default IVOR4 trap since we always install a real trap handler for the
   inevitable external interrupt handling. */

/**
 *   @func sup_IVOR5trap
 * Exception handler IVOR5 for the Alignment interrupt.
 */
    .global sup_IVOR5trap
    .type   sup_IVOR5trap, @function
    .align  4
sup_IVOR5trap:
    se_b    sup_IVOR5trap   /* Alignment interrupt handler */
    .size   sup_IVOR5trap, .-sup_IVOR5trap


/**
 *   @func sup_IVOR6trap
 * Exception handler IVOR6 for the Program interrupt.
 */
    .global sup_IVOR6trap
    .type   sup_IVOR6trap, @function
    .align  4
sup_IVOR6trap:
    se_b    sup_IVOR6trap   /* Program interrupt handler */
    .size   sup_IVOR6trap, .-sup_IVOR6trap


/**
 *   @func sup_IVOR7trap
 * Exception handler IVOR7 for the Floating-point unavailable interrupt.
 */
    .global sup_IVOR7trap
    .type   sup_IVOR7trap, @function
    .align  4
sup_IVOR7trap:
    se_b    sup_IVOR7trap   /* Floating-point unavailable interrupt handler */
    .size   sup_IVOR7trap, .-sup_IVOR7trap


/**
 *   @func sup_IVOR8trap
 * Exception handler IVOR8 for the System call interrupt.
 */
    .global sup_IVOR8trap
    .type   sup_IVOR8trap, @function
    .align  4
sup_IVOR8trap:
    se_b    sup_IVOR8trap   /* System call interrupt handler */
    .size   sup_IVOR8trap, .-sup_IVOR8trap


/**
 *   @func sup_IVOR9trap
 * Exception handler IVOR9 for the AP unavailable interrupt.
 */
    .global sup_IVOR9trap
    .type   sup_IVOR9trap, @function
    .align  4
sup_IVOR9trap:
    se_b    sup_IVOR9trap   /* AP unavailable interrupt handler */
    .size   sup_IVOR9trap, .-sup_IVOR9trap


/**
 *   @func sup_IVOR10trap
 * Exception handler IVOR10 for the Decrementer interrupt.
 */
    .global sup_IVOR10trap
    .type   sup_IVOR10trap, @function
    .align  4
sup_IVOR10trap:
    se_b    sup_IVOR10trap   /* Decrementer interrupt handler */
    .size   sup_IVOR10trap, .-sup_IVOR10trap


/**
 *   @func sup_IVOR11trap
 * Exception handler IVOR11 for the Fixed Interval Timer interrupt.
 */
    .global sup_IVOR11trap
    .type   sup_IVOR11trap, @function
    .align  4
sup_IVOR11trap:
    se_b    sup_IVOR11trap   /* Fixed Interval Timer interrupt handler */
    .size   sup_IVOR11trap, .-sup_IVOR11trap


/**
 *   @func sup_IVOR12trap
 * Exception handler IVOR12 for the Watchdog Timer interrupt.
 */
    .global sup_IVOR12trap
    .type   sup_IVOR12trap, @function
    .align  4
sup_IVOR12trap:
    se_b    sup_IVOR12trap   /* Watchdog Timer interrupt handler */
    .size   sup_IVOR12trap, .-sup_IVOR12trap


/**
 *   @func sup_IVOR13trap
 * Exception handler IVOR13 for the Data TLB Error interrupt.
 */
    .global sup_IVOR13trap
    .type   sup_IVOR13trap, @function
    .align  4
sup_IVOR13trap:
    se_b    sup_IVOR13trap   /* Data TLB Error interrupt handler */
    .size   sup_IVOR13trap, .-sup_IVOR13trap


/**
 *   @func sup_IVOR14trap
 * Exception handler IVOR14 for the Instruction TLB Error interrupt.
 */
    .global sup_IVOR14trap
    .type   sup_IVOR14trap, @function
    .align  4
sup_IVOR14trap:
    se_b    sup_IVOR14trap   /* Instruction TLB Error interrupt handler */
    .size   sup_IVOR14trap, .-sup_IVOR14trap


/**
 *   @func sup_IVOR15trap
 * Exception handler IVOR15 for the Debug I.
 */
    .global sup_IVOR15trap
    .type   sup_IVOR15trap, @function
    .align  4
sup_IVOR15trap:
    se_b    sup_IVOR15trap   /* Debug Interrupt */
    .size   sup_IVOR15trap, .-sup_IVOR15trap

/**
 *   @func sup_IVOR32trap
 * Exception handler IVOR32 for the SPE unavailable interrupt.
 */
    .global sup_IVOR32trap
    .type   sup_IVOR32trap, @function
    .align  4
sup_IVOR32trap:
    se_b    sup_IVOR32trap   /* SPE unavailable */
    .size   sup_IVOR32trap, .-sup_IVOR32trap


/**
 *   @func sup_IVOR33trap
 * Exception handler IVOR33 for the SPE data exception interrupt.
 */
    .global sup_IVOR33trap
    .type   sup_IVOR33trap, @function
    .align  4
sup_IVOR33trap:
    se_b    sup_IVOR33trap   /* SPE data exception */
    .size   sup_IVOR33trap, .-sup_IVOR33trap


/**
 *   @func sup_IVOR34trap
 * Exception handler IVOR34 for the SPE round exception interrupt.
 */
    .global sup_IVOR34trap
    .type   sup_IVOR34trap, @function
    .align  4
sup_IVOR34trap:
    se_b    sup_IVOR34trap   /* SPE round exception */
    .size   sup_IVOR34trap, .-sup_IVOR34trap

/* End of IVOR exception handlers 0..15 and 32..24 */



/**
 *   @func initMMU
 * Shape an initial MMU configuration such that all addresses can be used without the
 * danger of an exception.
 *   @remark
 * This code is based on NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file
 * MPC5643L_HWInit.c, l. 396ff.
 */
    .type   initMMU, @function
initMMU:

    mflr    r27

    /* On entry we have a single valid MMU entry, which permits executing code from the
       boot sector. This is entry 0. We must not overwrite this entry, whicle our code
       execution still depends on it and the MMU documentation says that we must not have
       overlapping memory regions defined, neither. This makes the initiliazation sequence
       a bit complicated. We first install a temporary entry for the flash ROM section (to
       read the rest of this code), make it then operate, install the final flash ROM
       related entry (which may now overwrite the initial one in table entry 0), make this
       one operate and can safely install the rest.
         The trick to make the temporary entry operated at a defined point in time and
       without getting an overlap with the initial entry is the use of another address
       space, see MSR, bit IS. 
         Note, the assembly code in this routine must not touch the RAM, it is not
       accessible yet. */

    /* Install the temporary table entry for flash ROM instruction read. We use the normal
       configuration with the only modification of selecting another address space.
         1MB at 0x0 (flash ROM), address space 1. */
    e_lis   r3, MMU_TLB1_TMP_ENTRY1_MAS0@h
    e_or2i  r3, MMU_TLB1_TMP_ENTRY1_MAS0@l
    e_lis   r4, MMU_TLB1_TMP_ENTRY1_MAS1@h
    e_or2i  r4, MMU_TLB1_TMP_ENTRY1_MAS1@l
    e_lis   r5, MMU_TLB1_TMP_ENTRY1_MAS2@h
    e_or2i  r5, MMU_TLB1_TMP_ENTRY1_MAS2@l
    e_lis   r6, MMU_TLB1_TMP_ENTRY1_MAS3@h
    e_or2i  r6, MMU_TLB1_TMP_ENTRY1_MAS3@l
    e_bl    writeMMUTableEntryS

    /* Make the new entry operate by switching to the other address space 1. */
    mfmsr   r3
    se_mr   r7, r3          /* Save current value of msr. t7 is not used in the sub-routines */
    e_or2i  r3, 0x20        /* Set IS. DS irrelevant, we must anyway not do a data access */
    mtmsr   r3              /* Does implicit isync, next instruction should be re-read
                               through our new table entry */

    /* Write intended table entry for flash ROM. It uses the normal address space 0 and is
       therefore not (yet) matching and does therefore not mean an overlapping region. The
       BAM provided boot section region is overwritten and thereby invalidated at the same
       time.
         1MB at 0x0 (flash ROM), address space 0. */
    e_lis   r3, MMU_TLB1_ENTRY0_MAS0@h
    e_or2i  r3, MMU_TLB1_ENTRY0_MAS0@l
    e_lis   r4, MMU_TLB1_ENTRY0_MAS1@h
    e_or2i  r4, MMU_TLB1_ENTRY0_MAS1@l
    e_lis   r5, MMU_TLB1_ENTRY0_MAS2@h
    e_or2i  r5, MMU_TLB1_ENTRY0_MAS2@l
    e_lis   r6, MMU_TLB1_ENTRY0_MAS3@h
    e_or2i  r6, MMU_TLB1_ENTRY0_MAS3@l
    e_bl    writeMMUTableEntry

    /* Switch back the normal address space. */
    mtmsr   r7              /* Does implicit isync, next instruction should be re-read
                               through our normal table entry, index 0. */
    
    /* Continue code execution in the normal address space under control of the final flash
       ROM entry 0.  */
    
    /* 1MB at 0xf0_0000 (shadow flash ROM). Writing this entry implicitely invalidates the
       temporary flash ROM entry for address space 1. */
    e_lis   r3, MMU_TLB1_ENTRY1_MAS0@h
    e_or2i  r3, MMU_TLB1_ENTRY1_MAS0@l
    e_lis   r4, MMU_TLB1_ENTRY1_MAS1@h
    e_or2i  r4, MMU_TLB1_ENTRY1_MAS1@l
    e_lis   r5, MMU_TLB1_ENTRY1_MAS2@h
    e_or2i  r5, MMU_TLB1_ENTRY1_MAS2@l
    e_lis   r6, MMU_TLB1_ENTRY1_MAS3@h
    e_or2i  r6, MMU_TLB1_ENTRY1_MAS3@l
    e_bl    writeMMUTableEntry

    /* 128k at 0x4000_0000 (SRAM) */
    e_lis   r3, MMU_TLB1_ENTRY2_MAS0@h
    e_or2i  r3, MMU_TLB1_ENTRY2_MAS0@l
    e_lis   r4, MMU_TLB1_ENTRY2_MAS1@h
    e_or2i  r4, MMU_TLB1_ENTRY2_MAS1@l
    e_lis   r5, MMU_TLB1_ENTRY2_MAS2@h
    e_or2i  r5, MMU_TLB1_ENTRY2_MAS2@l
    e_lis   r6, MMU_TLB1_ENTRY2_MAS3@h
    e_or2i  r6, MMU_TLB1_ENTRY2_MAS3@l
    e_bl    writeMMUTableEntryS             /* "S": We may execute some code in RAM, see
                                               e.g. initFlash */

    /* 256k at 0x8ff0_0000 (on platform 1 peripherals, from PBRIDGE_0 till STM_1) */
    e_lis   r3, MMU_TLB1_ENTRY3_MAS0@h
    e_or2i  r3, MMU_TLB1_ENTRY3_MAS0@l
    e_lis   r4, MMU_TLB1_ENTRY3_MAS1@h
    e_or2i  r4, MMU_TLB1_ENTRY3_MAS1@l
    e_lis   r5, MMU_TLB1_ENTRY3_MAS2@h
    e_or2i  r5, MMU_TLB1_ENTRY3_MAS2@l
    e_lis   r6, MMU_TLB1_ENTRY3_MAS3@h
    e_or2i  r6, MMU_TLB1_ENTRY3_MAS3@l
    e_bl    writeMMUTableEntry

    /* 64k at 0x8ff4_0000 (on platform 1 peripherals, ECSM_1 and INTC_1) */
    e_lis   r3, MMU_TLB1_ENTRY4_MAS0@h
    e_or2i  r3, MMU_TLB1_ENTRY4_MAS0@l
    e_lis   r4, MMU_TLB1_ENTRY4_MAS1@h
    e_or2i  r4, MMU_TLB1_ENTRY4_MAS1@l
    e_lis   r5, MMU_TLB1_ENTRY4_MAS2@h
    e_or2i  r5, MMU_TLB1_ENTRY4_MAS2@l
    e_lis   r6, MMU_TLB1_ENTRY4_MAS3@h
    e_or2i  r6, MMU_TLB1_ENTRY4_MAS3@l
    e_bl    writeMMUTableEntry

    /* 512k at 0xC3f8_0000 (off-platform peripherals, till STCU) */
    e_lis   r3, MMU_TLB1_ENTRY5_MAS0@h
    e_or2i  r3, MMU_TLB1_ENTRY5_MAS0@l
    e_lis   r4, MMU_TLB1_ENTRY5_MAS1@h
    e_or2i  r4, MMU_TLB1_ENTRY5_MAS1@l
    e_lis   r5, MMU_TLB1_ENTRY5_MAS2@h
    e_or2i  r5, MMU_TLB1_ENTRY5_MAS2@l
    e_lis   r6, MMU_TLB1_ENTRY5_MAS3@h
    e_or2i  r6, MMU_TLB1_ENTRY5_MAS3@l
    e_bl    writeMMUTableEntry

    /* 512k at 0xffe0_0000 (off-platform peripherals, from ADC) */
    e_lis   r3, MMU_TLB1_ENTRY6_MAS0@h
    e_or2i  r3, MMU_TLB1_ENTRY6_MAS0@l
    e_lis   r4, MMU_TLB1_ENTRY6_MAS1@h
    e_or2i  r4, MMU_TLB1_ENTRY6_MAS1@l
    e_lis   r5, MMU_TLB1_ENTRY6_MAS2@h
    e_or2i  r5, MMU_TLB1_ENTRY6_MAS2@l
    e_lis   r6, MMU_TLB1_ENTRY6_MAS3@h
    e_or2i  r6, MMU_TLB1_ENTRY6_MAS3@l
    e_bl    writeMMUTableEntry

    /* 256k at 0xfff0_0000 (on Platform 0 Peripherals, from PBRIDGE till STM) */
    e_lis   r3, MMU_TLB1_ENTRY7_MAS0@h
    e_or2i  r3, MMU_TLB1_ENTRY7_MAS0@l
    e_lis   r4, MMU_TLB1_ENTRY7_MAS1@h
    e_or2i  r4, MMU_TLB1_ENTRY7_MAS1@l
    e_lis   r5, MMU_TLB1_ENTRY7_MAS2@h
    e_or2i  r5, MMU_TLB1_ENTRY7_MAS2@l
    e_lis   r6, MMU_TLB1_ENTRY7_MAS3@h
    e_or2i  r6, MMU_TLB1_ENTRY7_MAS3@l
    e_bl    writeMMUTableEntry

    /* 64k at 0xfff4_0000 (on Platform 0 Peripherals, from ECSM till INTC) */
    e_lis   r3, MMU_TLB1_ENTRY8_MAS0@h
    e_or2i  r3, MMU_TLB1_ENTRY8_MAS0@l
    e_lis   r4, MMU_TLB1_ENTRY8_MAS1@h
    e_or2i  r4, MMU_TLB1_ENTRY8_MAS1@l
    e_lis   r5, MMU_TLB1_ENTRY8_MAS2@h
    e_or2i  r5, MMU_TLB1_ENTRY8_MAS2@l
    e_lis   r6, MMU_TLB1_ENTRY8_MAS3@h
    e_or2i  r6, MMU_TLB1_ENTRY8_MAS3@l
    e_bl    writeMMUTableEntry

    /* 64k at 0xfff9_0000 (off Platform Peripherals, only DSPI) */
    e_lis   r3, MMU_TLB1_ENTRY9_MAS0@h
    e_or2i  r3, MMU_TLB1_ENTRY9_MAS0@l
    e_lis   r4, MMU_TLB1_ENTRY9_MAS1@h
    e_or2i  r4, MMU_TLB1_ENTRY9_MAS1@l
    e_lis   r5, MMU_TLB1_ENTRY9_MAS2@h
    e_or2i  r5, MMU_TLB1_ENTRY9_MAS2@l
    e_lis   r6, MMU_TLB1_ENTRY9_MAS3@h
    e_or2i  r6, MMU_TLB1_ENTRY9_MAS3@l
    e_bl    writeMMUTableEntry

    /* 256k at 0xfffc_0000 (off Platform Peripherals, rest) */
    e_lis   r3, MMU_TLB1_ENTRY10_MAS0@h
    e_or2i  r3, MMU_TLB1_ENTRY10_MAS0@l
    e_lis   r4, MMU_TLB1_ENTRY10_MAS1@h
    e_or2i  r4, MMU_TLB1_ENTRY10_MAS1@l
    e_lis   r5, MMU_TLB1_ENTRY10_MAS2@h
    e_or2i  r5, MMU_TLB1_ENTRY10_MAS2@l
    e_lis   r6, MMU_TLB1_ENTRY10_MAS3@h
    e_or2i  r6, MMU_TLB1_ENTRY10_MAS3@l
    e_bl    writeMMUTableEntry

    mtlr r27

    se_blr

    .size   initMMU, .-initMMU
/* End of initMMU */




/**
 *   @func writeMMUTableEntryS
 * Creates a new TLB entry with synchronization. It ensures that the TLB context change
 * doesn't affect the core by execution before and after tlbwe a context synchronizing
 * instruction (CSI). This should be called when creating SRAM or FLASH TLB entries.\n
 *   Instruction sequence: Write GPR to MAS, execute CSI, tlbwe, CSI
 *   @remark
 * This code is based on NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file
 * MPC5643L_HWInit.c, l. 210ff
 */
    .type   writeMMUTableEntryS, @function
writeMMUTableEntryS:

    /* Write MMU Assist Register 0 (MAS0); SPR 624 */
    mtspr   624, r3
    /* Write MMU Assist Register 1 (MAS1); SPR 625 */
    mtspr   625, r4
    /* Write MMU Assist Register 2 (MAS2); SPR 626 */
    mtspr   626, r5
    /* Write MMU Assist Register 3 (MAS3); SPR 627 */
    mtspr   627, r6

    /* Write the table entry.
         All instruction will complete here in current context. */
    msync
    tlbwe

    /* Synchronize instruction fetches and data accesses in respect with the new created
       TLB entry. */
    msync
    isync

    se_blr

    .size   writeMMUTableEntryS, .-writeMMUTableEntryS
/* End of writeMMUTableEntryS */



/**
 *   @func writeMMUTableEntry
 * Creates a new TLB entry with following instruction sequence: write GPR to MAS, execute
 * tlbwe
 *   @remark
 * This code is based on NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file
 * MPC5643L_HWInit.c, l. 241ff
 */
    .type   writeMMUTableEntry, @function
writeMMUTableEntry:

    /* Write MMU Assist Register 0 (MAS0); SPR 624 */
    mtspr   624, r3
    /* Write MMU Assist Register 1 (MAS1); SPR 625 */
    mtspr   625, r4
    /* Write MMU Assist Register 2 (MAS2); SPR 626 */
    mtspr   626, r5
    /* Write MMU Assist Register 3 (MAS3); SPR 627 */
    mtspr   627, r6
    /* Write the table entry */
    tlbwe

    se_blr

    .size   writeMMUTableEntry, .-writeMMUTableEntry
/* End of writeMMUTableEntry */



/**
 *   @func initTimeBase
 * Initialize the MCU owned timer "Time Base", which is a free running 64 Bit high
 * resolution timer, mostly used to measure execution times. It is enabled and reset to
 * zero.
 */
    .type   initTimeBase, @function
initTimeBase:
    /* Reset timer */
    se_li   r0, 0
    mtspr   284, r0    /* Clear TBL */
    mtspr   285, r0    /* Clear TBU */

    /* Enable time base */
    mfspr   r0, 1008    /* Read register Hardware implementation dependent (HID) 0 */
    e_or2i  r0, 1<<14   /* Set bit time base enable (TBEN) */
    mtspr   1008, r0    /* Write updated value back to HID0 */

    se_blr

    .size   initTimeBase, .-initTimeBase
/* End of initTimeBase */




/**
 *   @func zeroizeSRAM
 * Zeroize all SRAM cells. All bytes from ld_memRamStart and ld_memRamStart+ld_memRamSize are
 * written to zero.\n
 *   ld_memRamStart needs to be 4 Byte aligned, ld_memRamStart+ld_memRamSize is rounded
 * downwards to a multiple of 16. (Remaining bytes wouldn't be touched.)
 *   @remark
 * An optimization could be zeroizing only the used RAM cells. The according labels are
 * exported by the linker. The stack portion of the RAM will anyway be initialized twice;
 * first here and later it is filled with the fill pattern required for computation of
 * stack consumption.
 */
    .type   zeroizeSRAM, @function
    .extern ld_memRamStart, ld_memRamSize
zeroizeSRAM:
    e_lis   r8, ld_memRamStart@h
    e_or2i  r8, ld_memRamStart@l
    e_lis   r9, ld_memRamSize@h
    e_or2i  r9, ld_memRamSize@l
    srawi   r9, r9, 4       /* Divide length in Byte by 2^4=16 */
    mtctr   r9
    se_li   r31, 0
    se_li   r30, 0
    se_li   r29, 0
    se_li   r28, 0

zreg_loop:
    /* ECC SRAM must be initialized with 64-bit writes, hence stmw;
     * 16-btye alignment should work always */
    e_stmw  r28, 0(r8)      /* Write registers 28-31 thus 16 Byte */
    e_la    r8, 16(r8)
    e_bdnz  zreg_loop

    se_blr

    .size   zeroizeSRAM, .-zeroizeSRAM
/* End of zeroizeSRAM */




/**
 *   @func disableWatchdog
 * Disable the watchdog.
 *   @remark
 * The assembler implementation here follows for example the implementation of NXP
 * sample NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file main.c, l. 221ff.
 */
    .type   disableWatchdog, @function
disableWatchdog:
    // SWT.SR.R = 0x0000c520;   /* Write 1st key to clear soft lock bit */
    e_lis   r9, 0xfff3
    e_or2i  r9, 0x8000
    e_li    r10, 0
    e_or2i  r10, 0xc520
    e_stw   r10, 16(r9)
    // SWT.SR.R = 0x0000d928;   /* Write 2nd key to clear soft lock bit */
    e_li    r10, 0
    e_or2i  r10, 0xd928
    e_stw   r10, 16(r9)
    // SWT.CR.R = 0x8000010A;   /* Clear watchdog enable (WEN) */
    e_lis   r10, 0x8000
    e_or2i  r10, 0x10a
    e_stw   r10, 0(r9)
    // Clear e200 Core Watchdog Timer
    e_li    r9, 0
    mtspr   340, r9

    se_blr

    .size   disableWatchdog, .-disableWatchdog
/* End of disableWatchdog */



/**
 *   @func initFlash
 * Access the flash flash memory block C90FL and select the optimal strategy. This affects
 * the number of wait states, port usage (data versus instruction fetches) and pre-fetch
 * strategies.\n
 *   The function needs to be called prior to setting the normal operation clock rate of
 * the controller (i.e. prior to PLL configuration).
 *   @remark
 * This function uses a stack frame and must not be called until to the stack
 * initialization has been done.
 *   @remark
 * MCU ref. manual, 23.1.5, p. 560: The code to access the registers PFCR0 and PFAPR of the
 * flash memory block must reside in RAM - the flash configuration must not be done from
 * code, which itself resides in ROM. This function copies a tiny machine code fragment onto
 * the stack and branches there to fulfill this requirement.
 */
    .type   initFlash, @function
initFlash:
    e_stwu  sp, -24(sp) /* We need 20 Byte but stack frame must be multiple of 8 (EABI) */
    mflr    r0
    e_stw   r0, 28(sp)
    
    /* Load the required three movable instructions into r8..r10. */
    e_lis   r10, if_movalbeMachineCode@h
    e_add16i r10, r10, if_movalbeMachineCode@l
    e_lwz   r8, 0(r10)
    e_lwz   r9, 4(r10)
    e_lwz   r10, 8(r10)
    
    /* Store moveable sub-routine in the stack area. */
    e_stw   r8, 8(sp)
    e_stw   r9, 12(sp)
    e_stw   r10, 16(sp)
    
    /* Load function arguments of our moveable sub-routine. */
    e_lis   r3, 0x18c7      /* 0x18c7eded: Configuration word. */
    e_or2i  r3, 0xeded
    e_lis   r4, 0xc3f8      /* &CFLASH.PFCR0.R: Configuration register of flash controller */
    e_or2i  r4, 0x801c
    msync
    isync
    
    /* Call sub-routine, which is in the stack area. */
    e_la    r9, 8(sp)
    mtctr   r9
    se_bctrl
    
    e_lwz   r0, 28(sp)
    mtlr    r0
    e_la    sp, 24(sp)
    se_blr

if_movalbeMachineCode:
    e_stw   r3, 0(r4)
    msync
    se_blr
    .dc.w   0               /* We move 3 32 Bit words, so ensure there are those. */
    
    .size   initFlash, .-initFlash
/* End of initFlash */



/**
 * Prepare some registers as required by the PowerPC eabi. GCC will invoke this function on
 * entry into main.
 *   @remark
 * GCC 4.9.0 and 4.9.2 for PowerPC invoke the callback __eabi even if the switch
 * -mnoeabi is given on the command line.
 *   @remark
 * Never rename this function. GCC will otherwise not find it as callback and take its own
 * variant from the clib, which is not operational in our context.
 */
    .globl  __eabi
    .extern _SDA_BASE_, _SDA2_BASE_
    .type   __eabi, @function
__eabi:
    e_lis   r13, _SDA_BASE_@h       /* Load .sdata base into r13 */
    e_or2i  r13, _SDA_BASE_@l
    e_lis   r2, _SDA2_BASE_@h       /* Load .sdata2 base into r2 */
    e_or2i  r2, _SDA2_BASE_@l

    se_blr

    .size   __eabi, .-__eabi
/* End of __eabi */

    /* At the end of the code: Avoid a gap in the section. This makes the flash procedure
       in Code Warrior 10.7 fail to flash the code. */
    .align  4

