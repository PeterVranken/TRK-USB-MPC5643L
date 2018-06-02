/**
 * @file lbd_ledAndButtonDriver.c
 * A simple I/O driver to give access to the LEDs and buttons on the evaluation board.\n
 *   This file is a modification of the similar file lbd_ledAndButtonDriver.h used in the
 * other TRK-USB-MPC5643L samples: It is a kernelBuilder I/O driver, which implements the
 * same functionality as system calls so that it becomes available to user mode tasks.
 * 
 * Copyright (C) 2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   lbd_sc_setLED
 *   lbd_sc_getButtonSw2
 *   lbd_sc_getButtonSw3
 *   lbd_sc_getButton   
 * Local functions
 *   setLED      
 *   getButtonSw2
 *   getButtonSw3
 *   getButton
 */
/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "typ_types.h"
#include "lbd_ledAndButtonDriver.h"


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
static inline void setLED(lbd_led_t led, bool isOn)
{
    /* Using .B.PDO implements a byte access to one of the single pad registers. This means
       that we don't have race conditions with other pads (maybe concurrently controlled
       from other contexts). */ 
    SIU.GPDO[led].B.PDO = isOn? 0: 1;
    
} /* End of setLED */


/**
 * Get the current status of button SW2.
 *   @return
 * \a true if button SW2 is currently pressed, \a false otherwise. This is the debounced
 * read value from the GPIO.
 *   @param button
 * The enumeration value to identify a button.
 *   @remark
 * The function is implemented as static inline. This implies that any using code location
 * will get its own, irrelated debounce counter. One logical client of a button should not
 * have more than one code location to read its current value, otherwise its debouncing
 * won't function as intended.
 */
static inline bool getButtonSw2(void)
{
    _Static_assert( LBD_MAX_CNT_BTN_DEBOUNCE >= 1  && LBD_MAX_CNT_BTN_DEBOUNCE <= 50
                  , "Debounce time configuration out of range"
                  );

    static int cntDebounce_ = 0;
    static bool buttonState_ = false;
    cntDebounce_ += SIU.GPDI[lbd_bt_button_SW2].B.PDI? -1: 1;
    if(cntDebounce_ >= LBD_MAX_CNT_BTN_DEBOUNCE)
    {
        cntDebounce_ = LBD_MAX_CNT_BTN_DEBOUNCE;
        buttonState_ = true;
    }
    else if(cntDebounce_ <= -LBD_MAX_CNT_BTN_DEBOUNCE)
    {
        cntDebounce_ = -LBD_MAX_CNT_BTN_DEBOUNCE;
        buttonState_ = false;
    }
    return buttonState_;
    
} /* End of getButtonSw2 */




/**
 * Get the current status of button SW3.
 *   @return
 * \a true if button SW3 is currently pressed, \a false otherwise. This is the debounced
 * read value from the GPIO.
 *   @param button
 * The enumeration value to identify a button.
 *   @remark
 * The function is implemented as static inline. This implies that any using code location
 * will get its own, irrelated debounce counter. One logical client of a button should not
 * have more than one code location to read its current value, otherwise its debouncing
 * won't function as intended.
 */
static inline bool getButtonSw3(void)
{
    static int cntDebounce_ = 0;
    static bool buttonState_ = false;
    cntDebounce_ += SIU.GPDI[lbd_bt_button_SW3].B.PDI? -1: 1;
    if(cntDebounce_ >= LBD_MAX_CNT_BTN_DEBOUNCE)
    {
        cntDebounce_ = LBD_MAX_CNT_BTN_DEBOUNCE;
        buttonState_ = true;
    }
    else if(cntDebounce_ <= -LBD_MAX_CNT_BTN_DEBOUNCE)
    {
        cntDebounce_ = -LBD_MAX_CNT_BTN_DEBOUNCE;
        buttonState_ = false;
    }
    return buttonState_;
    
} /* End of getButtonSw3 */




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
static inline bool getButton(lbd_button_t button)
{
    if(button == lbd_bt_button_SW2)
        return getButtonSw2();
    else
    {
        assert(button == lbd_bt_button_SW3);
        return getButtonSw3();
    }
} /* End of getButton */


/**
 * Initialization of LED driver. The GPIO ports are defined to become outputs and the
 * output values are set such that the LEDs are shut off.
 *   @remark
 * This is an ordinary function, which can be called directly (not as a system call) and
 * which requires supervisor mode for execution.
 */
void lbd_initLEDAndButtonDriver(void)
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
 * A wrapper around the LED I/O driver function setLED, which offers the access to the LEDs
 * as (simple) system call.
 *   @param pMSR
 * The kernelBuilder API offers to continue the calling context with changed machine
 * status. The CPU register MSR value can be accessed by reference. However, this system
 * call doesn't change the machine status.
 *   @param led
 * The ID of the LED to access. See setLED() for more.
 *   @param isOn
 * The new state of the LED. See setLED() for more.
 *   @remark
 * This is the implementation of a system call. Never call this function directly. Only use
 * macro #lbd_setLed to invoke it, which is a wrapper around int_systemCall() from the
 * kernelBuilder API. Any attempt to call this function in user mode will cause an
 * exception.
 */
uint32_t lbd_sc_setLED(uint32_t * const pMSR ATTRIB_UNUSED, lbd_led_t led, bool isOn)
{
    setLED(led, isOn);
    return 0;

} /* End of lbd_sc_setLED */



/**
 * A wrapper around the button I/O driver function getButton, which offers the access to
 * the buttons as (simple) system call.
 *   @return
 * Get the button state as a Boolean, \a true if button is currently pressed. See
 * getButton() for more.
 *   @param pMSR
 * The kernelBuilder API offers to continue the calling context with changed machine
 * status. The CPU register MSR value can be accessed by reference. However, this system
 * call doesn't change the machine status.
 *   @param button
 * The ID of the button to access. See getButton() for more.
 *   @remark
 * This is the implementation of a system call. Never call this function directly. Only use
 * macro #lbd_setLed to invoke it, which is a wrapper around int_systemCall() from the
 * kernelBuilder API. Any attempt to call this function in user mode will cause an
 * exception.
 */
uint32_t lbd_sc_getButton(uint32_t * const pMSR ATTRIB_UNUSED, lbd_button_t button)
{
    return (uint32_t)getButton(button);

} /* End of lbd_sc_getButton */

