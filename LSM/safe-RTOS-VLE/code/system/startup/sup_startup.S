/**
 * @file sup_startup.S
 * VLE assembler implementation of startup code for MPC5643L in Lockstep Mode (LSM). The
 * code has been designed for use with GCC for PowerPC EABI.
 *   On core 0, the startup code:\n
 *   - Zeroizes all GPR\n
 *   - Initializes the exception vectors (IVOR)\n
 *   - Initializes the MMU for widely open access\n
 *   - Enables Critical Interrupt handling\n
 *   - Enables branch target prediction\n
 *   - Initializes the high resolution timer (time base)\n
 *   - Zeroizes all physical RAM\n
 *   - Disables the watchdog, which is running by default\n
 *   - Enables instruction cache\n
 *   - Initializes the stack\n
 *   - Presets the small data pointers r2 and r13\n
 *   - Copy initial values to RAM for all initialized data sections\n
 *   - Load instance pointer for given core into USPR
 *   - Branches into the C main function\n
 *   The further HW initialization is left to the C code and needs to be called from the
 * begining of the main function. A minumum configuration would be:\n
 *   - All clock settings (on entry into main we still run on the fast internal RC
 *     oscillator)\n
 *   - Peripheral bridge\n
 *   - Interrupt controller for External Interrupts\n
 *
 * Copyright (C) 2017-2020 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   sup_startup
 *   sup_IVOR0trap, sup_IVOR9trap .. sup_IVOR12trap
 * Local functions
 *   zeroizeRegisters
 *   disableWatchdog
 *   initExceptionHandlers
 *   initMMU
 *   writeMMUTableEntryS
 *   writeMMUTableEntry
 *   initTimeBase
 *   zeroizeSRAM
 *   initSDataRegisters
 *   disableWatchdog
 *   initFlash
 */

#ifndef __VLE__
# error This file is intended for compilation for VLE instruction set only
#endif

/*
 * Include files
 */

#include "mmu_mmuRegions.h"
#include "rtos_ivorHandler.h"


/*
 * Defines
 */

/** Switch instruction cache on. */
#define ENABLE_ICACHE               1

/** Switch branch prediction on. */
#define ENABLE_BRANCH_PREDICTION    1

/* The definition of the boot sector (at 0x00000000). The data contents are defined here
   and the location of the boot sector is defined in the linker control file.
     See MCU reference manual, 5.3, p.126ff */
#define RCHW_VLE        0x0100      /* Optional: Enable VLE instruction set */
#define RCHW_BOOT_ID    0x005A      /* Fixed, hard-coded value */
#define RCHW_VALUE      (RCHW_VLE|RCHW_BOOT_ID)


/*
 * External function declarations
 */

/** C main function. */
.extern main

/*
 * Data declarations
 */


/*
 * Function implementation
 */


/**
 * The boot sector contents are defined as first element of sector .boot.
 */
    .section    .bamData, "a"
    .global     sup_bamData         /* Nice to have this in the map file */
sup_bamData:
    .dc.w       RCHW_VALUE, 0
    .dc.l       sup_startUp


/**
 *   @func sup_startUp
 * Reset address: All machine code execution starts here. The label sup_startUp is
 * specified in the BAM data block in the boot sector to make this happen after reset.
 */
    .section    .boot, "a"
    .extern     ld_stackStartOS     /* Absolute start of all stack memory. */
    .extern     ld_stackEndOS       /* End of stack memory area. */
    .global     sup_startUp
    .balign     2
sup_startUp:
    e_bl        zeroizeRegisters    /* Clear all GPR and all relevant SPR */
    e_bl        initExceptionHandlers   /* Set all IVOR registers of the MCU */
    e_bl        initMMU             /* Make MMU allow all memory accesses */

    /* Exception handlers are installed, memory is accessible - there's no reason to wait
       any longer with enabling the higher prior interrupts (machine check). We have the
       minimum functionality of an empty trap handler, which reports the problem to the SW
       developer.
         Note, the external interrupts are not enabled yet. They are entirely managed by
       the application software. */
    e_li        r0, 0x1000  /* Initialize MSR: CE=EE=0 but ME=1 */
    mtmsr       r0

#if 0 // We don't enable debug interrupts at all
    /* Our RTOS requires catching the trap instruction with the IVOR #15 handler. This can
       be done safely only if the handler cannot be preempted by an External Interrupt. By
       default, this is possible. We configure the HID0 to clear MSR[EE] on entry into IVOR
       #15. */
    mfspr       r0, 1008    /* Read register Hardware implementation dependent (HID) 0 */
    se_bseti    r0, 19      /* Set bit DCLREE, Debug Interrupt Clears MSR[EE] */
    se_bseti    r0, 20      /* Set bit DCLRCE, Debug Interrupt Clears MSR[CE] */
    mtspr       1008, r0    /* Write updated value back to HID0 */
#endif

#if ENABLE_BRANCH_PREDICTION == 1
    /* Enable the Branch Target Buffer (BTB) for branch prediction. Clear all buffer
       entries, which might be arbitrarily set after reset, and enable all branch
       predictions. See CoreRM, sections 2.4.13, p. 81ff, and 4.4.2, p. 140.
         0x200: BBFI, Branch target buffer flash invalidate
         0x1: BPEN, Branch target buffer prediction enable */
    e_li        r0, 0x201
    mtspr       1013, r0        /* SPR 1013, Branch Unit Control and Status Register (BUCSR) */
    se_isync
#endif

    e_bl        initTimeBase    /* Start the MCU's high resolution counter */
    e_bl        zeroizeSRAM     /* Clear all RAM to avoid ECC errors */

    /* The watchdog is disabled now. Most available sample code code does do this as C
       function at the beginning of main, however, if we are in lockstep mode and use the
       full available RAM of 128k then it takes much too long to copy the data section and
       to zeroize the bss section and the watchdog bites. */
    e_bl        disableWatchdog

    /* Enable instruction cache. The procedure is described in the e200z4 Power
       Architecture Core Reference Manual, section 9.6, p. 552ff. 9.6.1 Cache
       Enable/Disable and 9.6.5 Cache Invalidation apply. */
#if ENABLE_ICACHE == 1
su_icache_cfg:
    /* Core RM, 9.6.5: Instruction cache invalidation. */
    se_li       r0, 0x2     /* SPR 1011: L1CSR1, Cache control and status register 1, Bit 1 */
    mtspr       1011, r0    /* Set bit L1CSR1[ICINV] to start the invalidation of the
                               entire instruction cache. All other bits remain zero, which
                               is their reset default value */

su_icache_waitForInvalidation:
    sync            /* EREF 2.0, 4.5.4.3, table 4-3, demands a sync prio to mfspr L1CSR0/1 */
    mfspr       r0, 1011    /* Read L1CSR1, invalidation busy bit 0x2 will be reset by HW
                               on completion, abort bit 0x4 may be set before */
    se_btsti    r0, 29      /* Test for abort */
    se_beq      su_icache_noAbort
    se_bclri    r0, 29      /* Explicitly clear the abort flag by SW */
    mtspr       1011, r0
    se_b        su_icache_cfg   /* Start over, retry */

su_icache_noAbort:
    se_btsti    r0, 30      /* Test still busy flag */
    se_bne      su_icache_waitForInvalidation   /* Busy wait for completion. */

    /* Core RM, 9.6.1: Data cache enabling */
    se_bseti    r0, 31      /* Set bit L1CSR1[CDE], data cache enable */
    se_isync
    msync
    mtspr       1011, r0
    se_isync        /* EREF 2.0, 4.5.4.3, table 4-3, demands an isync after mfspr L1CSR0/1.
                       We do this only here, where cache enabling has a context altering
                       effect. */
#endif

   /* The initial setting of the stack pointer. The stack rises towards lower addresses so
      the initial value is the end of the reserved RAM area. The value is provided by the
      linker, which is in charge to do the reservation but 16 Bytes are subtracted in order
      to implements some EABI requirements concerning the logical bottom of the stack.
        Note, there are some strict alignment constraints (8 Byte aligned values needed),
      which need to be ensured in the linker script file. They are not checked here and
      will in case cause an exception. */
    e_lis       sp, (ld_stackEndOS-16)@h
    e_or2i      sp, (ld_stackEndOS-16)@l

    /* Select appropriate conditions for the operation of the flash (waitstates, caching,
       etc.) It is essential to do this prior to rising the CPU clock rate by configuring
       the PLL. */
    e_bl        initFlash

    /* Fill the stack with a byte pattern. This pattern is used later to measure the
       current stack usage. */
    e_lis       r3, ld_stackStartOS@h
    e_or2i      r3, ld_stackStartOS@l
    e_lis       r4, ld_stackEndOS@h
    e_or2i      r4, ld_stackEndOS@l
    se_sub      r4, r3
    se_srwi     r4, 0x2             /* Divide length in Byte by 2^2=4 */
    se_mtctr    r4
    e_lis       r4, 0xa5a5a5a5@h    /* Load pattern into r4 */
    e_or2i      r4, 0xa5a5a5a5@l
su_loopFillStack:
    e_stw       r4, 0(r3)           /* Write the four pattern bytes in r4 */
    se_addi     r3, 4               /* Increment write pointer */
    e_bdnz      su_loopFillStack    /* Branch on counter register not zero */

    /* Taken from NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file __start.c, l. 186ff:
       "Prepare a terminating stack record."
         Note, we had set the stack pointer 16 Bytes below the end of the RAM area, so we
       have enough head room to do so. */
    se_li       r3, 0       /* Clear r3 */
    e_stw       r3, 0(sp)   /* Initial stack frame link pointer is set to NULL */
    se_subi     r3, 1       /* Load 0xffffffff into r3 */
    e_stw       r3, 4(sp)   /* By convention, initialize return address from first stack
                               frame to -1. Has actually no effect */

    /* Load r2 and r13 for small, fast data access */
    e_bl        initSDataRegisters

    /* Copy the mirror of the initialized RAM sections from flash ROM to the RAM - all
       global C data will have the wanted initial values.
         suc_initializeRam: Copying the RAM blocks is implemented in C code. */
    e_lis       r3, suc_initializeRam@h
    e_or2i      r3, suc_initializeRam@l
    se_mtlr     r3
    se_blrl

    /* Load the pointer to the core instance pointer, i.e. the pointer to the data, which
       is individual to each core. */
    .extern     rtos_kernelInstanceDataAry
    mfspr       r3, SPR_PIR
    e_mull2i    r3, SIZE_OF_INSTANCE_DATA   /* Requires SIZE_OF_INSTANCE_DATA < 0x8000 */
    e_add2is    r3, rtos_kernelInstanceDataAry@ha
    e_add16i    r3, r3, rtos_kernelInstanceDataAry@l
    mtspr       SPR_G4_CDATA_RW, r3

    /* Calling main: Load EABI function argument registers with argc=0, argv=NULL. */
    e_lis       r3, main@h
    e_or2i      r3, main@l
    se_mtlr     r3
    se_li       r3, 0
    se_mr       r4, r3
    se_blrl

    /* We enter an infinite loop if someone should try to leave main - which is not
       intended. The external interrupts are disabled so that possible RTOS code would not
       make some other portions of code become active any more. */
    wrteei      0
su_waitForever:
    e_b         su_waitForever

/* End of sup_startUp */



/**
 *   @func zeroizeRegisters
 * First step of MCU startup is to bring all relevant registers into a defined state. In
 * LSM, it is essential do do this prior to the first write outside the sphere of
 * replication in order to avoid a false lock step error notifictation. We set all of them
 * to zero. The somewhat hidden 64 Bit character of the general purpose registers is
 * considered, the never used upper 32 Bit are cleared, too.
 *   @remark This code is based on NXP sample
 * MPC5643L-LINFlex-UART-DMA-CW210, file MPC5643L_LSM_Startup.c, l. 24ff
 */
zeroizeRegisters:

    /* Set the MSR[SPE] bit so code can use "evmergelo" instruction. */
    e_lis       r1, 0x0200
    mtmsr       r1
    se_isync                /* Core RM, 4.6: mtmsr only has completion synchronization.
                               It's not fully clear if we need dispatch or refetch
                               synchronization to make the SPE work; likely it's dispatch.
                               Anyway, isync does both. */

    /* Initialize the 64-bits of GPR0 to 0 */
    se_li       r1, 0       /* Initialize the lower 32-bits of GPR1 to 0. */
    evmergelo   r0, r1, r1  /* Write the lower 32-bits of GPR1 into both halfs of GPR0. */
    evmergelo   r1, r0, r0  /* Clear 64 Bit of r1, r2 ... */
    evmergelo   r2 ,r0 ,r0
    evmergelo   r3 ,r0 ,r0
    evmergelo   r4, r0, r0
    evmergelo   r5, r0, r0
    evmergelo   r6, r0, r0
    evmergelo   r7, r0, r0
    evmergelo   r8, r0, r0
    evmergelo   r9, r0, r0
    evmergelo   r10, r0, r0
    evmergelo   r11, r0, r0
    evmergelo   r12, r0, r0
    evmergelo   r13, r0, r0
    evmergelo   r14, r0, r0
    evmergelo   r15, r0, r0
    evmergelo   r16, r0, r0
    evmergelo   r17, r0, r0
    evmergelo   r18, r0, r0
    evmergelo   r19, r0, r0
    evmergelo   r20, r0, r0
    evmergelo   r21, r0, r0
    evmergelo   r22, r0, r0
    evmergelo   r23, r0, r0
    evmergelo   r24, r0, r0
    evmergelo   r25, r0, r0
    evmergelo   r26, r0, r0
    evmergelo   r27, r0, r0
    evmergelo   r28, r0, r0
    evmergelo   r29, r0, r0
    evmergelo   r30, r0, r0
    evmergelo   r31, r0, r0 /* ... r31 */

    mtcr        r0

    mtspr       284, r0     /* TBL     */
    mtspr       285, r0     /* TBU     */
    mtspr       272, r0     /* SPRG0-7 */
    mtspr       273, r0
    mtspr       274, r0
    mtspr       275, r0
    mtspr       276, r0
    mtspr       277, r0
    mtspr       278, r0
    mtspr       279, r0
    mtspr       604, r0     /* SPRG8-9 */
    mtspr       605, r0
    mtspr       26, r0      /* SRR0-1  */
    mtspr       27, r0
    mtspr       58, r0      /* CSRR0-1 */
    mtspr       59, r0
    mtspr       63, r0      /* IVPR    */
    mtspr       61, r0      /* DEAR    */
    mtspr       62, r0      /* ESR     */
    mtspr       570, r0     /* MCSRR0  */
    mtspr       571, r0     /* MCSRR1  */
    mtspr       340, r0     /* TCR     */
    mtspr       512, r0     /* SPEFSCR */
    mtspr       1, r0       /* XER     */
    mtspr       256, r0     /* USPRG0  */
    mtspr       9, r0       /* CTR     */
    mtspr       308, r0     /* DBCR0-6 */
    mtspr       309, r0
    mtspr       310, r0
    mtspr       561, r0
    mtspr       563, r0
    mtspr       564, r0
    mtspr       603, r0
    mtspr       574, r0     /* DSRR0-1 */
    mtspr       575, r0     /* DSRR1 */
    mtspr       400, r0     /* IVOR0-ICOR15 */
    mtspr       401, r0
    mtspr       402, r0
    mtspr       403, r0
    mtspr       404, r0
    mtspr       405, r0
    mtspr       406, r0
    mtspr       407, r0
    mtspr       408, r0
    mtspr       409, r0
    mtspr       410, r0
    mtspr       411, r0
    mtspr       412, r0
    mtspr       413, r0
    mtspr       414, r0
    mtspr       415, r0
    mtspr       528, r0     /* IVOR32-IVOR34 */
    mtspr       529, r0
    mtspr       530, r0
    mtspr       573, r0     /* MCAR  */
    mtspr       22, r0      /* DEC   */
    mtspr       54, r0      /* DECAR */
    mtspr       318, r0     /* DVC1  */
    mtspr       319, r0     /* DVC2  */
    mtspr       562, r0     /* DBCNT */
    mtspr       624, r0     /* MAS0 */
    mtspr       625, r0     /* MAS1 */
    mtspr       626, r0     /* MAS2 */
    mtspr       627, r0     /* MAS3 */
    mtspr       628, r0     /* MAS4 */
    mtspr       630, r0     /* MAS6 */

    evmra       r0, r0      /* ACC */

    /* Reset the MSR[SPE] bit. The 64 Bit SPE instructions are generally unwanted. Once we
       start using them, we need to save our CPU contexts on interrupts as full 64 Bit,
       although only very occasionally used only. */
    mtmsr       r0
    se_isync

    se_blr

/* End of zeroizeRegisters */



/**
 *   @func initExceptionHandlers
 * Set up the MCU's interrupt table. This relates to the 16 built-in exception handlers,
 * not to the external interrupts the application code will mainly use. (The external
 * interrupts are one of the 16 exceptions set here.) We assign empty handlers to most
 * exceptions. These handlers loop infinitely so that you will see a caught exception in
 * the debugger.
 */
    .extern     rtos_ivor1Handler, rtos_ivor2Handler, rtos_ivor3Handler, rtos_ivor4Handler
    .extern     rtos_ivor5Handler, rtos_ivor6Handler, rtos_ivor7Handler, rtos_ivor8Handler
    .extern     rtos_ivor13Handler, rtos_ivor14Handler, rtos_ivor15Handler, rtos_ivor32Handler
    .extern     rtos_ivor33And34Handler

initExceptionHandlers:

    /* Write the common most significant 16 bits of all the exception handler addresses. */
    e_lis       r3, sup_IVOR0trap@h
    mtspr       63, r3      /* IVPR */

    /* Write the individual bits 4..15 of the all the exception handler addresses. It
       doesn't matter to write all bits, later at exception handling time they will
       be ignored or read zero. Therefore, we need to ensure that bits 0..3 of the actual
       addresses are surely zero. This is done below by assembler statement .align. */
    e_or2i      r3, sup_IVOR0trap@l
    mtspr       400, r3     /* IVOR0 Critical input */
    
    e_lis       r3, rtos_ivor1Handler@h
    e_or2i      r3, rtos_ivor1Handler@l
    mtspr       401, r3     /* IVOR1 Machine check (non-maskable interrupt) */

    e_lis       r3, rtos_ivor2Handler@h
    e_or2i      r3, rtos_ivor2Handler@l
    mtspr       402, r3     /* IVOR2 Data storage */
    
    e_lis       r3, rtos_ivor3Handler@h
    e_or2i      r3, rtos_ivor3Handler@l
    mtspr       403, r3     /* IVOR3 Instruction storage */

    e_lis       r3, rtos_ivor4Handler@h
    e_or2i      r3, rtos_ivor4Handler@l
    mtspr       404, r3     /* IVOR4 External input */

    e_lis       r3, rtos_ivor5Handler@h
    e_or2i      r3, rtos_ivor5Handler@l
    mtspr       405, r3     /* IVOR5 Alignment */
    
    e_lis       r3, rtos_ivor6Handler@h
    e_or2i      r3, rtos_ivor6Handler@l
    mtspr       406, r3     /* IVOR6 Program */
    
    e_lis       r3, rtos_ivor7Handler@h
    e_or2i      r3, rtos_ivor7Handler@l
    mtspr       407, r3     /* IVOR7 Floating-point unavailable */

    e_lis       r3, rtos_ivor8Handler@h
    e_or2i      r3, rtos_ivor8Handler@l
    mtspr       408, r3     /* IVOR8 System call */

    e_lis       r3, sup_IVOR9trap@h
    e_or2i      r3, sup_IVOR9trap@l
    mtspr       409, r3     /* IVOR9 AP unavailable (not used by this core) */
    e_lis       r3, sup_IVOR10trap@h
    e_or2i      r3, sup_IVOR10trap@l
    mtspr       410, r3     /* IVOR10 Decrementer */
    e_lis       r3, sup_IVOR11trap@h
    e_or2i      r3, sup_IVOR11trap@l
    mtspr       411, r3     /* IVOR11 Fixed-interval timer */
    e_lis       r3, sup_IVOR12trap@h
    e_or2i      r3, sup_IVOR12trap@l
    mtspr       412, r3     /* IVOR12 Watchdog timer */
    
    e_lis       r3, rtos_ivor13Handler@h
    e_or2i      r3, rtos_ivor13Handler@l
    mtspr       413, r3     /* IVOR13 Data TLB error */
    
    e_lis       r3, rtos_ivor14Handler@h
    e_or2i      r3, rtos_ivor14Handler@l
    mtspr       414, r3     /* IVOR14 Instruction TLB error */
    
    e_lis       r3, rtos_ivor15Handler@h
    e_or2i      r3, rtos_ivor15Handler@l
    mtspr       415, r3     /* IVOR15 Debug */

    e_lis       r3, rtos_ivor32Handler@h
    e_or2i      r3, rtos_ivor32Handler@l
    mtspr       528, r3      /* IVOR32 SPE unavailable */
    
    e_lis       r3, rtos_ivor33And34Handler@h
    e_or2i      r3, rtos_ivor33And34Handler@l
    mtspr       529, r3      /* IVOR33 SPE data exception */
    
    e_lis       r3, rtos_ivor33And34Handler@h
    e_or2i      r3, rtos_ivor33And34Handler@l
    mtspr       530, r3      /* IVOR34 SPE round exception */

    /* The IVOR handlers require to restore the SDA pointers in r2 and r13 since they could
       have been clobbered by the preempted, untrusted user code. We place the values in
       the general purpose special registers, which are not accessible from user mode. In
       these registers the values stay all time long. */
    e_lis       r3, _SDA_BASE_@h
    e_or2i      r3, _SDA_BASE_@l
    mtspr       SPR_G1_SDA, r3
    e_lis       r3, _SDA2_BASE_@h
    e_or2i      r3, _SDA2_BASE_@l
    mtspr       SPR_G2_SDA2, r3

    se_blr

/* End of initExceptionHandlers */


/**
 *   @func sup_IVOR0trap
 * Exception handler IVOR0 for the Critical Input interrupt.
 */
    .global     sup_IVOR0trap   /* Note, it is not required to make this symbol global.
                                   However it is handy to see the traps in the application
                                   map file and this is why we do so for all traps. */
    .p2align 4
sup_IVOR0trap:
    se_b        sup_IVOR0trap   /* Critical Input interrupt handler */


#if 0
/**
 *   @func sup_IVOR1trap
 * Exception handler IVOR1 for the Machine check / (NMI) interrupt.
 */
    .global     sup_IVOR1trap
    .p2align 4
sup_IVOR1trap:
    se_b        sup_IVOR1trap   /* Machine check / (NMI) interrupt handler */
#endif


#if 0
/**
 *   @func sup_IVOR2trap
 * Exception handler IVOR2 for the Data Storage interrupt. This trap is taken if the MMU
 * signals an unpermitted data access.
 */
    .global     sup_IVOR2trap
    .p2align    4
sup_IVOR2trap:
    se_b        sup_IVOR2trap   /* Data Storage interrupt handler */
#endif


#if 0
/**
 *   @func sup_IVOR3trap
 * Exception handler IVOR3 for the Instruction Storage interrupt. This trap is taken if
 * the MMU signals an unpermitted instruction fetch.
 */
    .global     sup_IVOR3trap
    .p2align    4
sup_IVOR3trap:
    se_b        sup_IVOR3trap   /* Instruction Storage interrupt handler */
#endif


/* Note, there's no default IVOR4 trap since we always install a real trap handler for the
   inevitable External Interrupt handling. */


#if 0
/**
 *   @func sup_IVOR5trap
 * Exception handler IVOR5 for the Alignment interrupt.
 */
    .global     sup_IVOR5trap
    .p2align    4
sup_IVOR5trap:
    se_b        sup_IVOR5trap   /* Alignment interrupt handler */
#endif


#if 0
/**
 *   @func sup_IVOR6trap
 * Exception handler IVOR6 for the Program interrupt.
 */
    .global     sup_IVOR6trap
    .p2align    4
sup_IVOR6trap:
    se_b        sup_IVOR6trap   /* Program interrupt handler */
#endif


#if 0
/**
 *   @func sup_IVOR7trap
 * Exception handler IVOR7 for the Floating-point unavailable interrupt.
 */
    .global     sup_IVOR7trap
    .p2align    4
sup_IVOR7trap:
    se_b        sup_IVOR7trap   /* Floating-point unavailable interrupt handler */
#endif


#if 0
/**
 *   @func sup_IVOR8trap
 * Exception handler IVOR8 for the System call interrupt.
 */
    .global     sup_IVOR8trap
    .p2align    4
sup_IVOR8trap:
    se_b        sup_IVOR8trap   /* System call interrupt handler */
#endif


/**
 *   @func sup_IVOR9trap
 * Exception handler IVOR9 for the AP unavailable interrupt.
 */
    .global     sup_IVOR9trap
    .p2align    4
sup_IVOR9trap:
    se_b        sup_IVOR9trap   /* AP unavailable interrupt handler */


/**
 *   @func sup_IVOR10trap
 * Exception handler IVOR10 for the Decrementer interrupt.
 */
    .global     sup_IVOR10trap
    .p2align    4
sup_IVOR10trap:
    se_b        sup_IVOR10trap   /* Decrementer interrupt handler */


/**
 *   @func sup_IVOR11trap
 * Exception handler IVOR11 for the Fixed Interval Timer interrupt.
 */
    .global     sup_IVOR11trap
    .p2align    4
sup_IVOR11trap:
    se_b        sup_IVOR11trap   /* Fixed Interval Timer interrupt handler */


/**
 *   @func sup_IVOR12trap
 * Exception handler IVOR12 for the Watchdog Timer interrupt.
 */
    .global     sup_IVOR12trap
    .p2align    4
sup_IVOR12trap:
    se_b        sup_IVOR12trap   /* Watchdog Timer interrupt handler */


#if 0
/**
 *   @func sup_IVOR13trap
 * Exception handler IVOR13 for the Data TLB Error interrupt.
 */
    .global     sup_IVOR13trap
    .p2align    4
sup_IVOR13trap:
    se_b        sup_IVOR13trap   /* Data TLB Error interrupt handler */
#endif


#if 0
/**
 *   @func sup_IVOR14trap
 * Exception handler IVOR14 for the Instruction TLB Error interrupt.
 */
    .global     sup_IVOR14trap
    .p2align    4
sup_IVOR14trap:
    se_b        sup_IVOR14trap   /* Instruction TLB Error interrupt handler */
#endif


#if 0
/**
 *   @func sup_IVOR15trap
 * Exception handler IVOR15 for the Debug I.
 */
    .global     sup_IVOR15trap
    .p2align    4
sup_IVOR15trap:
    se_b        sup_IVOR15trap   /* Debug Interrupt */
#endif


#if 0
/**
 *   @func sup_IVOR32trap
 * Exception handler IVOR32 for the SPE unavailable interrupt.
 */
    .global     sup_IVOR32trap
    .p2align    4
sup_IVOR32trap:
    se_b        sup_IVOR32trap   /* SPE unavailable */
#endif


#if 0
/**
 *   @func sup_IVOR33trap
 * Exception handler IVOR33 for the SPE data exception interrupt.
 */
    .global     sup_IVOR33trap
    .p2align    4
sup_IVOR33trap:
    se_b        sup_IVOR33trap   /* SPE data exception */
#endif


#if 0
/**
 *   @func sup_IVOR34trap
 * Exception handler IVOR34 for the SPE round exception interrupt.
 */
    .global     sup_IVOR34trap
    .p2align    4
sup_IVOR34trap:
    se_b        sup_IVOR34trap   /* SPE round exception */
#endif

/* End of IVOR exception handlers 0..15 and 32..24 */


/**
 *   @func initMMU
 * Shape an initial MMU configuration such that all addresses can be used without the
 * danger of an exception.
 *   @remark
 * This code is based on NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file
 * MPC5643L_HWInit.c, l. 396ff.
 */
initMMU:
    mflr        r27

    /* On entry we have a single valid MMU entry, which permits executing code from the
       boot sector. This is entry 0. We must not overwrite this entry, while our code
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
    e_lis       r3, MMU_TLB1_TMP_ENTRY1_MAS0@h
    e_or2i      r3, MMU_TLB1_TMP_ENTRY1_MAS0@l
    e_lis       r4, MMU_TLB1_TMP_ENTRY1_MAS1@h
    e_or2i      r4, MMU_TLB1_TMP_ENTRY1_MAS1@l
    e_lis       r5, MMU_TLB1_TMP_ENTRY1_MAS2@h
    e_or2i      r5, MMU_TLB1_TMP_ENTRY1_MAS2@l
    e_lis       r6, MMU_TLB1_TMP_ENTRY1_MAS3@h
    e_or2i      r6, MMU_TLB1_TMP_ENTRY1_MAS3@l
    e_bl        writeMMUTableEntryS

    /* Make the new entry operate by switching to the other address space 1. */
    mfmsr       r3
    se_mr       r7, r3      /* Save current value of msr. t7 is not used in the sub-routines */
    e_or2i      r3, 0x20    /* Set IS. DS irrelevant, we must anyway not do a data access */
    mtmsr       r3          /* Does implicit isync, next instruction should be re-read
                               through our new table entry */

    /* Write intended table entry for flash ROM. It uses the normal address space 0 and is
       therefore not (yet) matching and does therefore not mean an overlapping region. The
       BAM provided boot section region is overwritten and thereby invalidated at the same
       time.
         1MB at 0x0 (flash ROM), address space 0. */
    e_lis       r3, MMU_TLB1_ENTRY0_MAS0@h
    e_or2i      r3, MMU_TLB1_ENTRY0_MAS0@l
    e_lis       r4, MMU_TLB1_ENTRY0_MAS1@h
    e_or2i      r4, MMU_TLB1_ENTRY0_MAS1@l
    e_lis       r5, MMU_TLB1_ENTRY0_MAS2@h
    e_or2i      r5, MMU_TLB1_ENTRY0_MAS2@l
    e_lis       r6, MMU_TLB1_ENTRY0_MAS3@h
    e_or2i      r6, MMU_TLB1_ENTRY0_MAS3@l
    e_bl        writeMMUTableEntry

    /* Switch back the normal address space. */
    mtmsr       r7          /* Does implicit isync, next instruction should be re-read
                               through our normal table entry, index 0. */

    /* Continue code execution in the normal address space under control of the final flash
       ROM entry 0.  */

    /* 1MB at 0xf0_0000 (shadow flash ROM). Writing this entry implicitely invalidates the
       temporary flash ROM entry for address space 1. */
    e_lis       r3, MMU_TLB1_ENTRY1_MAS0@h
    e_or2i      r3, MMU_TLB1_ENTRY1_MAS0@l
    e_lis       r4, MMU_TLB1_ENTRY1_MAS1@h
    e_or2i      r4, MMU_TLB1_ENTRY1_MAS1@l
    e_lis       r5, MMU_TLB1_ENTRY1_MAS2@h
    e_or2i      r5, MMU_TLB1_ENTRY1_MAS2@l
    e_lis       r6, MMU_TLB1_ENTRY1_MAS3@h
    e_or2i      r6, MMU_TLB1_ENTRY1_MAS3@l
    e_bl        writeMMUTableEntry

    /* 128k at 0x4000_0000 (SRAM) */
    e_lis       r3, MMU_TLB1_ENTRY2_MAS0@h
    e_or2i      r3, MMU_TLB1_ENTRY2_MAS0@l
    e_lis       r4, MMU_TLB1_ENTRY2_MAS1@h
    e_or2i      r4, MMU_TLB1_ENTRY2_MAS1@l
    e_lis       r5, MMU_TLB1_ENTRY2_MAS2@h
    e_or2i      r5, MMU_TLB1_ENTRY2_MAS2@l
    e_lis       r6, MMU_TLB1_ENTRY2_MAS3@h
    e_or2i      r6, MMU_TLB1_ENTRY2_MAS3@l
    e_bl        writeMMUTableEntryS         /* "S": We may execute some code in RAM, see
                                               e.g. initFlash */

    /* 256k at 0x8ff0_0000 (on platform 1 peripherals, from PBRIDGE_0 till STM_1) */
    e_lis       r3, MMU_TLB1_ENTRY3_MAS0@h
    e_or2i      r3, MMU_TLB1_ENTRY3_MAS0@l
    e_lis       r4, MMU_TLB1_ENTRY3_MAS1@h
    e_or2i      r4, MMU_TLB1_ENTRY3_MAS1@l
    e_lis       r5, MMU_TLB1_ENTRY3_MAS2@h
    e_or2i      r5, MMU_TLB1_ENTRY3_MAS2@l
    e_lis       r6, MMU_TLB1_ENTRY3_MAS3@h
    e_or2i      r6, MMU_TLB1_ENTRY3_MAS3@l
    e_bl        writeMMUTableEntry

    /* 64k at 0x8ff4_0000 (on platform 1 peripherals, ECSM_1 and INTC_1) */
    e_lis       r3, MMU_TLB1_ENTRY4_MAS0@h
    e_or2i      r3, MMU_TLB1_ENTRY4_MAS0@l
    e_lis       r4, MMU_TLB1_ENTRY4_MAS1@h
    e_or2i      r4, MMU_TLB1_ENTRY4_MAS1@l
    e_lis       r5, MMU_TLB1_ENTRY4_MAS2@h
    e_or2i      r5, MMU_TLB1_ENTRY4_MAS2@l
    e_lis       r6, MMU_TLB1_ENTRY4_MAS3@h
    e_or2i      r6, MMU_TLB1_ENTRY4_MAS3@l
    e_bl        writeMMUTableEntry

    /* 512k at 0xC3f8_0000 (off-platform peripherals, till STCU) */
    e_lis       r3, MMU_TLB1_ENTRY5_MAS0@h
    e_or2i      r3, MMU_TLB1_ENTRY5_MAS0@l
    e_lis       r4, MMU_TLB1_ENTRY5_MAS1@h
    e_or2i      r4, MMU_TLB1_ENTRY5_MAS1@l
    e_lis       r5, MMU_TLB1_ENTRY5_MAS2@h
    e_or2i      r5, MMU_TLB1_ENTRY5_MAS2@l
    e_lis       r6, MMU_TLB1_ENTRY5_MAS3@h
    e_or2i      r6, MMU_TLB1_ENTRY5_MAS3@l
    e_bl        writeMMUTableEntry

    /* 512k at 0xffe0_0000 (off-platform peripherals, from ADC) */
    e_lis       r3, MMU_TLB1_ENTRY6_MAS0@h
    e_or2i      r3, MMU_TLB1_ENTRY6_MAS0@l
    e_lis       r4, MMU_TLB1_ENTRY6_MAS1@h
    e_or2i      r4, MMU_TLB1_ENTRY6_MAS1@l
    e_lis       r5, MMU_TLB1_ENTRY6_MAS2@h
    e_or2i      r5, MMU_TLB1_ENTRY6_MAS2@l
    e_lis       r6, MMU_TLB1_ENTRY6_MAS3@h
    e_or2i      r6, MMU_TLB1_ENTRY6_MAS3@l
    e_bl        writeMMUTableEntry

    /* 256k at 0xfff0_0000 (on Platform 0 Peripherals, from PBRIDGE till STM) */
    e_lis       r3, MMU_TLB1_ENTRY7_MAS0@h
    e_or2i      r3, MMU_TLB1_ENTRY7_MAS0@l
    e_lis       r4, MMU_TLB1_ENTRY7_MAS1@h
    e_or2i      r4, MMU_TLB1_ENTRY7_MAS1@l
    e_lis       r5, MMU_TLB1_ENTRY7_MAS2@h
    e_or2i      r5, MMU_TLB1_ENTRY7_MAS2@l
    e_lis       r6, MMU_TLB1_ENTRY7_MAS3@h
    e_or2i      r6, MMU_TLB1_ENTRY7_MAS3@l
    e_bl        writeMMUTableEntry

    /* 64k at 0xfff4_0000 (on Platform 0 Peripherals, from ECSM till INTC) */
    e_lis       r3, MMU_TLB1_ENTRY8_MAS0@h
    e_or2i      r3, MMU_TLB1_ENTRY8_MAS0@l
    e_lis       r4, MMU_TLB1_ENTRY8_MAS1@h
    e_or2i      r4, MMU_TLB1_ENTRY8_MAS1@l
    e_lis       r5, MMU_TLB1_ENTRY8_MAS2@h
    e_or2i      r5, MMU_TLB1_ENTRY8_MAS2@l
    e_lis       r6, MMU_TLB1_ENTRY8_MAS3@h
    e_or2i      r6, MMU_TLB1_ENTRY8_MAS3@l
    e_bl        writeMMUTableEntry

    /* 64k at 0xfff9_0000 (off Platform Peripherals, only DSPI) */
    e_lis       r3, MMU_TLB1_ENTRY9_MAS0@h
    e_or2i      r3, MMU_TLB1_ENTRY9_MAS0@l
    e_lis       r4, MMU_TLB1_ENTRY9_MAS1@h
    e_or2i      r4, MMU_TLB1_ENTRY9_MAS1@l
    e_lis       r5, MMU_TLB1_ENTRY9_MAS2@h
    e_or2i      r5, MMU_TLB1_ENTRY9_MAS2@l
    e_lis       r6, MMU_TLB1_ENTRY9_MAS3@h
    e_or2i      r6, MMU_TLB1_ENTRY9_MAS3@l
    e_bl        writeMMUTableEntry

    /* 256k at 0xfffc_0000 (off Platform Peripherals, rest) */
    e_lis       r3, MMU_TLB1_ENTRY10_MAS0@h
    e_or2i      r3, MMU_TLB1_ENTRY10_MAS0@l
    e_lis       r4, MMU_TLB1_ENTRY10_MAS1@h
    e_or2i      r4, MMU_TLB1_ENTRY10_MAS1@l
    e_lis       r5, MMU_TLB1_ENTRY10_MAS2@h
    e_or2i      r5, MMU_TLB1_ENTRY10_MAS2@l
    e_lis       r6, MMU_TLB1_ENTRY10_MAS3@h
    e_or2i      r6, MMU_TLB1_ENTRY10_MAS3@l
    e_bl        writeMMUTableEntry

    mtlr        r27

    se_blr

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
writeMMUTableEntryS:

    mtspr       624, r3 /* SPR 624: MMU Assist Register 0 (MAS0) */
    mtspr       625, r4 /* SPR 625: MMU Assist Register 1 (MAS1) */
    mtspr       626, r5 /* SPR 626: MMU Assist Register 2 (MAS2) */
    mtspr       627, r6 /* SPR 627: MMU Assist Register 3 (MAS3) */

    /* Write the table entry.
         All instructions will complete here in current context. */
    msync
    tlbwe               /* Copy contents of assist registers into the MMU table entry */

    /* Synchronize instruction fetches and data accesses in respect with the new created
       TLB entry. */
    msync
    isync

    se_blr

/* End of writeMMUTableEntryS */



/**
 *   @func writeMMUTableEntry
 * Creates a new TLB entry with following instruction sequence: write GPR to MAS, execute
 * tlbwe
 *   @remark
 * This code is based on NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file
 * MPC5643L_HWInit.c, l. 241ff
 */
writeMMUTableEntry:

    mtspr       624, r3 /* SPR 624: MMU Assist Register 0 (MAS0) */
    mtspr       625, r4 /* SPR 625: MMU Assist Register 1 (MAS1) */
    mtspr       626, r5 /* SPR 626: MMU Assist Register 2 (MAS2) */
    mtspr       627, r6 /* SPR 627: MMU Assist Register 3 (MAS3) */
    tlbwe               /* Copy contents of assist registers into the MMU table entry */

    se_blr

/* End of writeMMUTableEntry */



/**
 *   @func initTimeBase
 * Initialize the MCU owned timer "Time Base", which is a free running 64 Bit high
 * resolution timer, mostly used to measure execution times. It is enabled and reset to
 * zero.
 */
initTimeBase:
    /* Reset timer */
    se_li       r0, 0
    mtspr       284, r0     /* Clear TBL */
    mtspr       285, r0     /* Clear TBU */

    /* Enable time base */
    mfspr       r0, 1008    /* Read register Hardware implementation dependent (HID) 0 */
    se_bseti    r0, 17      /* Set bit time base enable (TBEN) */
    mtspr       1008, r0    /* Write updated value back to HID0 */

    se_blr

/* End of initTimeBase */




/**
 *   @func zeroizeSRAM
 * Zeroize all SRAM cells. All bytes from ld_memRamStart and ld_memRamStart+ld_memRamSize are
 * written to zero.\n
 *   ld_memRamStart needs to be 8 Byte aligned. ld_memRamSize is rounded downwards to a
 * multiple of 8. (Remaining bytes wouldn't be touched and couldn't be used due to
 * resulting ECC errors.)
 *   @todo
 * An optimization could be zeroizing only the used RAM cells. The according labels are
 * exported by the linker. The stack portion of the RAM will anyway be initialized twice;
 * first here and later it is filled with the fill pattern required for computation of
 * stack consumption. This change would not only speed up the system start but install an
 * additional fault recognition: A read access to an unused memory address would be
 * reported as ECC error.
 */
    .extern     ld_memRamStart, ld_memRamSize
zeroizeSRAM:
    e_lis       r3, ld_memRamStart@h
    e_or2i      r3, ld_memRamStart@l
    e_lis       r4, ld_memRamSize@h
    e_or2i      r4, ld_memRamSize@l
    se_srwi     r4, 3           /* Divide length in Byte by 2^3=8 */
    se_mtctr    r4
    se_li       r31, 0
    se_li       r30, 0

zreg_loop:
    /* RM, 46.5: ECC SRAM must be initialized with 32-bit writes on 4-Byte aligned
       addresses. Here, we adopt the code from the MPC5748G, where 8 Byte writes are
       required, performed at once by an e_stmw. */
    e_stmw      r30, 0(r3)      /* Write registers 30 and 31 in one instruction, thus 8 Byte */
    se_addi     r3, 8
    e_bdnz      zreg_loop

    se_blr

/* End of zeroizeSRAM */




/**
 * Prepare some registers as required by the PowerPC eabi for the implementation of fast
 * small data access.
 */
    .extern     _SDA_BASE_, _SDA2_BASE_
initSDataRegisters:
    e_lis       r13, _SDA_BASE_@h       /* Load .sdata base into r13 */
    e_or2i      r13, _SDA_BASE_@l
    e_lis       r2, _SDA2_BASE_@h       /* Load .sdata2 base into r2 */
    e_or2i      r2, _SDA2_BASE_@l

    se_blr

/* End of initSDataRegisters */


/**
 *   @func disableWatchdog
 * Disable the watchdog.
 *   @remark
 * The assembler implementation here follows for example the implementation of NXP
 * sample NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file main.c, l. 221ff.
 */
disableWatchdog:
    /* SWT.SR.R = 0x0000c520. Write 1st key to clear soft lock bit */
    e_lis       r9, 0xfff3
    e_or2i      r9, 0x8000
    e_li        r10, 0
    e_or2i      r10, 0xc520
    e_stw       r10, 16(r9)
    /* SWT.SR.R = 0x0000d928. Write 2nd key to clear soft lock bit */
    e_li        r10, 0
    e_or2i      r10, 0xd928
    e_stw       r10, 16(r9)
    /* SWT.CR.R = 0x8000010A. Clear watchdog enable (WEN) */
    e_lis       r10, 0x8000
    e_or2i      r10, 0x10a
    e_stw       r10, 0(r9)
    /* Clear e200 Core Watchdog Timer */
    e_li        r9, 0
    mtspr       340, r9

    se_blr

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
initFlash:
    e_stwu      sp, -24(sp) /* We need 20 Byte but stack frame must be multiple of 8 (EABI) */
    mflr        r0
    e_stw       r0, 28(sp)

    /* Load the required four movable instructions into r8..r10. */
    e_lis       r10, if_movalbeMachineCode@ha
    e_add16i    r10, r10, if_movalbeMachineCode@l
    e_lwz       r8, 0(r10)
    e_lwz       r9, 4(r10)
    e_lwz       r10, 8(r10)

    /* Store moveable sub-routine in the stack area. */
    e_stw       r8, 8(sp)
    e_stw       r9, 12(sp)
    e_stw       r10, 16(sp)

    /* Load function arguments of our moveable sub-routine.
         Configuration word 0x18c7eded: 3 wait states on read and write, buffers enabled
       and prefetch of buffers on instruction read, prefetch on miss or hit. */
    e_lis       r3, 0x18c7
    e_or2i      r3, 0xeded
    e_lis       r4, 0xc3f8  /* CFLASH.PFCR0: Configuration register of flash controller */
    e_or2i      r4, 0x801c

    /* Call sub-routine, which is in the stack area. */
    e_la        r9, 8(sp)
    mtctr       r9
    se_bctrl

    e_lwz       r0, 28(sp)
    mtlr        r0
    e_la        sp, 24(sp)
    se_blr

    .align      2
if_movalbeMachineCode:
    se_isync                /* Force re-reading instructions after leaving flash. */
    e_stw       r3, 0(r4)
    msync                   /* Accomplish transaction before return to flash. */
    se_blr

/* End of initFlash */
