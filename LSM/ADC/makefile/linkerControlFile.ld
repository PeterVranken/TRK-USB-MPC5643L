/*
 * GNU GCC Memory map for Freescale MPC5643L in Lock Step Mode
 *
 * Copyright (C) 2017-2022 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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

/* Choose heap and stack size. Note, these sizes are lower boundaries. Due to alignment
   operations, the actual sizes may become a bit larger. */
ld_heapSize  = 8k;
ld_stackSize = 8k;

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
    memBoot(rx):    org = 0x00000000, len = 0x00004000

    /* 1 MB of flash memory available in Lock Step Mode. The boot code segment is
       subtracted. */
    memFlash(rx):   org = 0x00004000, len = 0x00100000 - 0x4000

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

    .text : ALIGN(4)
    {
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
        *(.rela.*)

        /* Normal code segments go here. */
        *(.text.*)
        *(.text)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);

    } >memFlash

    INCLUDE makefile/crtSupport.ld

    .rodata : ALIGN(4)
    {
        /* Normal, const data from C source files goes here. */
        *(.rodata)
        *(.rodata.*)

        /* Leave this section 4 Byte aligned so that the subsequent load sections have this
           alignment. Our startup code requires this alignment. */
        . = ALIGN(4);

    } >memFlash


    /* The order of data sections matters because of some constraints:
         - The two sX and the two sX2 should be close together since they use the same
       _SDA_BASE_ pointer, or _SDA2_BASE_, respectively
         - The interrupt vector table is initialized data and needs to be on a 16 Bit
       boundary (HW constraint). If we don't want to waste alignment memory then we need to
       put it at the very beginning */

    /* The compiler's default section for initialized data. */
    .data : ALIGN(8)
    {
        ld_ramStart = . ;
        ld_dataStart = ABSOLUTE(.);


        /* The External Interrupt vector table requires a 16 Bit alignment. This is ensured
           by an assembler statement but this can mean a waste of up to 64k of RAM if the
           section location is not at such a boundary. To reduce this risk, we place the
           table's section as very first, which is the beginning of RAM and fine.
             @todo A better solution would be the introduction of a dedicated memory
           segment at address 0x40000000 and of required length 256*4 Byte. */
        *(.INTCInterruptHandlerAry)
        *(.INTCInterruptHandlerAry.*)

        *(.data)
        *(.data.*)

        *(.got2)
        *(.got2.*)
        *(.tm_clone_table)
        *(.tm_clone_table.*)
        *(.jcr)
        *(.jcr.*)
        *(.rela.data)
        *(.rela.data.*)

        /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
           The end address of the copied area should thus be a multiple of 4. */
        . = ALIGN(4);

        ld_dataEnd = ABSOLUTE(.);

    } >memData AT>memFlash


    .bss (NOLOAD) : ALIGN(4)
    {
        *(.bss)
        *(.bss.*)

        *(COMMON)

        /* The size of the ring buffer used for serial output in Bit. */
        ld_noBitsDmaRingBuffer = ABSOLUTE(10);

        /* Main stack. */
        . = ALIGN(8); /* Stacks need to be 8 Byte aligned.  */
        ld_stackStart = ABSOLUTE(.);
        . += ld_stackSize;

        /* The end of the stack would normally 8 Byte aligned. We extend the alignment to
           already fit the constraints of the next section; this way the required fill
           bytes contribute to our stack instead of being wasted. */
        . = ALIGN(1 << ld_noBitsDmaRingBuffer);
        ld_stackEnd = ABSOLUTE(.);

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

        /* The heap area for the C library. */
        . = ALIGN(8) ;
        ld_sbrkStart = ABSOLUTE(.);
        . += ld_heapSize;
        . = ALIGN(32);
        ld_sbrkEnd = ABSOLUTE(.);

    } >memData


    /* Order of small data sections: The two sX and the two sX2 should be close together
       since they use the same _SDA_BASE_ pointer, or _SDA2_BASE_, respectively. */
    .sdata  : ALIGN(8)
    {
        ld_sdataStart = ABSOLUTE(.);
        _SDA_BASE_ = ABSOLUTE(ld_sdataStart) + 0x8000 ;

        /* Some empty fragments from the C library go here. */
        *(.rela.sdata)
        *(.rela.sdata.*)

        *(.sdata)
        *(.sdata.*)

        /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
           The end address of the copied area should thus be a multiple of 4. */
        . = ALIGN(4);

        ld_sdataEnd = ABSOLUTE(.);

    } >memData AT>memFlash =0xae


    /* The uninitialized RAM sections follow. These are the compiler's default sections for
       uninitialized data. */
    .sbss   (NOLOAD) : ALIGN(4)
    {
        *(.sbss)
        *(.sbss.*)
    } >memData


    /* Order of small data sections: The two sX and the two sX2 should be close together
       since they use the same _SDA_BASE_ pointer, or _SDA2_BASE_, respectively. */
    .sdata2 : ALIGN(4)
    {
        ld_sdata2Start = ABSOLUTE(.);
        _SDA2_BASE_ = ABSOLUTE(ld_sdata2Start) + 0x8000 ;

        *(.sdata2)
        *(.sdata2.*)

        /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
           The end address of the copied area should thus be a multiple of 4. */
        . = ALIGN(4);

        ld_sdata2End = ABSOLUTE(.);

    } >memData AT>memFlash


    /* The uninitialized RAM sections follow. These are the compiler's default sections for
       uninitialized data. */
    .sbss2 (NOLOAD) : ALIGN(4)
    {
        /* @todo NXP document "Power PC Embedded Application Binary Interface (EABI):
           32-Bit Implementation", p. 11: ".sbss2 is intended to hold writable small data"
           At another location the same documents says that the same base pointer
           _SDA2_BASE_ is used to address both sections in a 64k area. This conflicts with
           holding sdata2 in flash ROM and sbss2 in RAM. The specification permits holding
           sdata2 in RAM, too, and this seems to be the only chance to conform with all
           requirements. Unfortunately, on cost of using expensive RAM for const data. */
        *(.sbss2)
        *(.sbss2.*)

    } >memData


    /* Create a table of all initialized memory area. The startup code uses this table to
       copy the ROM mirror contents to the RAM blocks. */
    .initializeRAM_array : ALIGN(4)
    {
        ld_initializedRamBlockAry = ABSOLUTE(.);

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
       placing so that we can benefit from orphan section warnings. */
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
    
    /* Read the end of the used flash ROM into a linker label. */
    .dummyROM : { ld_romEnd = ABSOLUTE(.); } >memFlash
    
    /* Read the end of the used RAM into a linker label.
         Our assembler startup code rounds the Ram end address downwards to the next
       multiple of 16 Byte when zeroizing the RAM. To avoid non zero but used RAM cells
       we align here to the next such boundary.
         @todo Enable the alignment statement if the startup code is switched to zeroize
       the used RAM only (open TODO in assembly code). Currently it zeroizes all physical
       RAM. */
    .dummyRAM : { /* . = ALIGN(16); */ ld_ramEnd = ABSOLUTE(.); } >memData

    /* Compute the used portions of RAM and ROM, so that they appear in the map file and
       can be used from the startup and application code. */
    ld_dataSize = ld_dataEnd - ld_dataStart;
    ld_sdataSize = ld_sdataEnd - ld_sdataStart;
    ld_sdata2Size = ld_sdata2End - ld_sdata2Start;
    ld_ramSize = ld_ramEnd - ld_ramStart;
    ld_romSize = ld_romEnd - ld_romStart;
}
