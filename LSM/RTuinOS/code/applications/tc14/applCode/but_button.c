/**
 * @file but_button.cpp
 *   Evaluate the button status and implement a state machine that represents the user
 * interface.
 *
 * Copyright (C) 2013-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   init_button
 *   but_onCheckUserInput
 * Local functions
 */

/*
 * Include files
 */

#include <assert.h>

#include "typ_types.h"
#include "lbd_ledAndButtonDriver.h"
#include "adc_analogInput.h"
#include "clk_clock.h"
#include "but_button.h"


/*
 * Defines
 */


/*
 * Local type definitions
 */

/** The buttons are enumerated. */
typedef enum { btnNone
             , btnShort
             , btnLong
             } enumButtonEvent_t;

/** Main state of button input. */
typedef enum { btState_adcInputSelection
             , btState_rtcAdjustment
             } enumButtonMainState_t;

/*
 * Local prototypes
 */


/*
 * Data definitions
 */

/** State of buuton input: Does the button pair belong to the ADC input selection or to the
    RTC adjustment? */
static enumButtonMainState_t _btnInputState = btState_rtcAdjustment;


/*
 * Function implementation
 */

/**
 * Module initialization.
 */
void init_button()
{
    _btnInputState = btState_rtcAdjustment;
    lbd_setLED(lbd_led_D5_grn, /* isOn */ false);
    lbd_setLED(lbd_led_D5_red, /* isOn */ true);
    
} /* End of init_button */



/**
 * The step function of the state machine that evaluates the buttons and represents the user
 * interface. The state machine is required to debounce the button events like button pressed
 * and button released. Furthermore, this module knows about the clients of the buttons and
 * will notify them accordingly.
 *   @remark
 * This function is triggered by a fast, regular RTOS event. It doesn't have a parameter.
 * Instead, it reads its input directly from the LED and button driver.
 */
void but_onCheckUserInput()
{
    /* The original Arduino code used to have two pairs of buttons, both having the meaning
       of up and down but for two different functions: the ADC input selection and the
       current time of the real time clock.
         To emulate this behavior with the two buttons of the TRK-USB-MPC5643L we need a
       state variable, which assigns the button pair to one of the functions. The state
       toggles between ADC input selection and RTC adjustment and is indicated by the two
       colors of the LED. Red means RTC, green means ADC input.
         A state change is yielded by holding a button down, short pressing is the normal
       use of the button in its state. */
#define CNTS_SHORT  3       /* Unit: Invocation period, about 20ms */
#define CNTS_LONG   100

    /* Poll current button state. */
    bool sw2Down = lbd_getButton(lbd_bt_button_SW2)
       , sw3Down = lbd_getButton(lbd_bt_button_SW3);
       
    /* Measure timing to get the main state; button maintained shortly or long. */
    enumButtonEvent_t btnEventSw2 = btnNone
                    , btnEventSw3 = btnNone;
    static unsigned int cntSw2_ = 0
                      , cntSw3_ = 0;
    if(sw2Down)
    {
        if(++cntSw2_ == 0)
            -- cntSw2_;
        if(cntSw2_ == CNTS_LONG)
            btnEventSw2 = btnLong;
    } 
    else
    {
        if(cntSw2_ >= CNTS_SHORT  &&  cntSw2_ < CNTS_LONG)
            btnEventSw2 = btnShort;
        cntSw2_ = 0;
    }
    if(sw3Down)
    {
        if(++cntSw3_ == 0)
            -- cntSw3_;
        if(cntSw3_ == CNTS_LONG)
            btnEventSw3 = btnLong;
    } 
    else
    {
        if(cntSw3_ >= CNTS_SHORT  &&  cntSw3_ < CNTS_LONG)
            btnEventSw3 = btnShort;
        cntSw3_ = 0;
    }
    
    /* Evaluate the main state and provide feedback about by setting the LED. Toggle main
       state on any button maintained long. */
    if(btnEventSw2 == btnLong  ||  btnEventSw3 == btnLong)
    {
        if(_btnInputState == btState_rtcAdjustment)
        {
            _btnInputState = btState_adcInputSelection;
            lbd_setLED(lbd_led_D5_grn, /* isOn */ true);
            lbd_setLED(lbd_led_D5_red, /* isOn */ false);
        }
        else
        {
            assert(_btnInputState == btState_adcInputSelection);
            _btnInputState = btState_rtcAdjustment;
            lbd_setLED(lbd_led_D5_grn, /* isOn */ false);
            lbd_setLED(lbd_led_D5_red, /* isOn */ true);
        }
                
        /* Ignore other button in same tick as the main state toggles: we wouldn't know
           whether to relate the event to the left or to the entered state. */
        btnEventSw2 = btnNone;
        btnEventSw3 = btnNone;
    }
    else if(btnEventSw2 != btnNone  ||  btnEventSw3 != btnNone)
    {
        if(_btnInputState == btState_rtcAdjustment)
        {
            /* Up and down are used to adjust the real time clock. The number of such
               events is counted; the RTC code acknowledges by decrementing by the number
               of events it has considered.
                 The RTC task is running at a lower priority, so we can safely access its
               global interface without synchronization code. */
            if(btnEventSw2 == btnShort)
                ++ clk_noButtonEvtsUp;
            if(btnEventSw3 == btnShort)
                ++ clk_noButtonEvtsDown;
        }
        else
        {
            /* The buttons right and left are used to switch hence and forth between ADC
               inputs. */
            if(btnEventSw2 == btnShort)
                adc_nextInput(/* up */ true);
            if(btnEventSw3 == btnShort)
                adc_nextInput(/* up */ false);
        }
    }
#undef CNTS_SHORT
#undef CNTS_LONG
} /* End of but_onCheckUserInput */




