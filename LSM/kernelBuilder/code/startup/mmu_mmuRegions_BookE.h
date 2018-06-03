#ifndef MMU_MMUREGIONS_BOOK_E_INCLUDED
#define MMU_MMUREGIONS_BOOK_E_INCLUDED
/**
 * @file mmu_mmuRegions.h
 * Definition of registers of MMU. Macros are provided that allow to access the fields of
 * the MMU's table entries in readable and maintainable manner.
 *   @remark
 * This is not a C header file but an include file that is read by assembly code. Only
 * ordinary preprocessor constructs must be used.
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

#ifdef __VLE__
# error This file is intended for compilation for Book E instruction set only
#endif

/*
 * Include files
 */


/*
 * Defines
 */

/* The software is written as portable as reasonably possible. This requires the awareness
   of the C language standard it is compiled with. */
#if defined(__STDC_VERSION__)
# if (__STDC_VERSION__)/100 == 2011
#  define _STDC_VERSION_C11
# elif (__STDC_VERSION__)/100 == 1999
#  define _STDC_VERSION_C99
# endif
#endif

/* TODO DPM: The size of the SRAM is 64k instead of 128k and the macro to set the page
   address requires another mask (one more bit). An and additional table entry is
   required. */


/* Down here come some defines for assembling the MMU assist register MASM0. */

#define SUB_MMU_TBL_TLBSEL(tbl) (((tbl) & 0x3)<<28)
#define SUB_MMU_TBL_ESEL(esel)  (((esel) & 0x1f)<<16)

/** Use this macro to compose the value of MMU assist register MAS0 at compile time. */
#define MMU_MAS0(idxTblEntry)   (SUB_MMU_TBL_TLBSEL(1) | SUB_MMU_TBL_ESEL(idxTblEntry))



/* Down here come some defines for assembling the MMU assist register MASM1. */

#define SUP_MMU_TBL_VALID       0x80000000              // Make entry be used
#define SUP_MMU_TBL_IPROT       0x40000000              // Protection against invalidation
#define SUP_MMU_TBL_TS(as)      (((as) & 0x1)<<12)      // Address space, 0..1
#define SUP_MMU_TBL_TID(pid)    (((pid) & 0xff)<<16)    // Process ID (0 for don't care)
#define SUP_MMU_TBL_TSIZ(s)     (((s) & 0x1f)<<7)       // Size as enumeration

/** Use this macro to compose the value of MMU assist register MAS1 for flash ROM related
    table entries. The value is constant and evaluated at compile time. */
#define MMU_MAS1_FLASH          (SUP_MMU_TBL_VALID                          \
                                 | SUP_MMU_TBL_TID(/* pid */ 0)             \
                                 |  SUP_MMU_TBL_TS(/* adress space */ 0)    \
                                 | SUP_MMU_TBL_TSIZ(10 /* 1MB */)           \
                                )

/** Use this macro to compose the value of MMU assist register MAS1 for the SRAM related
    table entry/ies. The value is constant and evaluated at compile time. */
#define MMU_MAS1_SRAM           (SUP_MMU_TBL_VALID                          \
                                 | SUP_MMU_TBL_TID(/* pid */ 0)             \
                                 |  SUP_MMU_TBL_TS(/* adress space */ 0)    \
                                 | SUP_MMU_TBL_TSIZ(7 /* 128k */)           \
                                )

/** Use this macro to compose the value of MMU assist register MAS1 for table entries that
    control the access to memory mapped peripherals. The value is constant and evaluated at
    compile time.
      @param s The size of the memory area as power of 2: noBytes = 2^s kByte */
#define MMU_MAS1_PERIPHERALS(s) (SUP_MMU_TBL_VALID                          \
                                 | SUP_MMU_TBL_TID(/* pid */ 0)             \
                                 |  SUP_MMU_TBL_TS(/* adress space */ 0)    \
                                 | SUP_MMU_TBL_TSIZ(s)                      \
                                )


/* Down here come some defines for assembling the MMU assist register MASM2. */

#define SUP_MMU_TBL_VLE     0x20    /// VLE page indication
#define SUP_MMU_TBL_W       0x10    /// Cache: Write-through
#define SUP_MMU_TBL_I       0x08    /// Cache inhibit
#define SUP_MMU_TBL_M       0x04    /// Memory coherence required
#define SUP_MMU_TBL_G       0x02    /// Guarded against bus cycle abortion
#define SUP_MMU_TBL_E       0x01    /// Endianess

/** Use this macro to compose the value of MMU assist register MAS2 for flash ROM related
    table entries. The value is constant and evaluated at compile time.
      @param a The address of the memory area; used for both effective and physical
    address; there's no translation involved. */
#define MMU_MAS2_FLASH(a)       (((a) & 0xfff00000) | (0))

/** Use this macro to compose the value of MMU assist register MAS2 for the SRAM related
    table entry/ies. The value is constant and evaluated at compile time.
      @param a The address of the memory area; used for both effective and physical
    address; there's no translation involved. */
#define MMU_MAS2_SRAM(a)        (((a) & 0xffffe000) | (SUP_MMU_TBL_I))

/** Use this macro to compose the value of MMU assist register MAS2 for table entries that
    control the access to memory mapped peripherals. The value is constant and evaluated at
    compile time.
      @param a The address of the memory area; used for both effective and physical
    address; there's no translation involved.\n
      Note, the address must not contain more non-zero, most significant bits as permitted
    for the chosen region size s: 32-log2(s/Byte). */
#define MMU_MAS2_PERIPHERALS(a) (((a) & 0xfffff000) | (SUP_MMU_TBL_I|SUP_MMU_TBL_G))


/* Down here come some defines for assembling the MMU assist register MASM3. */

#define SUP_MMU_TBL_SR      0x01    /// Read for supervisor
#define SUP_MMU_TBL_UR      0x02    /// Read for user
#define SUP_MMU_TBL_SW      0x04    /// Write for supervisor
#define SUP_MMU_TBL_UW      0x08    /// Write for user
#define SUP_MMU_TBL_SX      0x10    /// Execute for supervisor
#define SUP_MMU_TBL_UX      0x20    /// Execute for user

/** Use this macro to compose the value of MMU assist register MAS1 for flash ROM related
    table entries. The value is constant and evaluated at compile time.
      @param a The address of the memory area; used for both effective and physical
    address; there's no translation involved. */
#define MMU_MAS3_FLASH(a)  (((a) & 0xfff00000)                                  \
                            | SUP_MMU_TBL_SR | SUP_MMU_TBL_SW | SUP_MMU_TBL_SX  \
                            | SUP_MMU_TBL_UR | SUP_MMU_TBL_UW | SUP_MMU_TBL_UX  \
                           )

/** Use this macro to compose the value of MMU assist register MAS3 for the SRAM related
    table entry/ies. The value is constant and evaluated at compile time.
      @param a The address of the memory area; used for both effective and physical
    address; there's no translation involved. */
#define MMU_MAS3_SRAM(a)   (((a) & 0xffffe000)                                  \
                            | SUP_MMU_TBL_SR | SUP_MMU_TBL_SW | SUP_MMU_TBL_SX  \
                            | SUP_MMU_TBL_UR | SUP_MMU_TBL_UW | SUP_MMU_TBL_UX  \
                           )

/** Use this macro to compose the value of MMU assist register MAS3 for table entries that
    control the access to memory mapped peripherals. The value is constant and evaluated at
    compile time.
      @param a The address of the memory area; used for both effective and physical
    address; there's no translation involved. */
#define MMU_MAS3_PERIPHERALS(a) (((a) & 0xfffff000)                 \
                                 | SUP_MMU_TBL_SR | SUP_MMU_TBL_SW  \
                                 | SUP_MMU_TBL_UR | SUP_MMU_TBL_UW  \
                                )

/* 1MB at 0x0 (flash ROM) */
#define MMU_TLB1_ENTRY0_MAS0 MMU_MAS0(/* idxTblEntry */ 0)
#define MMU_TLB1_ENTRY0_MAS1 MMU_MAS1_FLASH
#define MMU_TLB1_ENTRY0_MAS2 MMU_MAS2_FLASH(/* address */ 0x00000000)
#define MMU_TLB1_ENTRY0_MAS3 MMU_MAS3_FLASH(/* address */ 0x00000000)

/* 1MB at 0x0 (flash ROM). This is  nearly a copy of the definition of entry 0. It uses
   however the other table entry 1 and the other address space.
     Reason: This table entry is use temporarily in order to avoid overlapping region
   definitions at any time during initialization. */
#define MMU_TLB1_TMP_ENTRY1_MAS0 MMU_MAS0(/* idxTblEntry */ 1)
#define MMU_TLB1_TMP_ENTRY1_MAS1 (MMU_MAS1_FLASH & ~SUP_MMU_TBL_TS(0) | SUP_MMU_TBL_TS(1))
#define MMU_TLB1_TMP_ENTRY1_MAS2 MMU_MAS2_FLASH(/* address */ 0x00000000)
#define MMU_TLB1_TMP_ENTRY1_MAS3 MMU_MAS3_FLASH(/* address */ 0x00000000)

/* 1MB at 0xf0_0000 (shadow flash ROM) */
#define MMU_TLB1_ENTRY1_MAS0 MMU_MAS0(/* idxTblEntry */ 1)
#define MMU_TLB1_ENTRY1_MAS1 MMU_MAS1_FLASH
#define MMU_TLB1_ENTRY1_MAS2 MMU_MAS2_FLASH(/* address */ 0x00f00000)
#define MMU_TLB1_ENTRY1_MAS3 MMU_MAS3_FLASH(/* address */ 0x00f00000)

/* 128k at 0x4000_0000 (SRAM) */
#define MMU_TLB1_ENTRY2_MAS0 MMU_MAS0(/* idxTblEntry */ 2)
#define MMU_TLB1_ENTRY2_MAS1 MMU_MAS1_SRAM
#define MMU_TLB1_ENTRY2_MAS2 MMU_MAS2_SRAM(/* address */ 0x40000000)
#define MMU_TLB1_ENTRY2_MAS3 MMU_MAS3_SRAM(/* address */ 0x40000000)

/* 256k at 0x8ff0_0000 (on platform 1 peripherals, from PBRIDGE_0 till STM_1) */
#define MMU_TLB1_ENTRY3_MAS0 MMU_MAS0(/* idxTblEntry */ 3)
#define MMU_TLB1_ENTRY3_MAS1 MMU_MAS1_PERIPHERALS(/* size */ 8 /* 256k */)
#define MMU_TLB1_ENTRY3_MAS2 MMU_MAS2_PERIPHERALS(/* address */ 0x8ff00000)
#define MMU_TLB1_ENTRY3_MAS3 MMU_MAS3_PERIPHERALS(/* address */ 0x8ff00000)

/* 64k at 0x8ff4_0000 (on platform 1 peripherals, ECSM_1 and INTC_1) */
#define MMU_TLB1_ENTRY4_MAS0 MMU_MAS0(/* idxTblEntry */ 4)
#define MMU_TLB1_ENTRY4_MAS1 MMU_MAS1_PERIPHERALS(/* size */ 6 /* 64k */)
#define MMU_TLB1_ENTRY4_MAS2 MMU_MAS2_PERIPHERALS(/* address */ 0x8ff40000)
#define MMU_TLB1_ENTRY4_MAS3 MMU_MAS3_PERIPHERALS(/* address */ 0x8ff40000)

/* 512k at 0xC3f8_0000 (off-platform peripherals, till STCU) */
#define MMU_TLB1_ENTRY5_MAS0 MMU_MAS0(/* idxTblEntry */ 5)
#define MMU_TLB1_ENTRY5_MAS1 MMU_MAS1_PERIPHERALS(/* size */ 9 /* 512k */)
#define MMU_TLB1_ENTRY5_MAS2 MMU_MAS2_PERIPHERALS(/* address */ 0xc3f80000)
#define MMU_TLB1_ENTRY5_MAS3 MMU_MAS3_PERIPHERALS(/* address */ 0xc3f80000)

/* 512k at 0xffe0_0000 (off-platform peripherals, from ADC) */
#define MMU_TLB1_ENTRY6_MAS0 MMU_MAS0(/* idxTblEntry */ 6)
#define MMU_TLB1_ENTRY6_MAS1 MMU_MAS1_PERIPHERALS(/* size */ 9 /* 512k */)
#define MMU_TLB1_ENTRY6_MAS2 MMU_MAS2_PERIPHERALS(/* address */ 0xffe00000)
#define MMU_TLB1_ENTRY6_MAS3 MMU_MAS3_PERIPHERALS(/* address */ 0xffe00000)

/* 256k at 0xfff0_0000 (on Platform 0 Peripherals, from PBRIDGE_0 till STM_0) */
#define MMU_TLB1_ENTRY7_MAS0 MMU_MAS0(/* idxTblEntry */ 7)
#define MMU_TLB1_ENTRY7_MAS1 MMU_MAS1_PERIPHERALS(/* size */ 8 /* 256k */)
#define MMU_TLB1_ENTRY7_MAS2 MMU_MAS2_PERIPHERALS(/* address */ 0xfff00000)
#define MMU_TLB1_ENTRY7_MAS3 MMU_MAS3_PERIPHERALS(/* address */ 0xfff00000)

/* 64k at 0xfff4_0000 (on Platform 0 Peripherals, from ECSM_0 till INTC_0) */
#define MMU_TLB1_ENTRY8_MAS0 MMU_MAS0(/* idxTblEntry */ 8)
#define MMU_TLB1_ENTRY8_MAS1 MMU_MAS1_PERIPHERALS(/* size */ 6 /* 64k */)
#define MMU_TLB1_ENTRY8_MAS2 MMU_MAS2_PERIPHERALS(/* address */ 0xfff40000)
#define MMU_TLB1_ENTRY8_MAS3 MMU_MAS3_PERIPHERALS(/* address */ 0xfff40000)

/* 64k at 0xfff9_0000 (off Platform Peripherals, only DSPI) */
#define MMU_TLB1_ENTRY9_MAS0 MMU_MAS0(/* idxTblEntry */ 9)
#define MMU_TLB1_ENTRY9_MAS1 MMU_MAS1_PERIPHERALS(/* size */ 6 /* 64k */)
#define MMU_TLB1_ENTRY9_MAS2 MMU_MAS2_PERIPHERALS(/* address */ 0xfff90000)
#define MMU_TLB1_ENTRY9_MAS3 MMU_MAS3_PERIPHERALS(/* address */ 0xfff90000)

/* 256k at 0xfffc_0000 (off Platform Peripherals, rest) */
#define MMU_TLB1_ENTRY10_MAS0 MMU_MAS0(/* idxTblEntry */ 10)
#define MMU_TLB1_ENTRY10_MAS1 MMU_MAS1_PERIPHERALS(/* size */ 8 /* 256k */)
#define MMU_TLB1_ENTRY10_MAS2 MMU_MAS2_PERIPHERALS(/* address */ 0xfffc0000)
#define MMU_TLB1_ENTRY10_MAS3 MMU_MAS3_PERIPHERALS(/* address */ 0xfffc0000)


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */



#endif  /* MMU_MMUREGIONS_BOOK_E_INCLUDED */
