/**
 * @file mai_main.c
 *   The main entry point of the C code. The interrupt handlers from the standard startup
 * code of the MCU in sample "startup" has been exchanged with the IVOR #4 and #8 handlers
 * of kernelBuiler, which support system calls and context switches. This sample of
 * kernelBuilder demonstrates typical elements of a true OS kernel, like timer control of
 * context switches and service functions, which are implemented as system calls. See
 * xsw_contextSwitch.c in the same folder for details.
 *
 * Copyright (C) 2017-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   main
 * Local functions
 *   getTBL
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include "typ_types.h"
#include "lbd_ledAndButtonDriver.h"
#include "del_delay.h"
#include "ihw_initMcuCoreHW.h"
#include "sio_serialIO.h"
#include "xsw_contextSwitch.h"
#include "sc_systemCalls.h"
#include "mai_main.h"


/*
 * Defines
 */


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
 * Entry point into C code. The C main function is entered without arguments and despite of
 * its return code definition it must never be left. (Returning from main would enter an
 * infinite loop in the calling assembler startup code.)
 */
void main()
{
    /* Init core HW of MCU so that it can be safely operated. */
    ihw_initMcuCoreHW();

#ifdef DEBUG
    /* Check linker script. It's error prone with respect to keeping the initialized RAM
       sections and the according initial-data ROM sections strictly in sync. As long as
       this has not been sorted out by a redesign of linker script and startup code we put
       a minimal plausibility check here, which will likely detect typical errors.
         If this assertion fires your initial RAM contents will be corrupt. */
    extern const uint8_t ld_dataSize[0], ld_dataMirrorSize[0];
    assert(ld_dataSize == ld_dataMirrorSize);
#endif

    /* Initialize the button and LED driver for the eval board. */
    lbd_initLEDAndButtonDriver();

    /* Initialize the serial interface. */
    sio_initSerialInterface(/* baudRate */ 115200);

    /* The external interrupts are enabled after configuring I/O devices and registering
       the interrupt handlers. */
    ihw_resumeAllInterrupts();

    iprintf( "TRK-USB-MPC5643LAtGitHub - kernelBuilder (alternatingContexts)\r\n"
             "Copyright (C) 2017-2018 Peter Vranken\r\n"
             "This program comes with ABSOLUTELY NO WARRANTY.\r\n"
             "This is free software, and you are welcome to redistribute it\r\n"
             "under certain conditions; see LGPL.\r\n"
           );

    /* Try the system calls. */
    unsigned int idxLoop;
    for(idxLoop=0; idxLoop<15; ++idxLoop)
    {
        unsigned int idxSem;
        for(idxSem=0; idxSem<11; ++idxSem)
        {
            unsigned int retSC = int_systemCall
                                    (/* idxSysCall */ SC_IDX_SYS_CALL_TEST_AND_DECREMENT
                                    , idxSem
                                    );

            iprintf("Semaphore %u has value %u after system call\r\n", idxSem, retSC);
            
            /* A small (busy) wait to let the printf buffer be flushed. Otherwise it'll
               overrun and output would be fragmented. */
            del_delayMicroseconds(/* tiCpuInUs */ 20000);
        }
    }    

    /* All initial counts of the semaphores are now exhausted. We return a single semaphore
       count. This semaphore is used in module xsw to synchronize the access to the LED
       between different contexts. */
    unsigned int retSC ATTRIB_DBG_ONLY = sc_increment(/*idxSem*/ 0);
    assert(retSC == 1);

    /* Branch into endless looping context switch experiment. */
    xsw_loop();
    assert(false);
   
} /* End of main */
