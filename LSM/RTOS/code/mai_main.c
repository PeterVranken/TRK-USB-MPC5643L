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
 *   task1s
 *   task1ms
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
#include "sup_settings.h"
#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
#include "RTOS.h"
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
                     , mai_cntTask1s = 0  /** Counter of calls of software interrupts 3 */
                     , mai_cntTask1ms = 0;/** Counter of calls of PIT 0 interrupts */

/** The color currently used by the interrupt handlers are controlled through selection of
    a pin. The selection is made by global variable. Here for D5. */
static volatile lbd_led_t _ledTask1s  = lbd_led_D5_grn;
 
/** The color currently used by the interrupt handlers are controlled through selection of
    a pin. The selection is made by global variable. Here for D4. */
static volatile lbd_led_t _ledTask1ms = lbd_led_D4_red;


/*
 * Function implementation
 */

/**
 * Task function 1s.
 */
static void task1s()
{
    ++ mai_cntTask1s;

    static int cntIsOn_ = 0;
    if(++cntIsOn_ >= 1)
        cntIsOn_ = -1;
    lbd_setLED(_ledTask1s, /* isOn */ cntIsOn_ >= 0);
    
} /* End of task1s */



/**
 * Task 1ms.
 */
static void task1ms()
{
    ++ mai_cntTask1ms;

    /* Read the current button status to possibly toggle the LED colors. */
    static bool lastStateButton_ = false;
    if(lbd_getButton(lbd_bt_button_SW3))
    {
        if(!lastStateButton_)
        {
            /* Button down event: Toggle colors */
            static unsigned int cntButtonPress_ = 0;
            
            lbd_setLED(_ledTask1s, /* isOn */ false);
            lbd_setLED(_ledTask1ms, /* isOn */ false);
            _ledTask1s  = (cntButtonPress_ & 0x1) != 0? lbd_led_D5_red: lbd_led_D5_grn;
            _ledTask1ms = (cntButtonPress_ & 0x2) != 0? lbd_led_D4_red: lbd_led_D4_grn;
            
            lastStateButton_ = true;
            ++ cntButtonPress_;
        }
    }
    else
        lastStateButton_ = false;

    static int cntIsOn_ = 0;
    if(++cntIsOn_ >= 500)
        cntIsOn_ = -500;
    lbd_setLED(_ledTask1ms, /* isOn */ cntIsOn_ >= 0);
    
} /* End of task1ms */




/**
 * Entry point into C code. The C main function is entered without arguments and despite of
 * its return code definition it must never be left. (Returning from main would enter an
 * infinite loop in the calling assembler startup code.)
 */
void main()
{
    /* Init core HW of MCU so that it can be safely operated. */
    ihw_initMcuCoreHW();
    
#ifdef DEBUG
    /* Check linker script. It's error prone with respect to keeping the initialized RAM
       sections and the according initial-data ROM sections strictly in sync. As long as
       this has not been sorted out by a redesign of linker script and startup code we put
       a minimal plausibility check here, which will likely detect typical errors.
         If this assertion fires your initial RAM contents will be corrupt. */
    extern const uint8_t ld_dataSize[0], ld_dataMirrorSize[0];
    assert(ld_dataSize == ld_dataMirrorSize);
#endif

    /* Initialize the button and LED driver for the eval board. */
    lbd_initLEDAndButtonDriver();
    
    /* Register the application tasks at the RTOS. */
    const unsigned int idTask1s = rtos_registerTask
                                    (&(rtos_taskDesc_t){ .taskFct = task1s
                                                       , .contextData = 0
                                                       , .tiCycleInMs = 1000
                                                       , .tiFirstActivationInMs = 100
                                                       , .priority = 1
                                                       }
                                    );
    assert(idTask1s == 0);

    const unsigned int idTask1ms = rtos_registerTask
                                    (&(rtos_taskDesc_t){ .taskFct = task1ms
                                                       , .contextData = 0
                                                       , .tiCycleInMs = 1
                                                       , .tiFirstActivationInMs = 10
                                                       , .priority = 2
                                                       }
                                    );
    assert(idTask1ms == 1);

    /* Initialize the RTOS kernel. */
    rtos_initKernel();

    /* The external interrupts are enabled after configuring I/O devices and initialization
       of the RTOS. This step starts the RTOS kernel. */
    ihw_resumeAllInterrupts();
    
    /* The code down here becomes our idle task. It is executed when and only when no
       application task is running. */

    while(true)
    {
        ++ mai_cntIdle;

#if 0
        /* Test of return from main: After 10s */
        if(mai_cntTask1ms >= 10000)
            break;
#endif
    }
} /* End of main */
