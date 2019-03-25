/*
 * GNU GCC Memory map for Freescale MPC5643L in Lock Step Mode
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

/* Entry Point */
ENTRY(sup_startUp)

/* Choose heap and stack sizes. Note, these sizes are lower boundaries. Due to alignment
   operations, the actual sizes may become a bit larger.
     The stack size of an used process should be set here to zero. This will switch the
   process entirely off. */
ld_heapSize  = 8k;
ld_stackSizeOS = 8k;
ld_stackSizeP1 = 2k;
ld_stackSizeP2 = 2k;
ld_stackSizeP3 = 2k;
ld_stackSizeP4 = 0;

/* Definition of memory regions using absolute MPC5643L addresses. */
MEMORY
{
    /* First 16 KB are used as boot sector. It's recomended by NXP to place all initial
       code into this sector until and including the MMU configuration. Safe code execution
       is guaranteed by the BAM and prior to the true MMU configuration only for this
       sector.
         Note, it would be adequate to define a dedicated memory segment for the BAM data
       in order to explicitly state the absolute addresses of BAM data and start of
       software. This worked fine with a definition like:
         ibamData(rx):  org = 0x00000000, len = 0x00000010
         memBoot(rx):   org = 0x00000010, len = 0x00004000 - 0x10
       and according section defintions below. However, while the SW worked well the Code
       Warrior debugger failed to find/load the assembler source code for the startup code.
       (Stepping through the code in the disassembly window worked well.) After a few
       executed instructions it managed to load the source file but there was a
       misalignment between source code window and machine instruction in the disassembly
       window. These effects disappeared when placing BAM data and startup code together
       into the same section. Now, the correct start address of the software at 0x10 is
       ensured by an .align statement in the assembler source code. */
    memBoot(rx): org = 0x00000000, len = 0x00004000

    /* 1 MB of flash memory available in Lock Step Mode. The boot code segment is
       subtracted. */
    memFlash(rx): org = 0x00004000, len = 0x00100000 - 0x4000

    /* Lock Step Mode, 128k of RAM. */
    memData(rxw): org = 0x40000000, len = 0x00020000
}

/* Export the location of the physical storage space as far as required by the startup
   code. */
ld_memRamStart = ORIGIN(memData);
ld_memRamSize = LENGTH(memData);
ld_memRamEnd = ld_memRamStart + ld_memRamSize;

SECTIONS
{
    /* The boot sector, the first 4k of ROM, which are accessible after
       reset and prior to configuration of the MMU. */
    .boot :
    {
        ld_romStart = ABSOLUTE(.);

        KEEP(*(.bamData))
        KEEP(*(.bamData.*))

        *(.boot)
        *(.boot.*)

    } >memBoot

    .text_vle :
    {
        INPUT_SECTION_FLAGS (SHF_PPC_VLE)

        /* All the MPU trap handlers need to be in the same 64k page. The boot sector
           defines the empty dummy handlers for unused traps. Implemented, true handlers
           should be located in or close to the boot sector to have them still in the same
           64k page as the dummies. We provide a dedicated section for this purpose, which
           is placed first after the boot sector. */
        *(.text.ivor)
        *(.text.ivor.*)

        /* Some empty fragments from the C library go here. */
        *(.rela.dyn)
        *(.rela.got2)
        *(.rela.text)
        *(.rela.text.*)
        *(.rela.rodata)
        *(.rela.rodata.*)

        /* Normal code segments go here. */
        *(.text.*)
        *(.text)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(16);

    } > memFlash

    INCLUDE makefile/crtSupport.ld

    .rodata :
    {
        /* Normal, const data from C source files goes here. */
        *(.rodata)
        *(.rodata.*)

        /* Leave this section 4 Byte aligned so that the subsequent load sections have this
           alignment. Our startup code requires this alignment. */
        . = ALIGN(4);

    } > memFlash


    /* The External Interrupt vector table requires an 11 Bit alignment. This is ensured
       by an assembler statement but this can mean a waste of up to 2k of RAM if the
       section location is not at such a boundary. To eliminate this risk, we place the
       table's section at absolute address at the beginning of the RAM. */
    .INTCInterruptServiceAry ORIGIN(memData) :
    {
        ld_INTCInterruptServiceAryStart = ABSOLUTE(.);
        
        *(.INTCInterruptServiceAry)
        *(.INTCInterruptServiceAry.*)

        /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
           The end address of the copied area should thus be a multiple of 4. */
        . = ALIGN(4);

        ld_INTCInterruptServiceAryEnd = ABSOLUTE(.);
        
    } >memData AT>memFlash

    .bss (NOLOAD) : ALIGN(4)
    {
        ld_ramStart = ABSOLUTE(.); /* INTC vector table is not contained. */
        ld_bssStart = ABSOLUTE(.);

        /* Uninitialized data sections for kernel and trusted I/O driver code. */
        ld_bssOSStart = ABSOLUTE(.);
        *_OS_*.o(.bss)
        *_OS_*.o(.bss.*)
        *(.bss.OS)
        *(.bss.OS.*)

        /* The size of the ring buffer used for serial output in Bit. */
        ld_noBitsDmaRingBuffer = ABSOLUTE(10);

        /* OS stack. */
        . = ALIGN(8); /* Stacks need to be 8 Byte aligned.  */
        ld_stackStartOS = ABSOLUTE(.);
        . += ld_stackSizeOS;

        /* The end of the stack would normally 8 Byte aligned. We extend the alignment to
           already fit the constraints of the next section; this way the required fill
           bytes contribute to our stack instead of being wasted. */
        . = ALIGN(1 << ld_noBitsDmaRingBuffer);
        ld_stackEndOS = ABSOLUTE(.);

        ld_bssOSEnd = ABSOLUTE(.);

        /* The DMA ring buffer is put at the end of the heap area. It has a very high
           alignment requirement: log2(sizeOfBuffer) least significant bits need to be zero
           in its address. If we align after the heap area then the gap is not lost but
           becomes additional heap space.
             Note, the section alignment hard-coded here in the file therefore limits the
           size of the buffer, which is chosen in the C source code. Assertions in the C
           code are required to ensure the crosswise consistency of C source code and
           linker control file. */
        . = ALIGN(1 << ld_noBitsDmaRingBuffer);
        *sio_serialIO.o(.dmaRingBuffer)
        *sio_serialIO.o(.dmaRingBuffer.*)

        *(COMMON)

        /* Require 32 Byte alignment for MPU. */
        . = ALIGN(32);

        ld_bssEnd = ABSOLUTE(.);

    } >memData


    .data : ALIGN(8)
    {
        ld_dataStart = ABSOLUTE(.);

        /* Data sections for kernel and trusted I/O driver code. */
        . = ALIGN(32);
        ld_dataOSStart = ABSOLUTE(.);
        *_OS_*.o(.data)
        *_OS_*.o(.data.*)
        *(.data.OS)
        *(.data.OS.*)
        . = ALIGN(32);
        ld_dataOSEnd = ABSOLUTE(.);

        /* Data sections, which are shared between all processes. */
        . = ALIGN(32);
        ld_dataSharedStart = ABSOLUTE(.);
        *_Shared_*.o(.data)
        *_Shared_*.o(.data.*)
        *(.data.Shared)
        *(.data.Shared.*)
        *_Shared_*.o(.bss)
        *_Shared_*.o(.bss.*)
        *(.bss.Shared)
        *(.bss.Shared.*)
        . = ALIGN(32);
        ld_dataSharedEnd = ABSOLUTE(.);

        /* Data sections for process 4. */
        . = ALIGN(32);
        ld_dataP4Start = ABSOLUTE(.);
        *_P4_*.o(.data)
        *_P4_*.o(.data.*)
        *(.data.P4)
        *(.data.P4.*)
        *_P4_*.o(.bss)
        *_P4_*.o(.bss.*)
        *(.bss.P4)
        *(.bss.P4.*)

        /* Stack of process 4. */
        . = ALIGN(8); /* Stacks need to be 8 Byte aligned.  */
        ld_stackStartP4 = ABSOLUTE(.);
        . += ld_stackSizeP4;
        . = ALIGN(32);
        ld_stackEndP4 = ABSOLUTE(.);

        . = ALIGN(32);
        ld_dataP4End = ABSOLUTE(.);

        /* Data sections for process 3. */
        . = ALIGN(32);
        ld_dataP3Start = ABSOLUTE(.);
        *_P3_*.o(.data)
        *_P3_*.o(.data.*)
        *(.data.P3)
        *(.data.P3.*)
        *_P3_*.o(.bss)
        *_P3_*.o(.bss.*)
        *(.bss.P3)
        *(.bss.P3.*)

        /* Stack of process 3. */
        . = ALIGN(8); /* Stacks need to be 8 Byte aligned.  */
        ld_stackStartP3 = ABSOLUTE(.);
        . += ld_stackSizeP3;
        . = ALIGN(32);
        ld_stackEndP3 = ABSOLUTE(.);

        . = ALIGN(32);
        ld_dataP3End = ABSOLUTE(.);

        /* Data sections for process 2. */
        . = ALIGN(32);
        ld_dataP2Start = ABSOLUTE(.);
        *_P2_*.o(.data)
        *_P2_*.o(.data.*)
        *(.data.P2)
        *(.data.P2.*)
        *_P2_*.o(.bss)
        *_P2_*.o(.bss.*)
        *(.bss.P2)
        *(.bss.P2.*)

        /* Stack of process 2. */
        . = ALIGN(8); /* Stacks need to be 8 Byte aligned.  */
        ld_stackStartP2 = ABSOLUTE(.);
        . += ld_stackSizeP2;
        . = ALIGN(32);
        ld_stackEndP2 = ABSOLUTE(.);

        . = ALIGN(32);
        ld_dataP2End = ABSOLUTE(.);

        /* Data sections for process 1. The implementation of the process either assigns its
           data objects explicitly to the sections we filter for here or the source files
           have the syllable _P1_ in their name. */
        . = ALIGN(32);
        ld_dataP1Start = ABSOLUTE(.);
        *_P1_*.o(.data)
        *_P1_*.o(.data.*)
        *(.data.P1)
        *(.data.P1.*)
        *_P1_*.o(.bss)
        *_P1_*.o(.bss.*)
        *(.bss.P1)
        *(.bss.P1.*)

        /* Stack of process 1. */
        . = ALIGN(8); /* Stacks need to be 8 Byte aligned.  */
        ld_stackStartP1 = ABSOLUTE(.);
        . += ld_stackSizeP1;
        . = ALIGN(32);
        ld_stackEndP1 = ABSOLUTE(.);

        /* The majority of code will always be in process 1, which is intended for QM code.
           We add the standard sections to this process. This makes writing the QM code
           most easy and the C libraries become available to this code.
             Note, as general for the processes, the sbss sections are located in the same
           area as the small initialized data sections. This is required to reduce the
           number of consecutive memory chunks and to allow static MPU programming. */
        *(.data)
        *(.data.*)
        *(.bss)
        *(.bss.*)

        /* @todo Double-check and document. These sections come from the compiler or C lib */
        *(.got2)
        *(.got2.*)
        *(.tm_clone_table)
        *(.tm_clone_table.*)
        *(.jcr)
        *(.jcr.*)
        *(.rela.data)
        *(.rela.data.*)

        /* The heap area for the C library needs to be in the area of process 1, too, if we
           want it to use the C library. */
        . = ALIGN(8);
        ld_sbrkStart = ABSOLUTE(.);
        . += ld_heapSize;
        . = ALIGN(32);
        ld_sbrkEnd = ABSOLUTE(.);

        . = ALIGN(32);
        ld_dataP1End = ABSOLUTE(.);

        /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
           The end address of the copied area should thus be a multiple of 4. */
        . = ALIGN(4);

        ld_dataEnd = ABSOLUTE(.);

    } >memData AT>memFlash


    /* Order of small data sections: The two sX and the two sX2 should be close together
       since they use the same _SDA_BASE_ pointer, or _SDA2_BASE_, respectively. */
    .sbss (NOLOAD) : ALIGN(4)
    {
        /* We should explicitly set the beginning of the SDA area. If we don't do then the
           default setting is the address of the first section, which matches *(.sdata.*).
           If we want to apply terms that filter for specific file names then these have to
           precede *(.sdata.*) and the SDA section of the file would actually be outside
           the SDA area. */
        _SDA_BASE_ = . ;
        __SDATA_START__ = . ;

        ld_sbssStart = ABSOLUTE(.);

        /* Small, uninitialized data sections for kernel and trusted I/O driver code. */
        ld_sbssOSStart = ABSOLUTE(.);
        *_OS_*.o(.sbss)
        *_OS_*.o(.sbss.*)
        *(.sbss.OS)
        *(.sbss.OS.*)
        ld_sbssOSEnd = ABSOLUTE(.);

        ld_sbssEnd = ABSOLUTE(.);

    } >memData


    .sdata : ALIGN(8)
    {
        ld_sdataStart = ABSOLUTE(.);

        /* Small data sections for kernel and trusted I/O driver code. */
        . = ALIGN(32);
        ld_sdataOSStart = ABSOLUTE(.);
        *_OS_*.o(.sdata)
        *_OS_*.o(.sdata.*)
        *(.sdata.OS)
        *(.sdata.OS.*)
        . = ALIGN(32);
        ld_sdataOSEnd = ABSOLUTE(.);

        /* Small data sections for process 4. */
        . = ALIGN(32);
        ld_sdaP4Start = ABSOLUTE(.);
        *_P4_*.o(.sdata)
        *_P4_*.o(.sdata.*)
        *(.sdata.P4)
        *(.sdata.P4.*)
        *_P4_*.o(.sbss)
        *_P4_*.o(.sbss.*)
        *(.sbss.P4)
        *(.sbss.P4.*)
        . = ALIGN(32);
        ld_sdaP4End = ABSOLUTE(.);

        /* Small data sections for process 3. */
        . = ALIGN(32);
        ld_sdaP3Start = ABSOLUTE(.);
        *_P3_*.o(.sdata)
        *_P3_*.o(.sdata.*)
        *(.sdata.P3)
        *(.sdata.P3.*)
        *_P3_*.o(.sbss)
        *_P3_*.o(.sbss.*)
        *(.sbss.P3)
        *(.sbss.P3.*)
        . = ALIGN(32);
        ld_sdaP3End = ABSOLUTE(.);

        /* Small data sections for process 2. */
        . = ALIGN(32);
        ld_sdaP2Start = ABSOLUTE(.);
        *_P2_*.o(.sdata)
        *_P2_*.o(.sdata.*)
        *(.sdata.P2)
        *(.sdata.P2.*)
        *_P2_*.o(.sbss)
        *_P2_*.o(.sbss.*)
        *(.sbss.P2)
        *(.sbss.P2.*)
        . = ALIGN(32);
        ld_sdaP2End = ABSOLUTE(.);

        /* Some empty fragments from the C library go here. */
        *(.rela.sdata)
        *(.rela.sdata.*)

        /* Small data sections for process 1. The implementation of the process either
           assigns its data objects explicitly to the sections we filter for here or the
           source files have the syllable _P1_ in their name. */
        . = ALIGN(32);
        ld_sdaP1Start = ABSOLUTE(.);
        *_P1_*.o(.sdata)
        *_P1_*.o(.sdata.*)
        *(.sdata.P1)
        *(.sdata.P1.*)
        *_P1_*.o(.sbss)
        *_P1_*.o(.sbss.*)
        *(.sbss.P1)
        *(.sbss.P1.*)

        /* The majority of code will always be in process 1, which is intended for QM code.
           We add the standard sections to this process. This makes writing the QM code
           most easy and the C libraries become available to this code.
             Note, as general for the processes, the sbss sections are located in the same
           area as the small initialized data sections. This is required to reduce the
           number of consecutive memory chunks and to allow static MPU programming. */
        *(.sdata)
        *(.sdata.*)
        *(.sbss)
        *(.sbss.*)

        . = ALIGN(32);
        ld_sdaP1End = ABSOLUTE(.);

        /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
           The end address of the copied area should thus be a multiple of 4. */
        . = ALIGN(4);

        ld_sdataEnd = ABSOLUTE(.);

    } >memData AT>memFlash


    .sbss2 (NOLOAD) : ALIGN(4)
    {
        /* We should explicitly set the beginning of the SDA2 area. If we don't do then the
           default setting is the address of the first section, which matches *(.sdata2.*).
           If we want to apply terms that filter for specific file names then these have to
           precede *(.sdata2.*) and the SDA2 section of the file would actually be outside
           the SDA2 area. */
        _SDA2_BASE_ = . ;
        __SDATA2_START__ = . ;

        ld_sbss2Start = ABSOLUTE(.);

        /* Small data 2 sections for all trusted and/or kernel code. */
        . = ALIGN(32);
        ld_sbss2OSStart = ABSOLUTE(.);
        *_OS_*.o(.sbss2)
        *_OS_*.o(.sbss2.*)
        *(.sbss2.OS)
        *(.sbss2.OS.*)
        . = ALIGN(32);
        ld_sbss2OSEnd = ABSOLUTE(.);

        ld_sbss2End = ABSOLUTE(.);

    } >memData

    .sdata2 : ALIGN(8)
    {
        ld_sdata2Start = ABSOLUTE(.);

        /* Note: Find process 1 at the end of the output section; only there it is possible
           to join its memory region with the standard region. */

        /* Small data 2 sections for all trusted and/or kernel code. */
        . = ALIGN(32);
        ld_sdata2OSStart = ABSOLUTE(.);
        *_OS_*.o(.sdata2)
        *_OS_*.o(.sdata2.*)
        *(.sdata2.OS)
        *(.sdata2.OS.*)
        . = ALIGN(32);
        ld_sdata2OSEnd = ABSOLUTE(.);

        /* Small data 2 sections for process 4. */
        . = ALIGN(32);
        ld_sda2P4Start = ABSOLUTE(.);
        *_P4_*.o(.sdata2)
        *_P4_*.o(.sdata2.*)
        *(.sdata2.P4)
        *(.sdata2.P4.*)
        *_P4_*.o(.sbss2)
        *_P4_*.o(.sbss2.*)
        *(.sbss2.P4)
        *(.sbss2.P4.*)
        . = ALIGN(32);
        ld_sda2P4End = ABSOLUTE(.);

        /* Small data 2 sections for process 3. */
        . = ALIGN(32);
        ld_sda2P3Start = ABSOLUTE(.);
        *_P3_*.o(.sdata2)
        *_P3_*.o(.sdata2.*)
        *(.sdata2.P3)
        *(.sdata2.P3.*)
        *_P3_*.o(.sbss2)
        *_P3_*.o(.sbss2.*)
        *(.sbss2.P3)
        *(.sbss2.P3.*)
        . = ALIGN(32);
        ld_sda2P3End = ABSOLUTE(.);

        /* Small data 2 sections for process 2. */
        . = ALIGN(32);
        ld_sda2P2Start = ABSOLUTE(.);
        *_P2_*.o(.sdata2)
        *_P2_*.o(.sdata2.*)
        *(.sdata2.P2)
        *(.sdata2.P2.*)
        *_P2_*.o(.sbss2)
        *_P2_*.o(.sbss2.*)
        *(.sbss2.P2)
        *(.sbss2.P2.*)
        . = ALIGN(32);
        ld_sda2P2End = ABSOLUTE(.);

        /* Small data 2 sections for process 1. The implementation of the process either
           assigns its data objects explicitly to the sections we filter for here or the
           source files have the syllable _P1_ in their name. */
        . = ALIGN(32);
        ld_sda2P1Start = ABSOLUTE(.);
        *_P1_*.o(.sdata2)
        *_P1_*.o(.sdata2.*)
        *(.sdata2.P1)
        *(.sdata2.P1.*)
        *_P1_*.o(.sbss2)
        *_P1_*.o(.sbss2.*)
        *(.sbss2.P1)
        *(.sbss2.P1.*)

        /* The majority of code will always be in process 1, which is intended for QM code.
           We add the standard sections to this process. This makes writing the QM code
           most easy and the C libraries become available to this code.
             Note, as general for the processes, the sbss sections are located in the same
           area as the small initialized data sections. This is required to reduce the
           number of consecutive memory chunks and to allow static MPU programming. */
        *(.sdata2)
        *(.sdata2.*)
        *(.sbss2)
        *(.sbss2.*)

        . = ALIGN(32);
        ld_sda2P1End = ABSOLUTE(.);

        /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
           The end address of the copied area should thus be a multiple of 4. */
        . = ALIGN(4);

        ld_sdata2End = ABSOLUTE(.);

    } >memData AT>memFlash

    /* Create a table of all initialized memory area. The startup code uses this table to
       copy the ROM mirror contents to the RAM blocks. */
    .initializeRAM_array : ALIGN(4)
    {
        ld_initializedRamBlockAry = ABSOLUTE(.);

        LONG(ld_INTCInterruptServiceAryStart)
        LONG(LOADADDR(.INTCInterruptServiceAry))
        LONG(ld_INTCInterruptServiceAryEnd - ld_INTCInterruptServiceAryStart)
        
        LONG(ld_dataStart)
        LONG(LOADADDR(.data))
        LONG(ld_dataSize)

        LONG(ld_sdataStart)
        LONG(LOADADDR(.sdata))
        LONG(ld_sdataSize)

        LONG(ld_sdata2Start)
        LONG(LOADADDR(.sdata2))
        LONG(ld_sdata2Size)

        /* The end of the list is indicated by a NULL pointer. */
        LONG(0)
        LONG(0)
        LONG(0)

    } > memFlash

    /* The DWARF debug sections are explicitly mentioned to avoid default orphan section
       plaing so that we can benefit from orphan section warnings. */
    .PPC.EMB.apuinfo 0 : { *(.PPC.EMB.apuinfo) }
    .boffs           0 : { KEEP (*(.boffs)) }
    .comment         0 : { *(.comment) }
    .gnu.attributes  0 : { *(.gnu.attributes) }
    .debug           0 : { *(.debug) }
    .debug_abbrev    0 : { *(.debug_abbrev) }
    .debug_aranges   0 : { *(.debug_aranges) }
    .debug_frame     0 : { *(.debug_frame) }
    .debug_funcnames 0 : { *(.debug_funcnames) }
    .debug_info      0 : { *(.debug_info) }
    .debug_line      0 : { *(.debug_line) }
    .debug_loc       0 : { *(.debug_loc) }
    .debug_macinfo   0 : { *(.debug_macinfo) }
    .debug_macro     0 : { *(.debug_macro) }
    .debug_pubnames  0 : { *(.debug_pubnames) }
    .debug_ranges    0 : { *(.debug_ranges) }
    .debug_sfnames   0 : { *(.debug_sfnames) }
    .debug_srcinfo   0 : { *(.debug_srcinfo) }
    .debug_str       0 : { *(.debug_str) }
    .debug_typenames 0 : { *(.debug_typenames) }
    .debug_varnames  0 : { *(.debug_varnames) }
    .debug_weaknames 0 : { *(.debug_weaknames) }
    .glink           0 : { *(.glink) }
    .iplt            0 : { *(.iplt); *(.rela.iplt) }
    .line            0 : { *(.line) }
    .version_info    0 : { *(.version_info) }

    /* Read the end of the used flash ROM into a linker label.
         The ROM end is moved to a multiple of 32 Byte. It is exported to the memory
       protection unit (MPU) configuration and this is a constrained of the MPU devices. */
    .dummyROM : { . = ALIGN(32); ld_romEnd = ABSOLUTE(.); } >memFlash

    /* Read the end of the used RAM into a linker label.
         Our assembler startup code rounds the Ram end address downwards to the next
       multiple of 16 Byte when zeroizing the RAM. To avoid non zero but used RAM cells
       we align here to the next such boundary.
         Another constraint is the MPU, which require a multiple of 32 Byte.
         @todo Enable the alignment statement if the startup code is switched to zeroize
       the used RAM only (open TODO in assembly code). Currently it zeroizes all physical
       RAM. */
    .dummyRAM : { . = ALIGN(32); ld_ramEnd = ABSOLUTE(.); } >memData

    /* Compute the used portions of RAM and ROM, so that they appear in the map file and
       can be used from the startup and application code. */
    ld_dataSize = ld_dataEnd - ld_dataStart;
    ld_bssSize = ld_bssEnd - ld_bssStart;
    ld_sdataSize = ld_sdataEnd - ld_sdataStart;
    ld_sbssSize = ld_sbssEnd - ld_sbssStart;
    ld_sdata2Size = ld_sdata2End - ld_sdata2Start;
    ld_sbss2Size = ld_sbss2End - ld_sbss2Start;

    ld_dataP1Size = ld_dataP1End - ld_dataP1Start;
    ld_sdaP1Size = ld_sdaP1End - ld_sdaP1Start;
    ld_sda2P1Size = ld_sda2P1End - ld_sda2P1Start;

    ld_dataP2Size = ld_dataP2End - ld_dataP2Start;
    ld_sdaP2Size = ld_sdaP2End - ld_sdaP2Start;
    ld_sda2P2Size = ld_sda2P2End - ld_sda2P2Start;

    ld_dataP3Size = ld_dataP3End - ld_dataP3Start;
    ld_sdaP3Size = ld_sdaP3End - ld_sdaP3Start;
    ld_sda2P3Size = ld_sda2P3End - ld_sda2P3Start;

    ld_dataP4Size = ld_dataP4End - ld_dataP4Start;
    ld_sdaP4Size = ld_sdaP4End - ld_sdaP4Start;
    ld_sda2P4Size = ld_sda2P4End - ld_sda2P4Start;

    ld_dataSharedSize = ld_dataSharedEnd - ld_dataSharedStart;

    ld_dataOSSize = ld_dataOSEnd - ld_dataOSStart;
    ld_bssOSSize = ld_bssOSEnd - ld_bssOSStart;
    ld_sdataOSSize = ld_sdataOSEnd - ld_sdataOSStart;
    ld_sbssOSSize = ld_sbssOSEnd - ld_sbssOSStart;
    ld_sdata2OSSize = ld_sdata2OSEnd - ld_sdata2OSStart;
    ld_sbss2OSSize = ld_sbss2OSEnd - ld_sbss2OSStart;

    ld_ramSize = ld_ramEnd - ld_ramStart;
    ld_romSize = ld_romEnd - ld_romStart;
}
