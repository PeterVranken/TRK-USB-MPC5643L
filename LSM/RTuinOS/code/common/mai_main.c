/**
 * @file mai_main.c
 *   The main entry point of the C code. The startup code of the MCU is identical to sample
 * "startup"; refer to that TRK-USB-MPC5643L sample for details.\n
 *   The hardware initialization is completed similar as it is done in all other
 * TRK-USB-MPC5643L samples. Then the RTuinOS initialization is invoked from where code
 * execution never returns. The RTOS initialization uses some hooks into the application to
 * give it the opportunity to do its application dependent initialization - the task
 * configuration in the first place.\n
 *   Because of doing all application individual stuff in the hooks this module can be
 * re-used in all RTuinOS samples.
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
/* Module interface
 *   mai_getTBL
 *   delay
 *   millis
 *   mai_blink
 *   main
 * Local functions
 */

/*
 * Include files
 */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
#include "sio_serialIO.h"
#include "del_delay.h"
#include "gsl_systemLoad.h"
#include "rtos.h"
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

#if 0
/**
 * Helper function: Read high resolution timer register of CPU. The register wraps around
 * after about 35s. The return value can be used to measure time spans up to this length.
 *   @return
 * Get the current register value. The value is incremented every 1/120MHz = (8+1/3)ns
 * regardless of the CPU activity.
 */
static inline uint32_t getTBL()
{
    uint32_t TBL;
    asm volatile ( /* AssemblerTemplate */
                   "mfspr %0, 268\n\r" /* SPR 268 = TBL, 269 = TBU */
                 : /* OutputOperands */ "=r" (TBL)
                 : /* InputOperands */
                 : /* Clobbers */
                 );
    return TBL;

} /* End of getTBL */
#endif



/**
 * Delay code execution for a number of Milliseconds of world time. The delay is
 * independent of the system load.\n
 *   Note, this function emulates an Arduino standard function. This explains, why the name
 * doesn't comply with our normal naming pattern.
 *   @param tiInMs
 * The number of Milliseconds to stay in the function. The range is limited to 0..INT_MAX
 */
void delay(unsigned int tiInMs)
{
    assert(tiInMs <= INT_MAX);
    const uint64_t tiReturn = (uint64_t)((float)tiInMs / 8.33333333333e-6f /* ms/tick */)
                              + GSL_PPC_GET_TIMEBASE();
    while((int64_t)(tiReturn - GSL_PPC_GET_TIMEBASE()) > 0)
        ;
} /* End of delay */



/**
 * System time elapsed since startup in Milliseconds.\n
 *   Note, this function emulates an Arduino standard function. This explains, why the name
 * doesn't comply with our normal naming pattern.
 *   @return
 * Get the time elapsed since startup in Milliseconds.
 *   @remark
 * The 32 Bit result wraps around after 49d 17h 2min 47s.
 */
unsigned long millis(void)
{
    return (unsigned long)(GSL_PPC_GET_TIMEBASE() / 120000ull);

} /* End of millis */



/**
 * System time elapsed since startup in Microseconds.
 *   @return
 * Get the time elapsed since startup in Microseconds.
 *   @remark
 * The 32 Bit result wraps around after 1h 11min 35s.
 */
unsigned long micros(void)
{
    return (unsigned long)(GSL_PPC_GET_TIMEBASE() / 120ull);

} /* End of micros */



/**
 * Trivial routine that flashes the LED a number of times to give simple feedback. The
 * routine is blocking. The timing doesn't depend on the system load, it is coupled to a real
 * time clock.
 *   @param noFlashes
 * The number of times the LED is lit.
 */
void mai_blink(unsigned int noFlashes)
{
#define TI_FLASH_MS 200

    while(noFlashes-- > 0)
    {
        lbd_setLED(lbd_led_D4_red, /* isOn */ true); /* Turn the LED on. */
        delay(/* tiInMs */ TI_FLASH_MS);
        lbd_setLED(lbd_led_D4_red, /* isOn */ false); /* Turn the LED off. */
        delay(/* tiInMs */ TI_FLASH_MS);
    }                              
    
    /* Wait for a second after the last flash - this command could easily be invoked
       immediately again and the bursts need to be separated. */
    delay(/* tiInMs */ (1000-TI_FLASH_MS));

#undef TI_FLASH_MS
} /* End mai_blink */



/**
 * Entry point into C code. The C main function is entered without arguments and despite of
 * its return code definition it must never be left. (Returning from main would enter an
 * infinite loop in the calling assembler startup code.)
 */
void main(void)
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

    /* The next function will never return. */
    rtos_initRTOS();
    
    /* This code is never reached. */
    assert(false);
   
} /* End of main */
