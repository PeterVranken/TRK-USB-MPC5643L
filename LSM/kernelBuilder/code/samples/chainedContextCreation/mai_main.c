/**
 * @file mai_main.c
 *   The main entry point of the C code. The startup code of the MCU is identical to sample
 * "startup"; refer to that sample for details. Then we initialize serial and LED output
 * and branch into the test routine in module
 * samples/chainedContextCreation/xsw_contextSwitch.c.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
#include "sio_serialIO.h"
#include "xsw_contextSwitch.h"
#include "int_interruptHandler.h"
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

    iprintf( "TRK-USB-MPC5643LAtGitHub - kernelBuilder (chainedContextCreation)\r\n"
             "Copyright (C) 2017-2018 Peter Vranken\r\n"
             "This program comes with ABSOLUTELY NO WARRANTY.\r\n"
             "This is free software, and you are welcome to redistribute it\r\n"
             "under certain conditions; see LGPL.\r\n"
           );

    /* Branch into endless looping context switch experiment. */
    xsw_startContextSwitching();
    assert(false);
   
} /* End of main */
