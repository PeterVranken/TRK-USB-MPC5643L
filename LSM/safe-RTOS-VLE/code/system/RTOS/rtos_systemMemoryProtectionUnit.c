/**
 * @file rtos_systemMemoryProtectionUnit.c
 * Configuration and initialization of the Memory Protection Unit (MPU). The
 * configuration is chosen static in this sample. The initially chosen configuration is
 * never changed during run-time.\n
 *   The configuration uses a descriptor for all ROM, which is occupied by the compiled
 * code: All processes have read and execution access. Write access, although it can't do
 * any harm is forbidden: According run-time failure reporting will point to define
 * implementation errors.\n
 *   The configuration has one descriptor for all RAM, which is occupied by the compiled
 * code. All processes have read access and only the OS process has write and execute
 * access, too.\n
 *   The configuration has one descriptor for the entire peripheral address space. The OS
 * process has read and write access.\n
 *   Each of the four user processes has three descriptors, all of them for RAM write and
 * execute access. There's a descriptor for the initialized data and bss, one for the small
 * data and bss and one for the small data 2 and bss 2.\n
 *   The configuration has one descriptor for a chunk of memory, which all user processes
 * have read and write access to. This memory is meant for inter-process communication and
 * must evidently never be used to hold data, which is essential for the health of a
 * process and in particular for the process, that implements supervisory safety tasks.\n
 *   The static configuration of the MPU is the explanation for the fixed number of four
 * supported processes (#RTOS_NO_PROCESSES). We need three descriptors per user process at
 * least three for the OS (including user ROM) and have 16 descriptors available.
 *
 * CAUTION: The mentioned memory chunks or areas are puzzled by the linker. The MPU
 * configuration makes no hard coded assumptions about memory arrangement and distribution
 * or addresses and sizes of the chunks. It reads address and length of the memory chunks
 * from linker provided symbols. Insofar do we have a very tight relationship between the
 * implementation of this module and the linker script, which must be obeyed when doing
 * maintenance work at either side.
 *
 * Alternative configurations, which may find their use case and do not break the safety
 * concept:
 *   - The OS process doesn't necessarily need execution access for RAM. There
 * are typically driver implementation patterns, which require RAM execution rights, but it
 * is mostly about initialization and could be done prior to calling the MPU configuration.
 *   - The user processes won't normally need execution rights for their RAM
 *   - It is possible to let a process with higher privileges have access to the memories
 * of all processes with lower privileges. Below, it is explained how to do this
 *   - If the number of processes is reduced (a safety concept requires two processes at
 * minimum) then each process could have up to six descriptors. This would enable a
 * configuration with disjunct data and bss chunks so that the waste of data image ROM
 * disappears, which the current configuration has (see linker script for details)
 *   - The shared memory section can be disabled if not required for the inter-process
 * communication
 *   - A difficult topic is the placing of the small data 2 and bss 2 sections. It is
 * possible to locate them into the ROM. This is likely not fully compliant with the EABI
 * but for all normal embedded applications alright. In which case we had four free
 * descriptors and could implement two more processes. A similar concept for increasing the
 * number of processes would be entirely switching off the short addressing modes. This is a
 * matter of the compiler command line
 *
 * Copyright (C) 2018-2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */
/* Module interface
 *   rtos_initMPU
 *   rtos_checkUserCodeWritePtr
 * Local functions
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"

#include "rtos.h"
#include "rtos_systemMemoryProtectionUnit.h"


/*
 * Defines
 */

/** Development support: If this define is set to one than the entire RAM is writable for
    all processes. */
#define RTOS_DISARM_MPU  0


/*
 * Local type definitions
 */


/*
 * Local prototypes
 */


/*
 * Data definitions
 */


/*
 * Function implementation
 */

/**
 * Configure and initialize the memory protection unit MPU.\n
 *   The configuration is static, a set of regions suitable for the RTOS (and its project
 * dependent configuration) is defined and then the unit is enabled.\n
 *   See module description for more details.
 */
void rtos_initMPU(void)
{
    /* See RM, 31, p. 1039ff. */

    /* This function must not be used repeatedly during system initialization. */
    assert((MPU.CESR.R & 0x1) == 0x0u);  /* VLD should be in reset state, MPU disabled. */

    /* RM, 14.1.4, table 14-1, p. 285: The MPU routes and protects accesses to FLASH,
       PBRIDGE and SRAM (the three slave ports). In lockstep mode, the connected masters
       are:
         - Core Z4, instruction and data bus:   M0
         - eDMA:                                M2
         - NEXUS debug port:                    M0 (Listed as M8 but only the lower
       significant 3 Bit are taken)
         - FlexRay:                             M3 */

    /* RM, 31.6.4.3: All regions grant the same, unrestricted access to all bus masters.
       The access word has two bits for each master in supervisor mode and three bits in
       unser mode. A sixth bit enables taking the process ID PID into account. Master 0 ..
       3 are ordered from right to left, i.e. from least to most significant bits. The
       remaing 8 Bit of the word stay unused. */
    #define WORD2(access) (((((((access) << 6) | (access)) << 6) | (access)) << 6) | (access))

    /* Macro for readability of code below: Construct region descriptor word 3 from the
       field values, we are interested in. (The PID mask doesn't care in our code and is
       generally set to "all PID bits matter".) */
    #define WORD3(pid)  (((pid)<<24) | 0x00000001u)

    /* We consider the entire flash ROM, as far as used in this sample, as one memory
       region. We use linker defined symbols to find the boundaries. They need to be
       aligned compatible with the constraints of the MPU. This is checked by assertion. */
    extern uint8_t ld_romStart[0], ld_romEnd[0];
    assert(((uintptr_t)ld_romStart & 0x1f) == 0  &&  ((uintptr_t)ld_romEnd & 0x1f) == 0);

    unsigned int r = 0;

    /* All used flash ROM.
         All masters and processes (i.e. user mode code) get full read and execute rights.
       Write access is forbidden in order to detect programming errors.
         Note, all start and end addresses have a granularity of 32 Byte. By hardware, the
       least significant five bits of a start address are set to zero and to all ones for
       an end address. This requires according alignment operations in the linker script. */
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_romStart; /* Start address of region, 31.6.4.1 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_romEnd-1; /* End address of region, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b001101); /* S: RX, U: RX, PID: - */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 0);
    ++ r;

    /* All used SRAM for operating system kernel and processes.
         All masters and the kernel have full access (RWX) for all used RAM. The processes
       have general read access. (They get write and execute rights only to their own
       portion of RAM, which is specified in different region descriptors.)
         We use linker defined symbols to find the boundaries of the region. In the linker
       script, they need to be aligned compatible with the constraints of the MPU. This is
       checked by assertion. */
    extern uint8_t ld_memRamStart[0], ld_ramEnd[0];
    assert(((uintptr_t)ld_memRamStart & 0x1f) == 0  &&  ((uintptr_t)ld_ramEnd & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_memRamStart; /* Start address of region, 31.6.4.1 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_ramEnd-1; /* End address of region, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b000100); /* S: RWX, U: R, PID: d.c. */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 0);
    ++ r;

    /* The peripheral address space.
         All masters get read and write access. The processes(i.e. user mode) don't get
       access. */
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)0x8FF00000; /* Start address of region, 31.6.4.1 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)0xffffffff; /* End address of region, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b010000); /* S: RW, U: n.a., PID: d.c. */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 0);
    ++ r;

    /* It would be very easy to offer a compile time switch to select a hirarchical memory
       access model, where process with ID i has write access to the memory of process with
       ID j if i>=j. This will require just a few alternative lines of code here. All
       processes would use ld_sdaP1Start, ld_sda2P1Start and ld_dataP1Start instaed of
       ld_sdaPiStart, ld_sda2PiStart and ld_dataPiStart, respectively, in their
       descriptors.
         The reason not to do so is the huge amount of additional testing, which would be
       required. */

    /* RAM access for process 1. */
#if RTOS_DISARM_MPU == 1
    extern uint8_t ld_ramStart[0], ld_ramEnd[0];
    assert(((uintptr_t)ld_ramStart & 0x1f) == 0  &&  ((uintptr_t)ld_ramEnd & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_ramStart; /* Start address of region, 31.6.4.1 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_ramEnd-1; /* End address of region, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 1);
    ++ r;
#else
    extern uint8_t ld_sdaP1Start[0], ld_sdaP1End[0];
    assert(((uintptr_t)ld_sdaP1Start & 0x1f) == 0  &&  ((uintptr_t)ld_sdaP1End & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_sdaP1Start; /* Start address sdata + sbss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_sdaP1End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 1);
    ++ r;
    extern uint8_t ld_sda2P1Start[0], ld_sda2P1End[0];
    assert(((uintptr_t)ld_sda2P1Start & 0x1f) == 0  &&  ((uintptr_t)ld_sda2P1End & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_sda2P1Start; /* Start address sdata2 + sbss2 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_sda2P1End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 1);
    ++ r;
    extern uint8_t ld_dataP1Start[0], ld_dataP1End[0];
    assert(((uintptr_t)ld_dataP1Start & 0x1f) == 0  &&  ((uintptr_t)ld_dataP1End & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_dataP1Start; /* Start address data + bss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_dataP1End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 1);
    ++ r;
#endif

    /* RAM access for process 2. */
#if RTOS_DISARM_MPU == 1
    extern uint8_t ld_ramStart[0], ld_ramEnd[0];
    assert(((uintptr_t)ld_ramStart & 0x1f) == 0  &&  ((uintptr_t)ld_ramEnd & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_ramStart; /* Start address of region, 31.6.4.1 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_ramEnd-1; /* End address of region, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 2);
    ++ r;
#else
    extern uint8_t ld_sdaP2Start[0], ld_sdaP2End[0];
    assert(((uintptr_t)ld_sdaP2Start & 0x1f) == 0  &&  ((uintptr_t)ld_sdaP2End & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_sdaP2Start; /* Start address sdata + sbss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_sdaP2End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 2);
    ++ r;
    extern uint8_t ld_sda2P2Start[0], ld_sda2P2End[0];
    assert(((uintptr_t)ld_sda2P2Start & 0x1f) == 0  &&  ((uintptr_t)ld_sda2P2End & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_sda2P2Start; /* Start address sdata2 + sbss2 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_sda2P2End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 2);
    ++ r;
    extern uint8_t ld_dataP2Start[0], ld_dataP2End[0];
    assert(((uintptr_t)ld_dataP2Start & 0x1f) == 0  &&  ((uintptr_t)ld_dataP2End & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_dataP2Start; /* Start address data + bss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_dataP2End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 2);
    ++ r;
#endif

    /* RAM access for process 3. */
#if RTOS_DISARM_MPU == 1
    extern uint8_t ld_ramStart[0], ld_ramEnd[0];
    assert(((uintptr_t)ld_ramStart & 0x1f) == 0  &&  ((uintptr_t)ld_ramEnd & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_ramStart; /* Start address of region, 31.6.4.1 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_ramEnd-1; /* End address of region, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 3);
    ++ r;
#else
    extern uint8_t ld_sdaP3Start[0], ld_sdaP3End[0];
    assert(((uintptr_t)ld_sdaP3Start & 0x1f) == 0  &&  ((uintptr_t)ld_sdaP3End & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_sdaP3Start; /* Start address sdata + sbss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_sdaP3End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 3);
    ++ r;
    extern uint8_t ld_sda2P3Start[0], ld_sda2P3End[0];
    assert(((uintptr_t)ld_sda2P3Start & 0x1f) == 0  &&  ((uintptr_t)ld_sda2P3End & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_sda2P3Start; /* Start address sdata2 + sbss2 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_sda2P3End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 3);
    ++ r;
    extern uint8_t ld_dataP3Start[0], ld_dataP3End[0];
    assert(((uintptr_t)ld_dataP3Start & 0x1f) == 0  &&  ((uintptr_t)ld_dataP3End & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_dataP3Start; /* Start address data + bss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_dataP3End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 3);
    ++ r;
#endif

    /* RAM access for process 4. */
#if RTOS_DISARM_MPU == 1
    extern uint8_t ld_ramStart[0], ld_ramEnd[0];
    assert(((uintptr_t)ld_ramStart & 0x1f) == 0  &&  ((uintptr_t)ld_ramEnd & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_ramStart; /* Start address of region, 31.6.4.1 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_ramEnd-1; /* End address of region, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 4);
    ++ r;
#else
    extern uint8_t ld_sdaP4Start[0], ld_sdaP4End[0];
    assert(((uintptr_t)ld_sdaP4Start & 0x1f) == 0  &&  ((uintptr_t)ld_sdaP4End & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_sdaP4Start; /* Start address sdata + sbss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_sdaP4End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 4);
    ++ r;
    extern uint8_t ld_sda2P4Start[0], ld_sda2P4End[0];
    assert(((uintptr_t)ld_sda2P4Start & 0x1f) == 0  &&  ((uintptr_t)ld_sda2P4End & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_sda2P4Start; /* Start address sdata2 + sbss2 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_sda2P4End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 4);
    ++ r;
    extern uint8_t ld_dataP4Start[0], ld_dataP4End[0];
    assert(((uintptr_t)ld_dataP4Start & 0x1f) == 0  &&  ((uintptr_t)ld_dataP4End & 0x1f) == 0);
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_dataP4Start; /* Start address data + bss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_dataP4End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 4);
    ++ r;
#endif

    /* A shared memory area. ALl processes can write. */
    extern uint8_t ld_dataSharedStart[0], ld_dataSharedEnd[0];
    assert(((uintptr_t)ld_dataSharedStart & 0x1f) == 0
           &&  ((uintptr_t)ld_dataSharedEnd & 0x1f) == 0
          );
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_dataSharedStart; /* Start address data + bss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_dataSharedEnd-1; /* End address, including. */
/// @todo User code doesn't require instruction read access
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b011111); /* S: d.c., U: RXW, PID: no */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 0);
    ++ r;

    assert(r <= 16);

    /* RM 31.6.1, p. 1044: After configuring all region descriptors, we can globally enable
       the MPU. */
    MPU.CESR.R = 0xe0000000u    /* SPERR, w2c: Reset all possibly pending errors. */
                 |      0x1u;   /* VLD: Globally enable the MPU. */

} /* End of rtos_initMPU */




/**
 * Helper function, mainly intended to support safe system call handler implementation:
 * Check if a pointer value is valid for writing in the context of a given process.\n
 *   A system call handler must never trust a user code provided pointer; evidently not for
 * write access but not even for read operation (a read into the address space of
 * peripherals can have a side effect). The user code could make the system call handler
 * overwrite some non-process owned data objects, cause an access violation in the
 * supervisor code or manipulate some peripherals.\n
 *   Normally, it's strongly disencouraged having pointers as arguments of system calls at
 * all. If not avoidable, one can use this helper function to check that a pointer points
 * into permitted address space and that all bytes of a data object pointed at are still in
 * that address space. Here for write access.\n
 *   Permitted address space is anywhere, where the process may write without causing an
 * exception or any kind of side effect. In particular, this means the process' own RAM and
 * the shared RAM.
 *   @return
 * Get \a true if the pointer may be used for write access and \a false otherwise.
 *   @param PID
 * The ID of the process the query relates to. Range is 1..4.
 *   @param address
 * The pointer value, or the beginning of the chunk of memory, which needs to be entirely
 * located in writable memory.
 *   @param noBytes
 * The size of the chunk of memory to be checked. Must not be less than one. (Checked by
 * assertion).
 *   @remark
 * The counterpart function rtos_checkUserCodeReadPtr() is implemented as inline function
 * in the RTOS API header file, rtos.h.
 *   @remark
 * Although this function is intended for use inside a system call handler it can be safely
 * used from user code, too.
 */
bool rtos_checkUserCodeWritePtr(unsigned int PID, const void *address, size_t noBytes)
{
    const uint8_t * const p = (uint8_t*)address;

    /* The function doesn't support the kernel process with ID zero. We consider the index
       offset by one. */
    const unsigned int idxP = PID-1;
    if(idxP >= RTOS_NO_PROCESSES)
        return false;

    /* All relevant RAM areas are defined in the linker script. We can access the
       information by declaring the linker defined symbols. */

#if RTOS_DISARM_MPU == 1
    extern uint8_t ld_ramStart[0], ld_ramEnd[0];
    return p >= ld_ramStart  &&  p+noBytes <= ld_ramEnd;
#else
    extern uint8_t ld_dataSharedStart[0], ld_dataSharedEnd[0];
    if(p >= ld_dataSharedStart  &&  p+noBytes <= ld_dataSharedEnd)
        return true;

    extern uint8_t ld_sdaP1Start[0], ld_sdaP1End[0]
                 , ld_sda2P1Start[0], ld_sda2P1End[0]
                 , ld_dataP1Start[0], ld_dataP1End[0];

    extern uint8_t ld_sdaP2Start[0], ld_sdaP2End[0]
                 , ld_sda2P2Start[0], ld_sda2P2End[0]
                 , ld_dataP2Start[0], ld_dataP2End[0];

    extern uint8_t ld_sdaP3Start[0], ld_sdaP3End[0]
                 , ld_sda2P3Start[0], ld_sda2P3End[0]
                 , ld_dataP3Start[0], ld_dataP3End[0];

    extern uint8_t ld_sdaP4Start[0], ld_sdaP4End[0]
                 , ld_sda2P4Start[0], ld_sda2P4End[0]
                 , ld_dataP4Start[0], ld_dataP4End[0];

    #define from    0
    #define to      1

    #define SDA     0
    #define SDA2    1
    #define DATA    2

    #define addrAry(prc, boundary)                  \
                { [SDA]  = ld_sda##prc##boundary    \
                , [SDA2] = ld_sda2##prc##boundary   \
                , [DATA] = ld_data##prc##boundary   \
                }
    #define addrPrcAryAry(prc)   {[from] = addrAry(prc, Start), [to] = addrAry(prc, End)}

    static const uint8_t const *ramAreaAryAryAry_[/* idxP */ RTOS_NO_PROCESSES]
                                                 [/* from/to */ 2]
                                                 [/* Area */ 3] =
                                                            { [/*idxP*/ 0] = addrPrcAryAry(P1)
                                                            , [/*idxP*/ 1] = addrPrcAryAry(P2)
                                                            , [/*idxP*/ 2] = addrPrcAryAry(P3)
                                                            , [/*idxP*/ 3] = addrPrcAryAry(P4)
                                                            };
    return p >= ramAreaAryAryAry_[idxP][from][SDA]
           &&  p+noBytes <= ramAreaAryAryAry_[idxP][to][SDA]
           ||  p >= ramAreaAryAryAry_[idxP][from][SDA2]
               &&  p+noBytes <= ramAreaAryAryAry_[idxP][to][SDA2]
           ||  p >= ramAreaAryAryAry_[idxP][from][DATA]
               &&  p+noBytes <= ramAreaAryAryAry_[idxP][to][DATA];

    #undef from
    #undef to
    #undef SDA
    #undef SDA2
    #undef DATA
    #undef addrAry
    #undef addrPrcAryAry
#endif
} /* End of rtos_checkUserCodeWritePtr */



