/*
 * This sub-ordinated linker script file specifies the RAM partitioning for the processes.
 *   It has been generated from the information in spreadsheet processes_ld.xlsx, worksheet
 * Processes.
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
.data : ALIGN(32)
{
    /* Data sections for kernel and trusted I/O driver code. */
    . = ALIGN(32);
    *_OS_*.o(.data)
    *_OS_*.o(.data.*)
    *(.data.OS)
    *(.data.OS.*)
    . = ALIGN(32);

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

    /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
       The end address of any area to copy should thus be a multiple of 4. */
    . = ALIGN(4);

} >memData AT>memFlash

.bss (NOLOAD) : ALIGN(32)
{
    /* Uninitialized data sections for kernel and trusted I/O driver code. */
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

    /* The DMA ring buffer is put at the end of the heap area. It has a very high
       alignment requirement: log2(sizeOfBuffer) least significant bits need to be zero
       in its address. If we align after the heap area then the gap is not lost but
       becomes additional heap space.
         Note, the section alignment hard-coded here in the file therefore limits the
       size of the buffer, which is chosen in the C source code. Assertions in the C
       code are required to ensure the crosswise consistency of C source code and
       linker control file. */
    . = ALIGN(1 << ld_noBitsDmaRingBuffer);
    *sio_serialIO.o(.sio_bss)
    *sio_serialIO.o(.sio_bss.*)

    *(COMMON)

} >memData


.data.P4 : ALIGN(32)
{
    /* Data sections for process P4. */
    . = ALIGN(32);
    ld_dataP4Start = ABSOLUTE(.);

    *_P4_*.o(.data)
    *_P4_*.o(.data.*)
    *(.data.P4)
    *(.data.P4.*)


    /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
       The end address of any area to copy should thus be a multiple of 4. */
    . = ALIGN(4);

} >memData AT>memFlash

.bss.P4 (NOLOAD) : ALIGN(32)
{
    *_P4_*.o(.bss)
    *_P4_*.o(.bss.*)
    *(.bss.P4)
    *(.bss.P4.*)

    /* Stack of process P4. */
    . = ALIGN(8); /* Stacks need to be 8 Byte aligned.  */
    ld_stackStartP4 = ABSOLUTE(.);
    . += ld_stackSizeP4;
    . = ALIGN(32);
    ld_stackEndP4 = ABSOLUTE(.);


    . = ALIGN(32);
    ld_dataP4End = ABSOLUTE(.);

} >memData

.data.P3 : ALIGN(32)
{
    /* Data sections for process P3. */
    . = ALIGN(32);
    ld_dataP3Start = ABSOLUTE(.);

    *_P3_*.o(.data)
    *_P3_*.o(.data.*)
    *(.data.P3)
    *(.data.P3.*)


    /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
       The end address of any area to copy should thus be a multiple of 4. */
    . = ALIGN(4);

} >memData AT>memFlash

.bss.P3 (NOLOAD) : ALIGN(32)
{
    *_P3_*.o(.bss)
    *_P3_*.o(.bss.*)
    *(.bss.P3)
    *(.bss.P3.*)

    /* Stack of process P3. */
    . = ALIGN(8); /* Stacks need to be 8 Byte aligned.  */
    ld_stackStartP3 = ABSOLUTE(.);
    . += ld_stackSizeP3;
    . = ALIGN(32);
    ld_stackEndP3 = ABSOLUTE(.);


    . = ALIGN(32);
    ld_dataP3End = ABSOLUTE(.);

} >memData

.data.P2 : ALIGN(32)
{
    /* Data sections for process P2. */
    . = ALIGN(32);
    ld_dataP2Start = ABSOLUTE(.);

    *_P2_*.o(.data)
    *_P2_*.o(.data.*)
    *(.data.P2)
    *(.data.P2.*)


    /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
       The end address of any area to copy should thus be a multiple of 4. */
    . = ALIGN(4);

} >memData AT>memFlash

.bss.P2 (NOLOAD) : ALIGN(32)
{
    *_P2_*.o(.bss)
    *_P2_*.o(.bss.*)
    *(.bss.P2)
    *(.bss.P2.*)

    /* Stack of process P2. */
    . = ALIGN(8); /* Stacks need to be 8 Byte aligned.  */
    ld_stackStartP2 = ABSOLUTE(.);
    . += ld_stackSizeP2;
    . = ALIGN(32);
    ld_stackEndP2 = ABSOLUTE(.);


    . = ALIGN(32);
    ld_dataP2End = ABSOLUTE(.);

} >memData

.data.P1 : ALIGN(32)
{
    /* Data sections for process P1. */
    . = ALIGN(32);
    ld_dataP1Start = ABSOLUTE(.);

    *_P1_*.o(.data)
    *_P1_*.o(.data.*)
    *(.data.P1)
    *(.data.P1.*)

    /* The majority of code will always be in process 1, which is intended for QM code.
       We add the standard sections to this process. This makes writing the QM code
       most easy and the C libraries become available to this code.
         Note, as general for the processes, the sbss sections are located in the same
       area as the small initialized data sections. This is required to reduce the
       number of consecutive memory chunks and to allow static MPU programming. */
    *(.data)
    *(.data.*)

    /* @todo Double-check and document. These sections come from the compiler or C lib */
    *(.got2)
    *(.got2.*)
    *(.tm_clone_table)
    *(.tm_clone_table.*)
    *(.jcr)
    *(.jcr.*)
    *(.rela.data)
    *(.rela.data.*)

    /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
       The end address of any area to copy should thus be a multiple of 4. */
    . = ALIGN(4);

} >memData AT>memFlash

.bss.P1 (NOLOAD) : ALIGN(32)
{
    *_P1_*.o(.bss)
    *_P1_*.o(.bss.*)
    *(.bss.P1)
    *(.bss.P1.*)

    /* Stack of process P1. */
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
    *(.bss)
    *(.bss.*)

    /* The heap area for the C library needs to be in the area of process 1, too, if we
       want it to use the C library. */
    . = ALIGN(8);
    ld_sbrkStart = ABSOLUTE(.);
    . += ld_heapSize;
    . = ALIGN(32);
    ld_sbrkEnd = ABSOLUTE(.);

    . = ALIGN(32);
    ld_dataP1End = ABSOLUTE(.);

} >memData


/* Order of small data sections: The two sX and the two sX2, X meaning bss or data,
   should be close together since they use the same _SDA_BASE_ pointer, or _SDA2_BASE_,
   respectively. */
.sbss (NOLOAD) : ALIGN(32)
{
    /* We should explicitly set the beginning of the SDA area. If we don't do then the
       default setting is the address of the first section, which matches *(.sdata.*).
       If we want to apply terms that filter for specific file names then these have to
       precede *(.sdata.*) and the SDA section of the file would actually be outside
       the SDA area. */
    _SDA_BASE_ = . + 0x8000;
    __SDATA_START__ = . ;

    /* Small, uninitialized data sections for kernel and trusted I/O driver code. */
    ld_sbssOSStart = ABSOLUTE(.);
    *_OS_*.o(.sbss)
    *_OS_*.o(.sbss.*)
    *(.sbss.OS)
    *(.sbss.OS.*)
    ld_sbssOSEnd = ABSOLUTE(.);

} >memData

.sdata : ALIGN(32)
{
    /* Small data sections for kernel and trusted I/O driver code. */
    . = ALIGN(32);
    ld_sdataOSStart = ABSOLUTE(.);
    *_OS_*.o(.sdata)
    *_OS_*.o(.sdata.*)
    *(.sdata.OS)
    *(.sdata.OS.*)
    . = ALIGN(32);
    ld_sdataOSEnd = ABSOLUTE(.);

    /* Small data sections for process P4. */
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

    /* Small data sections for process P3. */
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

    /* Small data sections for process P2. */
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

    /* Small data sections for process P1. */
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


    /* No further input section filters must be placed down here: The filters for process
       P1 are very open and, effectively, they will place all the unparticular rest into
       process P1. Additional filters will not likely catch anything. */

    /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
       The end address of any area to copy should thus be a multiple of 4. */
    . = ALIGN(4);

} >memData AT>memFlash


/* Order of small data sections: The two sX and the two sX2, X meaning bss or data,
   should be close together since they use the same _SDA_BASE_ pointer, or _SDA2_BASE_,
   respectively. */
.sbss2 (NOLOAD) : ALIGN(32)
{
    /* We should explicitly set the beginning of the SDA2 area. If we don't do then the
       default setting is the address of the first section, which matches *(.sdata2.*).
       If we want to apply terms that filter for specific file names then these have to
       precede *(.sdata2.*) and the SDA section of the file would actually be outside
       the SDA area. */
    _SDA2_BASE_ = . + 0x8000;
    __SDATA2_START__ = . ;

    /* Small, uninitialized data sections for kernel and trusted I/O driver code. */
    ld_sbss2OSStart = ABSOLUTE(.);
    *_OS_*.o(.sbss2)
    *_OS_*.o(.sbss2.*)
    *(.sbss2.OS)
    *(.sbss2.OS.*)
    ld_sbss2OSEnd = ABSOLUTE(.);

} >memData

.sdata2 : ALIGN(32)
{
    /* Small data sections for kernel and trusted I/O driver code. */
    . = ALIGN(32);
    ld_sdata2OSStart = ABSOLUTE(.);
    *_OS_*.o(.sdata2)
    *_OS_*.o(.sdata2.*)
    *(.sdata2.OS)
    *(.sdata2.OS.*)
    . = ALIGN(32);
    ld_sdata2OSEnd = ABSOLUTE(.);

    /* Small data sections for process P4. */
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

    /* Small data sections for process P3. */
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

    /* Small data sections for process P2. */
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

    /* Small data sections for process P1. */
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


    /* No further input section filters must be placed down here: The filters for process
       P1 are very open and, effectively, they will place all the unparticular rest into
       process P1. Additional filters will not likely catch anything. */

    /* Our ROM to RAM copy routine in the startup code copies always 4 Byte at once.
       The end address of any area to copy should thus be a multiple of 4. */
    . = ALIGN(4);

} >memData AT>memFlash

