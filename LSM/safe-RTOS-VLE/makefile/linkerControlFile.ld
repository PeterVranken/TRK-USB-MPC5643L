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
    .boot : ALIGN(4)
    {
        ld_romStart = ABSOLUTE(.);

        KEEP(*(.bamData))
        KEEP(*(.bamData.*))

        *(.boot)
        *(.boot.*)

    } >memBoot


    .text_vle : ALIGN(4)
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
        *(.rela.*)

        /* Normal code segments go here. */
        *(.text.*)
        *(.text)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(16);

    } > memFlash

    INCLUDE makefile/crtSupport.ld

    .rodata : ALIGN(4)
    {
        /* Normal, const data from C source files goes here. */
        *(.rodata)
        *(.rodata.*)

        /* Leave this section 4 Byte aligned so that the subsequent load sections have this
           alignment. Our startup code requires this alignment. */
        . = ALIGN(4);

    } > memFlash

    /* Read the beginning of the used RAM into a linker label. */
    .dummyRAMInit (NOLOAD) : { ld_ramStart = ABSOLUTE(.); } >memData

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

    INCLUDE makefile/processRAM.ld

    /* Create a table of all initialized memory area. The startup code uses this table to
       copy the ROM mirror contents to the RAM blocks. */
    .initializeRAM_array : ALIGN(4)
    {
        ld_initializedRamBlockAry = ABSOLUTE(.);

        LONG(ld_INTCInterruptServiceAryStart)
        LONG(LOADADDR(.INTCInterruptServiceAry))
        LONG(ld_INTCInterruptServiceAryEnd - ld_INTCInterruptServiceAryStart)

        INCLUDE makefile/processRAMInitAry.ld

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

    /* Read the end of the used flash ROM into a linker label.
         The ROM end is moved to a multiple of 32 Byte. It is exported to the memory
       protection unit (MPU) configuration and this is a constrained of the MPU devices. */
    .dummyROM : { . = ALIGN(32); ld_romEnd = ABSOLUTE(.); } >memFlash

    /* Read the end of the used RAM into a linker label.
         Our assembler startup code rounds the Ram end address downwards to the next
       multiple of 16 Byte when zeroizing the RAM. To avoid non zero but used RAM cells
       we align here to the next such boundary.
         Another constraint is the MPU, which requires a multiple of 32 Byte. */
    .dummyRAM (NOLOAD) : { . = ALIGN(32); ld_ramEnd = ABSOLUTE(.); } >memData

    /* Compute the sizes of the memory chunks of the processes, which are configured in the
       MPU. The computed sizes are not required in the code but it is useful to have them
       listed in the map file. */
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

    /* Compute the used portions of RAM and ROM, so that they appear in the map file and
       can be used from the startup and application code. */
    ld_ramSize = ld_ramEnd - ld_ramStart;
    ld_romSize = ld_romEnd - ld_romStart;
}
