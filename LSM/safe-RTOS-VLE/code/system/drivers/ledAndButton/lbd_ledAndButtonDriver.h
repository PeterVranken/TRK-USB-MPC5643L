#ifndef LBD_LEDANDBUTTONDRIVER_INCLUDED
#define LBD_LEDANDBUTTONDRIVER_INCLUDED
/**
 * @file lbd_ledAndButtonDriver.h
 * Definition of global interface of module lbd_ledAndButtonDriver.c
 * Simple hardware driver for the LEDs and buttons on the eval board TRK-USB-MPC5643L.
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
 *   lbd_initLEDAndButtonDriver
 *   lbd_setLED
 *   lbd_osSetLED
 *   lbd_getButton
 *   lbd_osGetButtonSw2
 *   lbd_osGetButtonSw3
 *   lbd_osGetButton
 */
/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>

#include "MPC5643L.h"
#include "rtos.h"


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
    the time between two invokations of interface function lbd_osGetButton.
      The range is 2 .. 100. */
#define LBD_DEBOUNCE_TIME_BUTTONS   4

/* The debounce time of the read process of the button states is determined by this
   counter maximum. */
#define LBD_MAX_CNT_BTN_DEBOUNCE    ((LBD_DEBOUNCE_TIME_BUTTONS)/2)

/** Index of implemented system call for switching an LED on or off. */
#define LBD_SYSCALL_SET_LED         16

/** Index of system call for getting the button state, lbd_scSmplHdlr_getButton(). */
#define LBD_SYSCALL_GET_BUTTON      17

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


/** The masks to filter the separate bits in argument \a buttonState of a function of type
    \a lbd_onButtonChangeCallback_t. */
typedef enum lbd_buttonStateMask_t
{
    lbd_btStMask_btnSw2_isOn = 0x01         /** Current state of button SW2. */
    , lbd_btStMask_btnSw2_changed = 0x02    /** Button SW2 went either on or off. */
    , lbd_btStMask_btnSw2_down = 0x04       /** Button SW2 went on. */
    , lbd_btStMask_btnSw2_released = 0x08   /** Button SW2 went off. */

    , lbd_btStMask_btnSw3_isOn = 0x10       /** Current state of button SW3. */
    , lbd_btStMask_btnSw3_changed = 0x20    /** Button SW3 went either on or off. */
    , lbd_btStMask_btnSw3_down = 0x40       /** Button SW3 went on. */
    , lbd_btStMask_btnSw3_released = 0x80   /** Button SW3 went off. */

} lbd_buttonStateMask_t;


/** Type of function pointer to an optional callback, invoked whenever the button status
    changes. See lbd_initLEDAndButtonDriver().\n
      The callback is invoked in the context of the specified process and at the same
    priority level as this I/O driver, i.e. the level at which lbd_task1ms() is regularly
    invoked.
      @param PID
    ID of process, which the callback is invoked in. Will be redundant information in most
    cases.
      @param buttonState
    The two LSBits 0 and 4 indicate the current state of the buttons lbd_bt_button_SW2 and
    lbd_bt_button_SW3, respectively: A set bit for button pressed, a zero bit otherwise.
    The remaining bits indictae the changes compared to the previous callback invocation;
    see type \a lbd_buttonStateMask_t for details. */
typedef int32_t (*lbd_onButtonChangeCallback_t)( uint32_t PID
                                               , uint8_t buttonState
                                               );

/*
 * Global data declarations
 */

/*
 * Global inline functions
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
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user
 * code will lead to a privileged exception. See lbd_setLED() for the user mode variant of
 * the function.
 */
static inline void lbd_osSetLED(lbd_led_t led, bool isOn)
{
    /* Using .B.PDO implements a byte access to one of the single pad registers. This means
       that we don't have race conditions with other pads (maybe concurrently controlled
       from other contexts). */
    SIU.GPDO[led].B.PDO = isOn? 0: 1;

} /* End of lbd_osSetLED */



/**
 * Switch a single LED on or off. This is a system call to make the LED I/O driver
 * accessible from a user task. It has the same functionality as lbd_osSetLED().
 *   @param led
 * The enumeration value to identify an LED.
 *   @param isOn
 * \a true to switch it on \a false to switch it off.
 *   @remark
 * There are no race-conditions between different LEDs. You need to consider using a
 * critical section only, if one and the same LED is served from different interrupt
 * contexts. This is not handled by this driver.
 *   @remark
 * This function must be called from the user task context only. Any attempt to use it from
 * OS code will lead to undefined behavior.
 */
static inline void lbd_setLED(lbd_led_t led, bool isOn)
{
    rtos_systemCall(LBD_SYSCALL_SET_LED, led, isOn);

} /* End of lbd_setLED */



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
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user
 * code will lead to a privileged exception. User task code can consider using
 * lbd_getButton() instead.
 */
static inline bool lbd_osGetButtonSw2(void)
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

} /* End of lbd_osGetButtonSw2 */




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
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user
 * code will lead to a privileged exception. User task code can consider using
 * lbd_getButton() instead.
 */
static inline bool lbd_osGetButtonSw3(void)
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

} /* End of lbd_osGetButtonSw3 */




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
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user
 * code will lead to a privileged exception. User task code can consider using
 * lbd_getButton() instead.
 */
static inline bool lbd_osGetButton(lbd_button_t button)
{
    if(button == lbd_bt_button_SW2)
        return lbd_osGetButtonSw2();
    else
    {
        assert(button == lbd_bt_button_SW3);
        return lbd_osGetButtonSw3();
    }
} /* End of lbd_osGetButton */



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
 *   @remark
 * This function must be called from the user task context only. Any attempt to use it from
 * OS code will lead to undefined behavior.
 */
static inline bool lbd_getButton(lbd_button_t button)
{
    return (bool)rtos_systemCall(LBD_SYSCALL_GET_BUTTON, button);

} /* End of lbd_getButton */



/*
 * Global prototypes
 */

/** Initialization of LED driver. */
void lbd_initLEDAndButtonDriver( lbd_onButtonChangeCallback_t onButtonChangeCallback
                               , unsigned int idOfAimedProcess
                               );

/** Regularly called step function of the I/O driver. */
void lbd_task1ms(void);


#endif  /* LBD_LEDANDBUTTONDRIVER_INCLUDED */
