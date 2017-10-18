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
 *   In this sample the main function implements ... TODOC\n
 *   The main function configures the application dependent hardware, which is a cyclic
 * timer (Programmable Interrupt Timer 0, PIT 0) with cycle time 1ms. An interrupt handler
 * for this timer is registered at the Interrupt Controller (INTC). ... TODOC.\n
 *   ...
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
 *   interruptPIT0Handler
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "sup_settings.h"
#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
#include "sio_serialIO.h"
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
                     , mai_cntIntPIT0 = 0;/** Counter of calls of PIT 0 interrupts */

/** The color currently used by the interrupt handlers are controlled through selection of
    a pin. The selection is made by global variable. Here for D4. */
static volatile lbd_led_t _ledPIT0Handler = lbd_led_D4_red;


/*
 * Function implementation
 */

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
            
            lbd_setLED(_ledPIT0Handler, /* isOn */ false);
            _ledPIT0Handler = (cntButtonPress_ & 0x1) != 0? lbd_led_D4_red: lbd_led_D4_grn;
            
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
    
#ifdef DEBUG
    /* Check linker script. It's error prone with respect to keeping the initialized RAM
       sections and the according initial-data ROM sections strictly in sync. As long as
       this has not been sorted out by a redesign of linker script and startup code we put
       a minimal plausibility check here, which will likely detect typical errors.
         If this assertion fires your initial RAM contents will be corrupt. */
    extern const uint8_t ld_dataSize[0], ld_dataMirrorSize[0];
    assert(ld_dataSize == ld_dataMirrorSize);
#endif

    /* Disable timers during configuration. */
    PIT.PITMCR.R = 0x2;

    /* Install the interrupt handler for cyclic timer PIT 0 (for test only) */
    ihw_installINTCInterruptHandler( interruptPIT0Handler
                                   , /* vectorNum */ 59
                                   , /* psrPriority */ 1
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
    
    /* Initialize the serial interface. */
    ldf_initSerialInterface(/* baudRate */ 19200);

    /* The external interrupts are enabled after configuring I/O devices and registering
       the interrupt handlers. */
    ihw_resumeAllInterrupts();
    
    while(true)
    {
        ++ mai_cntIdle;
        if((mai_cntIdle % 2500000) == 0)
        {
            char msg[256]
               , tmp[128];
            static unsigned int cnt_ = 0;
//            sprintf( msg
//                   , "main: cnt: %u, mai_cntIntPIT0: %lu\r\n"
//                   , cnt_
//                   , mai_cntIntPIT0
//                   );
            strcpy(msg, "main: Hello World, cnt: ");
            utoa(cnt_, tmp, /* base */ 10);
            strcat(msg, tmp);
            strcat(msg, ", mai_cntIntPIT0: ");
            utoa((unsigned int)mai_cntIntPIT0, tmp, /* base */ 10);
            strcat(msg, tmp);
            strcat(msg, "\r\n");
            sio_writeSerial(msg, strlen(msg));
            
            /* Echo meanwhile received input characters, but not that often. */
            if((cnt_ % 10) == 0)
            {
                char inputMsg[256];
                unsigned int u = 0;
                while(true)
                {
                    signed int c = sio_getchar();
                    if(c != -1)
                        inputMsg[u] = (char)c;
                    else
                        inputMsg[u] = '\0';
                        
                    if(u < sizeOfAry(inputMsg)-1)
                        ++ u;
                    else
                    {
                        inputMsg[sizeOfAry(inputMsg)-1] = '\0';
                        break;
                    }
                }
                size_t noChars = strlen(inputMsg);
                if(noChars > 0)
                {
                    sio_writeSerial("You've typed: ", sizeof("You've typed: ")-1);
                    sio_writeSerial(inputMsg, noChars);
                    if(inputMsg[noChars-1] != '\n')
                        sio_writeSerial("\r\n", 2);
                        
                    /* Interpret the input as possible command. */
                    if(strcmp(inputMsg, "green\r\n") == 0)
                    {
                        /* To avoid race conditions with the interrupt, we would actually
                           require a critial section. */
                        uint32_t msr = ihw_enterCriticalSection();
                        {
                            lbd_setLED(_ledPIT0Handler, /* isOn */ false);
                            _ledPIT0Handler = lbd_led_D4_grn;
                        }
                        ihw_leaveCriticalSection(msr);
                    }
                    else if(strcmp(inputMsg, "red\r\n") == 0)
                    {
                        uint32_t msr = ihw_enterCriticalSection();
                        {
                            lbd_setLED(_ledPIT0Handler, /* isOn */ false);
                            _ledPIT0Handler = lbd_led_D4_red;
                        }
                        ihw_leaveCriticalSection(msr);
                    }
                }
            } /* End if(Time to echo console input?) */
            
            ++ cnt_;
        }
#if 0
        /* Test of return from main: After 10s */
        if(mai_cntIntPIT0 >= 10000)
            break;
#endif
    }
} /* End of main */
