/**
 * @file lbd_ledAndButtonDriver.c
 * 
 *
 * Copyright (C) 2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   lbd_scSmplHdlr_setLED
 *   lbd_scSmplHdlr_getButton
 * Local functions
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
#include "rtos_ivorHandler.h"
#include "lbd_ledAndButtonDriver_defSysCalls.h"
#include "lbd_ledAndButtonDriver.h"


/*
 * Defines
 */
 
/** Abbreviating macro: Choose memory placement for driver owned data objects. \n
      Caution: The macro will solely work with ordinary variables, more complex objects x
    like "struct x" or "void (*x)(void)" will need explicit use of macro #SECTION. */
#define OS_VAR(varName)    varName SECTION(.sdata.OS.varName)


/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
 
/*
 * Data definitions
 */

/** The descriptor of a user task, which is run as notification in case of a button state
    change. */
static rtos_taskDesc_t OS_VAR(_onButtonChangeCallback) =
            { .addrTaskFct = 0
            , .PID = 0
            , .tiTaskMax = RTOS_TI_US2TICKS(1000)
            };

/*
 * Function implementation
 */

/**
 * Initialization of LED driver. The GPIO ports are defined to become outputs and the
 * output values are set such that the LEDs are shut off.
 *   @param onButtonChangeCallback
 * The I/O driver offers the service to poll the current button input status and to inform
 * the application code about any change. The notification is done per callback. Pass NULL
 * if no notification is desired.
 *   @param PID
 * The ID of the process to run the callback in. The value doesn't care if \a
 * onButtonChangeCallback is NULL. The range is 1 ... #RTOS_NO_PROCESSES. It is checked by
 * assertion.
 *   @remark
 * This function must be called from the OS context only. Any attempt to use it in user 
 * code will lead to a privileged exception.
 */
void lbd_initLEDAndButtonDriver( lbd_onButtonChangeCallback_t onButtonChangeCallback
                               , unsigned int PID
                               )
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
    
    /* Save optional callback in global variable. */
    if(onButtonChangeCallback != NULL)
    {
        /* Here we are in trusted code. The passed PID is static configuration data and
           cannot produce an occasional failure. Checking by assertion is appropriate. */
        assert(PID > 0  &&  PID < RTOS_NO_PROCESSES);
        _onButtonChangeCallback.PID = PID;

        _onButtonChangeCallback.addrTaskFct = (uintptr_t)onButtonChangeCallback;
        
        /* A difficult descision: Shall we generally set a time budget for all user code?
           This may rarely produce an exception, which can leave the user code in an
           inconsistent state, such set subsequent failures result. Even in safe system, a
           potentially not returning user function may be not critical: There will be a
           higher prioritized supervisory task to recognize this situation and to bring the
           system in a safe state. */
        assert(_onButtonChangeCallback.tiTaskMax > 0);
    }    
} /* End of lbd_initLEDAndButtonDriver */



/**
 * Sample implementation of a system call of conformance class "simple". Such a
 * system call can already be implemented in C but it needs to be run with all interrupts
 * suspended. It cannot be preempted. Suitable for short running services only.\n
 *   Here we use the concept to implement an I/O driver for the four LEDs on the eval board
 * TRK-USB-MPC5643L.
 *   @return
 * The value of the argument \a isOn is returned.
 *   @param pidOfCallingTask
 * Process ID of calling user task.
 *   @param led
 * The LED to address to.
 *   @param isOn
 * Switch the selected LED either on or off.
 */
uint32_t lbd_scSmplHdlr_setLED( uint32_t pidOfCallingTask ATTRIB_UNUSED
                              , lbd_led_t led, bool isOn
                              )
{
    /* A safe, "trusted" implementation needs to double check the selected LED in order to
       avoid undesired access to I/O ports other than the four true LED ports. */
    if(led != lbd_led_D4_grn  &&  led != lbd_led_D4_red  &&  led != lbd_led_D5_grn
       &&  led != lbd_led_D5_red
      )
    {
        /* Abort this system call and the calling user task and count this event as an
           error in the process the failing task belongs to. */
        rtos_osSystemCallBadArgument();
    }

    lbd_osSetLED(led, isOn);
    return isOn;
    
} /* End of lbd_scSmplHdlr_setLED */




/**
 * Sample implementation of a system call of conformance class "simple". Such a system call
 * can already be implemented in C but it needs to be run with all interrupts suspended. It
 * cannot be preempted. Suitable for short running services only.\n
 *   Here we use the concept to implement an input function for the two buttons on the eval
 * board TRK-USB-MPC5643L.
 *   @return
 * The value of the argument \a isOn is returned.
 *   @param pidOfCallingTask
 * Process ID of calling user task.
 *   @param led
 * The LED to address to.
 *   @param isOn
 * Switch the selected LED either on or off.
 */
uint32_t lbd_scSmplHdlr_getButton( uint32_t pidOfCallingTask ATTRIB_UNUSED
                                 , lbd_button_t button
                                 )
{
    /* A safe, "trusted" implementation needs to double check the selected button in order to
       avoid undesired access to I/O ports other than the two true button ports on the eval
       board. */
    if(button == lbd_bt_button_SW2  ||  button == lbd_bt_button_SW3)
        return (uint32_t)lbd_osGetButton(button);
    else
    {
        /* Abort this system call and the calling user task and count this event as an
           error in the process the failing task belongs to. */
        rtos_osSystemCallBadArgument();
    }
} /* End of lbd_scSmplHdlr_getButton */



/**
 * Regularly called step function of the I/O driver. This function needs to be called from
 * a regular 1ms operating system task. The button states are read and a callback is
 * invoked in case of a state change.
 */
void lbd_task1ms(void)
{
    /* Polling the buttons is useless if we have no notification callback. */
    if(_onButtonChangeCallback.addrTaskFct != 0)
    {
        /* Read the current button status. */
        const uint8_t stateButtons = (lbd_osGetButtonSw2()? 0x01: 0x0)
                                     | (lbd_osGetButtonSw3()? 0x10: 0x0)
                                     ;

        /* Compare with last state and invoke callback on any difference. */
        static uint8_t OS_VAR(lastStateButtons_) = 0;
        
        if(stateButtons != lastStateButtons_)
        {
            const uint8_t comp = stateButtons
                                 | ((stateButtons ^ lastStateButtons_) << 1)  /* changed  */
                                 | ((stateButtons & ~lastStateButtons_) << 2) /* went on  */
                                 | ((~stateButtons & lastStateButtons_) << 3) /* went off */
                                 ;
            rtos_osRunTask(&_onButtonChangeCallback, /* taskParam */ comp);
            lastStateButtons_ = stateButtons;
        }
    } /* End if(Callback demanded by system configuration?) */    
} /* End of lbd_task1ms */
