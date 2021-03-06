/**
 * @file mai_main.c
 *   The main entry point of the C code. The assembler implemented startup code has been
 * executed and brought the MCU in a preliminary working state, such that the C compiler
 * constructs can safely work (e.g. stack pointer is initialized, memory access through MMU
 * is enabled). After that it branches here, into the C entry point main().\n
 *  The first operation of the main function is the call of the remaining hardware
 * initialization ihw_initMcuCoreHW() that is still needed to bring the MCU in basic stable
 * working state. The main difference to the preliminary working state of the assembler
 * startup code is the selection of appropriate clock rates. Furthermore, the interrupt
 * controller is configured. This part of the hardware configuration is widely application
 * independent. The only reason, why this code has not been called from the assembler code
 * prior to entry into main() is code transparency. It would mean to have a lot of C
 * code without an obvious point, where it is used.\n
 *   In this most basic sample the main function implements the standard Hello World
 * program of the Embedded Software World, the blinking LED.\n
 *   The main function configures the application dependent hardware, which is a cyclic
 * timer (Programmable Interrupt Timer 0, PIT 0) with cycle time 1ms. An interrupt handler
 * for this timer is registered at the Interrupt Controller (INTC). A second interrupt
 * handler is registered for software interrupt 3. Last but not least the LED outputs and
 * button inputs of the TRK-USB-MPC5643L are initialized.\n
 *   The code enters an infinite loop and counts the cycles. Any 500000 cycles it triggers
 * the software interrupt.\n
 *   Both interrupt handlers control one of the LEDs. LED 4 is toggled every 500ms by the
 * cyclic timer interrupt. We get a blink frequency of 1 Hz.\n 
 *   The software interrupt toggles LED 5 every other time it is raised. This leads to a
 * blinking of irrelated frequency.\n
 *   The buttons are unfortunately connected to GPIO inputs, which are not interrupt
 * enabled. We use the timer interrupt handler to poll the status. On button press of
 * Switch 3 the colors of the LEDs are toggled.
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
 *   main
 * Local functions
 *   testFloatingPointConfiguration
 *   interruptSW3Handler
 *   interruptPIT0Handler
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <assert.h>

#include "MPC5643L.h"
#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
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
 
volatile unsigned long mai_cntIdle = 0    /** Counter of cycles of infinite main loop. */
                     , mai_cntIntSW3 = 0  /** Counter of calls of software interrupts 3 */
                     , mai_cntIntPIT0 = 0;/** Counter of calls of PIT 0 interrupts */

/** The color currently used by the interrupt handlers are controlled through selection of
    a pin. The selection is made by global variable. Here for D5. */
static volatile lbd_led_t _ledSW3Handler  = lbd_led_D5_grn;
 
/** The color currently used by the interrupt handlers are controlled through selection of
    a pin. The selection is made by global variable. Here for D4. */
static volatile lbd_led_t _ledPIT0Handler = lbd_led_D4_red;


/*
 * Function implementation
 */

/**
 * Some floating point operations in order to test the compiler configuration.
 */
static void testFloatingPointConfiguration()
{
    volatile float x, y = 99.0f, z;
    x = y / 3;
    x = y / 3.0f;
    x = y / 3.0;
    
    z = y / x;
    z = y * x;
    z = y + x;
    z = y - x;
    z = y + 56ul;
    
    x = 3.1415f / 4.0f;
    y = sin(x);
    y = sinf(x);
    y = cos(x);
    y = cosf(x);
    
    x = 1.0;
    y = exp(x);
    y = expf(x);
    y = log(x);
    y = logf(x);
    y = pow10(x);
    y = pow10f(x);
    
    x = 0.0f;
    y = z / x;
    y = log(x);
    y = logf(x);
    x = -1.0;
    y = sqrt(x);
    y = sqrtf(x);
    
    volatile double a, b = 99.0f, c;
    a = x + z;
    a = b / 3;
    a = b / 3.0f;
    a = b / 3.0;
    
    c = b / a;
    c = b * a;
    c = b + a;
    c = b - a;
    c = b + 56ul;
    
    a = 3.1415f / 4.0f;
    b = sin(a);
    b = sinf(a);
    b = cos(a);
    b = cosf(a);
    
    a = 1.0;
    b = exp(a);
    b = expf(a);
    b = log(a);
    b = logf(a);
    b = pow10(a);
    b = pow10f(a);
    
    a = 0.0f;
    b = c / a;
    b = log(a);
    b = logf(a);
    a = -1.0;
    b = sqrt(a);
    b = sqrtf(a);
    
    /* Give us a chance to see the last result in the debugger prior to leaving scope. */
    b = 0.0;

} /* testFloatingPointConfiguration */



/**
 * Interrupt handler that serves the SW interrupt 3.
 */
static void interruptSW3Handler()
{
    ++ mai_cntIntSW3;

    /* Acknowledge our SW interrupt 3 (test) in the causing HW device. */
    INTC.SSCIR3.R = 1<<0;
    
    /* Access to LED doesn't require a critical section since this interrupt is registered
       as non preemptable. */
    static int cntIsOn = 0;
    if(++cntIsOn >= 1)
        cntIsOn = -1;
    lbd_setLED(_ledSW3Handler, /* isOn */ cntIsOn >= 0);
    
} /* End of interruptSW3Handler */



/**
 * Interrupt handler that serves the interrupt of Programmable Interrupt Timer 0.
 */
static void interruptPIT0Handler()
{
    ++ mai_cntIntPIT0;

    /* Acknowledge the interrupt in the causing HW device. */
    PIT.TFLG0.B.TIF = 0x1;
    
    /* Read the current button status to possibly toggle the LED colors. */
    static bool lastStateButton = false;
    if(lbd_getButton(lbd_bt_button_SW3))
    {
        if(!lastStateButton)
        {
            /* Button down event: Toggle colors */
            static unsigned int cntButtonPress_ = 0;
            
            lbd_setLED(_ledSW3Handler, /* isOn */ false);
            lbd_setLED(_ledPIT0Handler, /* isOn */ false);
            _ledSW3Handler  = (cntButtonPress_ & 0x1) != 0? lbd_led_D5_red: lbd_led_D5_grn;
            _ledPIT0Handler = (cntButtonPress_ & 0x2) != 0? lbd_led_D4_red: lbd_led_D4_grn;
            
            lastStateButton = true;
            ++ cntButtonPress_;
        }
    }
    else
        lastStateButton = false;
            
    /* Access to LED doesn't require a critical section since this interrupt has the
       highest priority. */
    static int cntIsOn = 0;
    if(++cntIsOn >= 500)
        cntIsOn = -500;
    lbd_setLED(_ledPIT0Handler, /* isOn */ cntIsOn >= 0);
    
} /* End of interruptPIT0Handler */




/**
 * Entry point into C code. The C main function is entered without arguments and despite of
 * its return code definition it must never be left. (Returning from main would enter an
 * infinite loop in the calling assembler startup code.)
 */
void main()
{
    /* Init core HW of MCU so that it can be safely operated. */
    ihw_initMcuCoreHW();
    
    /* Install the interrupt handler for SW interrupt 3 (for test only) */
    ihw_installINTCInterruptHandler( interruptSW3Handler
                                   , /* vectorNum */ 3
                                   , /* psrPriority */ 1
                                   , /* isPreemptable */ false
                                   );

    /* Disable timers during configuration. */
    PIT.PITMCR.R = 0x2;

    /* Install the interrupt handler for cyclic timer PIT 0 (for test only) */
    ihw_installINTCInterruptHandler( interruptPIT0Handler
                                   , /* vectorNum */ 59
                                   , /* psrPriority */ 2
                                   , /* isPreemptable */ true
                                   );

    /* Enable timer operation and let them be stopped on debugger entry. */
    PIT.PITMCR.R = 0x1;
    
    /* Peripheral clock has been initialized to 120 MHz. To get a 1ms interrupt tick we
       need to count till 120000. */
    PIT.LDVAL0.R = 120000; /* Interrupt rate 1ms */
    
    /* Enable interrupts by this timer and start it. */
    PIT.TCTRL0.R  = 0x3; 

    /* Initialize the button and LED driver for the eval board. */
    lbd_initLEDAndButtonDriver();
    
    /* The external interrupts are enabled after configuring I/O devices and registering
       the interrupt handlers. */
    ihw_resumeAllInterrupts();
    
    /* Call test of floating point configuration. (Only useful with a connected debugger.) */
    testFloatingPointConfiguration();
    
    while(true)
    {
        ++ mai_cntIdle;
        if((mai_cntIdle % 500000) == 0)
        {
            /* Request SW interrupt 3 (test) */
            INTC.SSCIR3.R = 1<<1;
        }
#if 0
        /* Test of return from main: After 10s */
        if(mai_cntIntPIT0 >= 10000)
            break;
#endif
    }
} /* End of main */
