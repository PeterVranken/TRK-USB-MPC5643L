/**
 * @file tc01_idleTaskOnly.c
 *
 * Test case 01 of RTuinOS. No task is defined, only the idle task is running. System
 * behaves like an ordinary Arduino sketch.
 *
 * Copyright (C) 2012-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *
 * Module interface
 *   setup
 *   loop
 * Local functions
 */

/*
 * Include files
 */

#include <stdio.h>
#include <assert.h>

#include "sio_serialIO.h"
#include "lbd_ledAndButtonDriver.h"
#include "del_delay.h"
#include "mai_main.h"
#include "rtos.h"


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
 * The initalization of the RTOS tasks and general board initialization.
 */ 
void setup(void)
{
    /* Print standard greeting of RTuinOS applications. */
    iprintf(RTOS_EOL RTOS_RTUINOS_STARTUP_MSG RTOS_EOL);

} /* End of setup */




/**
 * The application owned part of the idle task. This routine is repeatedly called whenever
 * there's some execution time left. It's interrupted by any other task when it becomes
 * due.
 *   @remark
 * Different to all other tasks, the idle task routine may and should return. (The task as
 * such doesn't terminate). This has been designed in accordance with the meaning of the
 * original Arduino loop function.
 */ 
void loop(void)
{
    mai_blink(4);
    fputs("RTuinOS is idle" RTOS_EOL, stdout);
    
} /* End of loop */
