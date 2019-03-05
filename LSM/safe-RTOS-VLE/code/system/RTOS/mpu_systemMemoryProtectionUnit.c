/**
 * @file mpu_systemMemoryProtectionUnit.c
 * Configuration and initialization of the Memory Protection Unit (MPU). The
 * configuration is chosen static in this sample. The initially chosen configuration is
 * never changed during run-time. Furthermore, the configuration is most simple; the only
 * requirement to fulfill is to make initial SW development as easy as possible and to
 * support inter-core communication.\n
 *   To simplify development, all cores get unrestricted access to all address space,
 * memory and I/O.\n
 *   To support inter-core communication there is an uncached memory area. Many inter-core
 * communication patterns are based on shared memory and while all memory id basically
 * shared in this sample is only the uncached memory easily and safely usable. (Otherwise
 * special techniques need to be applied to let a core look behind its cache into the main
 * memory.) The only - minor- speciality of the configuration is the use of a memory
 * section for the uncached area. There's no need to consider particular address spaces or
 * obey size boundaries, just declare your data as uncached where appropriate and the
 * linker will put it together.\n
 *   In particular and despite of the module name, there is no memory protection implemented
 * here. If an application needs to have protected memory areas then it will need to change
 * the chosen configuration.
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
 *   mpu_initMPU
 * Local functions
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"

#include "mpu_systemMemoryProtectionUnit.h"


/*
 * Defines
 */
 
/** Development support: If this define is set to one than the entire RAM is writable for
    all processes. */
#define MPU_DISARM_MPU  0


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
 * dependent configuration) is defined and then the unit is enabled.
 */
void mpu_initMPU(void)
{
    /* See RM, 31, p. 1039ff. */

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
    
    /* We consider the entired flash ROM, as far as used in this sample, as one memory
       region. We use linker defined symbols to find the boundaries. They need to be
       aligned compatible with the constraints of the MPU. This is checked by assertion. */
    extern uint8_t ld_romStart[0], ld_romEnd[0];
    assert(((uintptr_t)ld_romStart & 0x1f) == 0  &&  ((uintptr_t)ld_romEnd & 0x1f) == 0);
    
    unsigned int r = 0;
    
    /* All used flash ROM.
         All masters and processes (i.e. user mode code) get full read and execute rights.
       Write access is forbidden in order to detect programming errors. */
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

    /* RAM access for process 1. */
#if MPU_DISARM_MPU == 1
    extern uint8_t ld_ramStart[0], ld_ramEnd[0];
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_ramStart; /* Start address of region, 31.6.4.1 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_ramEnd-1; /* End address of region, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 1);
    ++ r;
#else
    extern uint8_t ld_sdaP1Start[0], ld_sdaP1End[0];
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_sdaP1Start; /* Start address sdata + sbss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_sdaP1End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 1);
    ++ r;
    extern uint8_t ld_sda2P1Start[0], ld_sda2P1End[0];
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_sda2P1Start; /* Start address sdata2 + sbss2 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_sda2P1End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 1);
    ++ r;
    extern uint8_t ld_dataP1Start[0], ld_dataP1End[0];
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_dataP1Start; /* Start address data + bss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_dataP1End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 1);
    ++ r;
#endif
    
    /* RAM access for process 2. */
#if MPU_DISARM_MPU == 1
    extern uint8_t ld_ramStart[0], ld_ramEnd[0];
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_ramStart; /* Start address of region, 31.6.4.1 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_ramEnd-1; /* End address of region, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 2);
    ++ r;
#else
    extern uint8_t ld_sdaP2Start[0], ld_sdaP2End[0];
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_sdaP2Start; /* Start address sdata + sbss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_sdaP2End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 2);
    ++ r;
    extern uint8_t ld_sda2P2Start[0], ld_sda2P2End[0];
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_sda2P2Start; /* Start address sdata2 + sbss2 */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_sda2P2End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 2);
    ++ r;
    extern uint8_t ld_dataP2Start[0], ld_dataP2End[0];
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_dataP2Start; /* Start address data + bss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_dataP2End-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b111111); /* S: d.c., U: RXW, PID: yes */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 2);
    ++ r;
#endif
    
    /* A shared memory area. ALl processes can write. */
    extern uint8_t ld_dataSharedEnd[0], ld_dataSharedStart[0];
    MPU.REGION[r].RGD_WORD0.R = (uintptr_t)ld_dataSharedStart; /* Start address data + bss */
    MPU.REGION[r].RGD_WORD1.R = (uintptr_t)ld_dataSharedEnd-1; /* End address, including. */
    MPU.REGION[r].RGD_WORD2.R = WORD2(0b011111); /* S: d.c., U: RXW, PID: no */
    MPU.REGION[r].RGD_WORD3.R = WORD3(/* Pid */ 0);
    ++ r;
    
    assert(r <= 16);

    /* RM 31.6.1, p. 1044: After configuring all region descriptors, we can globally enable
       the MPU. */
    MPU.CESR.R = 0xe0000000u    /* SPERR, w2c: Reset all possibly pending errors. */
                 |      0x1u;   /* VLD: Globally enable the MPU. */

} /* End of mpu_initMPU */
