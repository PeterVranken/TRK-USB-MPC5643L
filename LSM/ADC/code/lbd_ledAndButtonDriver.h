#ifndef LBD_LEDANDBUTTONDRIVER_INCLUDED
#define LBD_LEDANDBUTTONDRIVER_INCLUDED
/**
 * @file lbd_ledAndButtonDriver.h
 * Definition of global interface of module lbd_ledAndButtonDriver.c
 * Simple hardware driver for the LEDs and buttons on the eval board TRK-USB-MPC5643L.
 *
 * Copyright (C) 2017 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   lbd_initLEDAndButtonDriver
 *   lbd_setLED
 *   lbd_getButton
 */
/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>

#include "MPC5643L.h"


/*
 * Defines
 */

/* The software is written as portable as reasonably possible. This requires the awareness
   of the C language standard it is compiled with. */
#if defined(__STDC_VERSION__)
# if (__STDC_VERSION__)/100 == 2011
#  define _STDC_VERSION_C11
# elif (__STDC_VERSION__)/100 == 1999
#  define _STDC_VERSION_C99
# endif
#endif


/*
 * Global type definitions
 */

/** The list of available LEDs. */
typedef enum lbd_led_t
    { lbd_led_D4_grn = 98   /// The value is the SIU index of green D4, port G2
    , lbd_led_D4_red = 99   /// The value is the SIU index of red D4, port G3
    , lbd_led_D5_grn = 106  /// The value is the SIU index of green D5, port G10
    , lbd_led_D5_red = 107  /// The value is the SIU index of red D5, port G11
    } lbd_led_t;

/** The list of available buttons. */
typedef enum lbd_button_t
    { lbd_bt_button_SW2 = 85 /// The value is the SIU index of button Switch 2, port F5
    , lbd_bt_button_SW3 = 86 /// The value is the SIU index of button Switch 3, port F6
    } lbd_button_t;
    

/*
 * Global data declarations
 */

/*
 * Global inline functions
 */

/**
 * Initialization of LED driver. The GPIO ports are defined to become outputs and the
 * output values are set such that the LEDs are shut off.
 */
static inline void lbd_initLEDAndButtonDriver()
{
    /* LEDs are initially off. */
    SIU.GPDO[lbd_led_D4_grn].B.PDO = 1;
    SIU.GPDO[lbd_led_D4_red].B.PDO = 1;
    SIU.GPDO[lbd_led_D5_grn].B.PDO = 1;
    SIU.GPDO[lbd_led_D5_red].B.PDO = 1;
    
    /* 0x200: Output buffer enable, 0x20: Open drain output, LED connected through resistor
       to +U */
    SIU.PCR[lbd_led_D4_grn].R = 0x0220;
    SIU.PCR[lbd_led_D4_red].R = 0x0220;
    SIU.PCR[lbd_led_D5_grn].R = 0x0220;
    SIU.PCR[lbd_led_D5_red].R = 0x0220;

    /* Unfortunately, the buttons are connected to inputs, that are not interrupt enabled.
       We will have to poll the current input values.
       0x100: Input buffer enable. */
    SIU.PCR[lbd_bt_button_SW2].R = 0x0100;
    SIU.PCR[lbd_bt_button_SW3].R = 0x0100;
    
} /* End of lbd_initLEDAndButtonDriver */


/**
 * Switch a single LED on or off.
 *   @param led
 * The enumeration value to identify an LED.
 *   @param isOn
 * \a true to switch it on \a false to switch it off.
 *   @remark
 * There are no race-conditions between different LEDs. You need to consider using a
 * critical section only, if one and the same LED is served from different interrupt
 * contexts. This is not handled by this driver.
 */
static inline void lbd_setLED(lbd_led_t led, bool isOn)
{
    /* Using .B.PDO implements a byte access to one of the single pad registers. This means
       that we don't have race conditions with other pads (maybe concurrently controlled
       from other contexts). */ 
    SIU.GPDO[led].B.PDO = isOn? 0: 1;
    
} /* End of lbd_setLED */


/**
 * Get the current status of a button.
 *   @return
 * \a true if button is currently pressed, \a false otherwise. This is the debounced read
 * value from the GPIO.
 *   @param button
 * The enumeration value to identify a button.
 *   @remark
 * The function is implemented as static inline. This implies that any using code location
 * will get its own, irrelated debounce counter. One logical client of a button should not
 * have more than one code location to read its current value, otherwise its debouncing
 * won't function as intended.
 */
static inline bool lbd_getButton(lbd_button_t button)
{
#define MAX_CNT 10

    static int cntDebounce = 0;
    static bool buttonState = false;
    cntDebounce += SIU.GPDI[button].B.PDI? -1: 1;
    if(cntDebounce > MAX_CNT)
    {
        cntDebounce = MAX_CNT;
        buttonState = true;
    }
    else if(cntDebounce < -MAX_CNT)
    {
        cntDebounce = -MAX_CNT;
        buttonState = false;
    }
    return buttonState;
    
#undef MAX_CNT
} /* End of lbd_getButton */


/*
 * Global prototypes
 */



#endif  /* LBD_LEDANDBUTTONDRIVER_INCLUDED */
