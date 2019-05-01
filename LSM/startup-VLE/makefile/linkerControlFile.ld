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

/* Definition of memory regions using absolute MPC5643L addresses. */
MEMORY
{
    /* First 16 KB are used as boot sector. It's recomended by NXP to place all initial
       code into this sector until and including the MPU configuration. Safe code execution
       is guaranteed by the BAM and prior to the true MPU configuration only for this
       sector.
         Note, it would be adequate to define a dedicated memory segment for the BAM data
       in order to explicitly state the absolute addresses of BAM data and start of
       software. This worked fine with a definition like:
         ibamData(rx):  org = 0x00000000, len = 0x00000010
         memBoot(rx):   org = 0x00000010, len = 0x00004000 - 0x10
       and according section defintions below. However, while the SW worked well the Code
       Warrior debugger failed to find/load the assembler source code for the startup code.
       (Stepping through the code in the diassembly window worked well.) After a few
       executed instructions it managed to load the source file but there was a
       misalignment between source code window and machine instruction in the disassembly
       window. These effects disappeared when placing BAM data and startup code together
       into the same section. Now, the correct start address of the software at 0x10 is
       ensured by an .align statement in the assembler source code. */
    memBoot(rx):    org = 0x00000000, len = 0x00004000

    /* 1 MB of flash memory available in Lock Step Mode. The boot code segment is
       subtracted. */
    memFlash(rx):   org = 0x00004000, len = 0x00100000 - 0x4000

    /* Lock Step Mode, 128k of RAM. It is distributed into several chunks because we want
       to have chunks at specific addresses and/or want them to have a specific size. */
    memData(rxw):   org = 0x40000000,
                    len = 0x00020000 - 0x1800 - 0x4000
    memHeap(rxw):   org = 0x40000000 + (0x00020000 - 0x1800 - 0x4000),
                    len = 0x1800
    memStack(rxw):  org = 0x40000000 + (0x00020000 - 0x4000),
                    len = 0x4000
}

/* Export the location of the physical storage space. */
ld_memBootStart = ORIGIN(memBoot);
ld_memBootSize = LENGTH(memBoot);
ld_memBootEnd = ld_memBootStart + ld_memBootSize;

ld_memFlashStart = ORIGIN(memFlash);
ld_memFlashSize = LENGTH(memFlash);
ld_memFlashEnd = ld_memFlashStart + ld_memFlashSize;

ld_memDataStart = ORIGIN(memData);
ld_memDataSize = LENGTH(memData);
ld_memDataEnd = ld_memDataStart + ld_memDataSize;

ld_memHeapStart = ORIGIN(memHeap);
ld_memHeapSize = LENGTH(memHeap);
ld_memHeapEnd = ld_memHeapStart + ld_memHeapSize;

ld_memStackStart = ORIGIN(memStack);
ld_memStackSize = LENGTH(memStack);
ld_memStackEnd = ld_memStackStart + ld_memStackSize;

ld_memRamStart = ORIGIN(memData);
ld_memRamSize = LENGTH(memData) + LENGTH(memHeap) + LENGTH(memStack);
ld_memRamEnd = ld_memRamStart + ld_memRamSize;


SECTIONS
{
    .boot :
    {
        ld_romStart = . ;

        KEEP(*(.bamData))
        KEEP(*(.bamData.*))

        *(.boot)
        *(.boot.*)

    } >memBoot


    .text_vle :
    {
        INPUT_SECTION_FLAGS (SHF_PPC_VLE)

        /* Some empty fragments from the C library go here. */
        *(.rela.dyn)
        *(.rela.got2)
        *(.rela.text)
        *(.rela.text.*)
        *(.rela.rodata.*)
        *(.rela.*)

        /* All the MPU trap handlers need to be in the same 64k page. The boot sector
           defines the empty dummy handlers for unused traps. Implemented, real handlers
           should be located in or close to the boot sector to have them still in the same
           64k page as the dummies. We provide a dedicated section for this purpose, which
           is placed first after the boot sector. */
        *(.text.ivor)
        *(.text.ivor.*)

        *(.text.*)
        *(.text)
        KEEP(*(.init))
        KEEP(*(.fini))

    } >memFlash

    INCLUDE makefile/crtSupport.ld

    .rodata :
    {
        *(.rodata)
        *(.rodata.*)

        /* Leave this section 4 Byte aligned so that the subsequent load sections have this
           alignment. Our startup code requires this alignment. */
        . = ALIGN(4);

    } >memFlash


    /* The compiler's default section for initialized data. */
    .data : ALIGN(8)
    {
        ld_ramStart = . ;
        ld_dataStart = . ;

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

        /* Our startup code copies one chunk of ROM to RAM to fill all initialized data
           sections. The linker doesn't know about this and does not guarantee that the
           gaps between the initialized RAM sections are identical to those between the
           related load sections in ROM. If this is not the case then the 2nd RAM section
           will contain bad data. We can avoid this by aligning the end of the first
           section such that the linker is satisfied and doesn't require any gap.
             @todo Note, this is just a workaround. We need to copy all load sections
           separately in the startup code. */
        . = ALIGN(8) ;

    } >memData AT>memFlash


    /* Order of small data sections: The two sX and the two sX2 should be close together
       since they use the same _SDA_BASE_ pointer, or _SDA2_BASE_, respectively. */
    .sdata  : ALIGN(8)
    {
        /* Some empty fragments from the C library go here. */
        *(.rela.sdata)
        *(.rela.sdata.*)

        *(.sdata)
        *(.sdata.*)

        /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
           The end address of the copied area should thus be a multiple of 4. */
        . = ALIGN(4);

        ld_dataEnd = . ;

    } >memData AT>memFlash =0xae


    /* The uninitialized RAM sections follow. These are the compiler's default sections for
       uninitialized data. */
    .sbss   (NOLOAD) : ALIGN(4)
    {
        *(.sbss)
        *(.sbss.*)
    } >memData

    .bss    (NOLOAD) : ALIGN(4)
    {
        *(.bss)
        *(.bss.*)

        *(COMMON)

    } >memData
    

    /* Order of small data sections: The two sX and the two sX2 should be close together
       since they use the same _SDA_BASE_ pointer, or _SDA2_BASE_, respectively. */
    .sdata2 :
    {
        *(.sdata2)
        *(.sdata2.*)

        /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
           The end address of the copied area should thus be a multiple of 4. */
        . = ALIGN(4);

    } >memData AT>memFlash

    .sbss2  (NOLOAD) :
    {
        /* @todo NXP document "Power PC Embedded Application Binary Interface (EABI):
           32-Bit Implementation", p. 11: ".sbss2 is intended to hold writable small data"
           At another location the same documents says that the same base pointer
           _SDA2_BASE_ is used to address both sections in a 64k area. This conflicts with
           holding sdata2 in flash ROM and sbss2 in RAM. The specification permits holding
           sdata2 in RAM, too, and this seems to be the only chance to conform with all
           requirements. Unfortunately, on cost of using expensive RAM for const data.
           Until we do not see the compiler placing writable data into sbss2 we are
           continuing with a flash ROM allocation. If it would ever happen then we had to
           either move both sections to RAM or not to use SDA2 at all (which is a matter of
           the compiler command line). */
        *(.sbss2)
        *(.sbss2.*)

        ld_sbss2End = . ;

    } >memData

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
    ld_dataSize = ld_dataEnd - ld_dataStart ;
    ld_ramSize = ld_ramEnd - ld_ramStart ;
    ld_dataMirrorStart = LOADADDR(.data);
    ld_dataMirrorEnd   = LOADADDR(.sdata) + SIZEOF(.sdata);
    ld_dataMirrorSize = ld_dataMirrorEnd - ld_dataMirrorStart;
    ld_romSize = ld_romEnd - ld_romStart ;
}
