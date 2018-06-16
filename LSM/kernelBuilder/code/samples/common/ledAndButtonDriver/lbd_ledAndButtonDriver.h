#ifndef LBD_LEDANDBUTTONDRIVER_INCLUDED
#define LBD_LEDANDBUTTONDRIVER_INCLUDED
/**
 * @file lbd_ledAndButtonDriver.h
 * Definition of global interface of module lbd_ledAndButtonDriver.c
 * Simple hardware driver for the LEDs and buttons on the eval board TRK-USB-MPC5643L.
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
 *   lbd_initLEDAndButtonDriver
 *   lbd_setLED
 *   lbd_getButtonSw2
 *   lbd_getButtonSw3
 *   lbd_getButton
 */
/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>

#include "MPC5643L.h"
#include "sc_systemCalls.h"


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

/** The debounce time of the read process of the button states in ticks, where one tick is
    the time between two invokations of interface function lbd_getButton.
      The range is 2 .. 100. */
#define LBD_DEBOUNCE_TIME_BUTTONS   4

/* The debounce time of the read process of the button states is determined by this
   counter maximum. */
#define LBD_MAX_CNT_BTN_DEBOUNCE    ((LBD_DEBOUNCE_TIME_BUTTONS)/2)


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



/*
 * Global prototypes
 */

/** Initialization of LED driver. */
void lbd_initLEDAndButtonDriver(void);

/** System call implementation: Never call this function directly. */
uint32_t lbd_sc_setLED(uint32_t * const pMSR, lbd_led_t led, bool isOn);

/** System call implementation: Never call this function directly. */
uint32_t lbd_sc_getButton(uint32_t * const pMSR, lbd_button_t button);


/*
 * System calls
 */

/** Simple system call: Invoke the API setLED() from the LED and button I/O driver as a
    system call. For a detailed function description refer to the API function. */
#define /* void */ lbd_setLED(/* lbd_led_t */ led, /* bool */ isOn)  \
                        int_systemCall(LBD_IDX_SIMPLE_SYS_CALL_SET_LED, led, isOn)

/** Simple system call: Invoke the API getButton() from the LED and button I/O driver as a
    system call. For a detailed function description refer to the API function. */
#define /* bool */ lbd_getButton(/* lbd_button_t */ button)  \
                        int_systemCall(LBD_IDX_SIMPLE_SYS_CALL_GET_BUTTON, button)

#endif  /* LBD_LEDANDBUTTONDRIVER_INCLUDED */
