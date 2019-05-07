/**
 * @file mai_main.c
 *   The main entry point of the C code. The interrupt handlers from the standard startup
 * code of the MCU in sample "startup" has been exchanged with the IVOR #4 and #8 handlers
 * of kernelBuiler, which support system calls and context switches.\n
 *   This sample of kernelBuilder demonstrates how to make a simple RTOS.
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
#include "ihw_initMcuCoreHW.h"
#include "sio_serialIO.h"
#include "sch_scheduler.h"
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
    
    /* Initialize the button and LED driver for the eval board. */
    lbd_initLEDAndButtonDriver();

    /* Initialize the serial interface. */
    sio_initSerialInterface(/* baudRate */ 115200);

    /* The external interrupts are enabled after configuring I/O devices and registering
       the interrupt handlers. */
    ihw_resumeAllInterrupts();

    iprintf( "TRK-USB-MPC5643LAtGitHub - kernelBuilder (simpleRTOS)\r\n"
             "Copyright (C) 2017-2018 Peter Vranken\r\n"
             "This program comes with ABSOLUTELY NO WARRANTY.\r\n"
             "This is free software, and you are welcome to redistribute it\r\n"
             "under certain conditions; see LGPL.\r\n"
           );

    /* Branch into the never returning scheduler. */
    sch_initAndStartScheduler();
    assert(false);
   
} /* End of main */
