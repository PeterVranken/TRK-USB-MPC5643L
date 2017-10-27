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
 * code without an obvious point, where it is called.\n
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
 *   showC
 *   showW
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
#include "f2d_float2Double.h"
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

    static int cntIsOn = 0;
    if(++cntIsOn >= 500)
        cntIsOn = -500;
    lbd_setLED(_ledPIT0Handler, /* isOn */ cntIsOn >= 0);

} /* End of interruptPIT0Handler */



/**
 * GPL proposes 'show w', see http://www.gnu.org/licenses/gpl-3.0.html (downloaded
 * Oct 27, 2017)
 */
static void showW()
{
    static const char gplShowW[] =
    "\rGNU GENERAL PUBLIC LICENSE\r\n"
    "\r\n"
    "Version 3, 29 June 2007\r\n"
    "\r\n"
    "(...)\r\n"
    "\r\n"
    "15. Disclaimer of Warranty.\r\n"
    "\r\n"
    "THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY\r\n"
    "APPLICABLE LAW. EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT\r\n"
    "HOLDERS AND/OR OTHER PARTIES PROVIDE THE PROGRAM \"AS IS\" WITHOUT\r\n"
    "WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT\r\n"
    "LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A\r\n"
    "PARTICULAR PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF\r\n"
    "THE PROGRAM IS WITH YOU. SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME\r\n"
    "THE COST OF ALL NECESSARY SERVICING, REPAIR OR CORRECTION.\r\n";

    fiprintf(stdout, gplShowW);
    
} /* End of showW() */



/**
 * GPL proposes 'show c', see http://www.gnu.org/licenses/gpl-3.0.html (downloaded
 * Oct 27, 2017)
 */
static void showC()
{
    static const char gplShowC[] =
    "\rTRK-USB-MPC5643LAtGitHub - printf, demonstrate use of C lib's stdout with serial"
    " interface\r\n"
    "Copyright (C) 2017  Peter Vranken\r\n"
    "\r\n"
    "This program is free software: you can redistribute it and/or modify\r\n"
    "it under the terms of the GNU General Public License as published by\r\n"
    "the Free Software Foundation, either version 3 of the License, or\r\n"
    "(at your option) any later version.\r\n"
    "\r\n"
    "This program is distributed in the hope that it will be useful,\r\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\r\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\r\n"
    "GNU General Public License for more details.\r\n"
    "\r\n"
    "You should have received a copy of the GNU General Public License\r\n"
    "along with this program.  If not, see <https://www.gnu.org/licenses/>.\r\n";

    puts(gplShowC);
    
} /* End of showC() */



/**
 * Print usage text.
 */
static void help()
{
    static const char help[] =
    "\rTRK-USB-MPC5643LAtGitHub - printf, demonstrate use of C lib's stdout with serial"
    " interface\r\n"
    "Type:\r\n"
    "help: Get this help text\r\n"
    "show c, show w: Show details of software license\r\n"
    "green, red: Switch LED color\r\n"
    "time: Print current time\r\n"
    "timing: Do some output and measure execution time\r\n";

    fputs(help, stderr);
    
} /* End of help() */



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

    char msg[512];
    snprintf( msg, sizeOfAry(msg)
            , "TRK-USB-MPC5643LAtGitHub - printf  Copyright (C) 2017  Peter Vranken\r\n"
              "This program comes with ABSOLUTELY NO WARRANTY; for details type `show w'.\r\n"
              "This is free software, and you are welcome to redistribute it\r\n"
              "under certain conditions; type `show c' for details.\r\n"
            );
    fputs(msg, stdout);

    /* Sample code from readMe.adoc. */
    float x = 3.14;
    double y = expf(1.0);
    printf("%s=%.2f, %c=%.5g\r\n", "pi", f2d(x), 'e', f2d(y));

    /* System time. (We use floating point for the only reason of proving its correct
       operation. After about 2^24 times 10ms tiNextCycle will no longer increment and the
       software will fail.) All times in seconds. */
    const float tiCycleTime = 0.01f; /* s */
    float tiSys = 0.0f
        , tiNextCycle = tiSys + tiCycleTime;

    unsigned int cntIdleLoops = 0;
    while(true)
    {
        ++ mai_cntIdle;

        if(tiSys > tiNextCycle)
        {
            /* This assert should fire after about two days and halt the software. (Not
               proven.) */
            assert(tiNextCycle + tiCycleTime > tiNextCycle);
            tiNextCycle += tiCycleTime;
            
            /* Look for possible user input through serial interface. */
            char inputMsg[40+1];
            if(sio_getLine(inputMsg, sizeOfAry(inputMsg)) != NULL)
            {
                sio_writeSerial("You've typed: ", sizeof("You've typed: ")-1);
                sio_writeSerial(inputMsg, strlen(inputMsg));
                sio_writeSerial("\r\n", 2);

                /* Interpret the input as possible command. */
                if(strcmp(inputMsg, "green") == 0)
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
                else if(strcmp(inputMsg, "red") == 0)
                {
                    uint32_t msr = ihw_enterCriticalSection();
                    {
                        lbd_setLED(_ledPIT0Handler, /* isOn */ false);
                        _ledPIT0Handler = lbd_led_D4_red;
                    }
                    ihw_leaveCriticalSection(msr);
                }
                else if(strcmp(inputMsg, "show c") == 0)
                    showC();
                else if(strcmp(inputMsg, "show w") == 0)
                    showW();
                else if(strcmp(inputMsg, "help") == 0)
                    help();
                else if(strcmp(inputMsg, "time") == 0)
                {
                    /* Tip: Consider using anywhere in your application the integer
                       variants of printf from the new C library and do not link the
                       floating point standard implementation. This will save ROM space and
                       a lot of CPU load. */
                    unsigned int h, m, s, ms;
                    h = mai_cntIntPIT0 / 3600000;
                    m = s = mai_cntIntPIT0 - h*3600000;
                    m /= 60000;
                    s = ms = s - m*60000;
                    s /= 1000;
                    ms = ms - s*1000;

                    iprintf( "%s: time=%u:%02u:%02u:%03u\r\n"
                           , __func__
                           , h, m, s, ms
                           );
                }
                else if(strcmp(inputMsg, "timing") == 0)
                {
                    static unsigned int cnt_ = 0;
                    uint32_t tiStart = getTBL();

                    puts("Hello World, this is puts\r\n");
                    fputs("Hello World, this is fputs(stdout)\r\n", stdout);
                    fputs("Hello World, this is fputs(stderr)\r\n", stderr);
                    fprintf(stdout, "Hello World, this is fprintf(%s)\r\n", "stdout");
                    fprintf(stderr, "Hello World, this is fprintf(%s)\r\n", "stderr");
                    putchar('x'); putchar('y'); putchar('z'); putchar('\r'); putchar('\n');
                    
                    /* Elapsed time for all output so far, basically measured in 8.33=25/3 ns
                       units. */
                    uint32_t tiPrintNs = 25*(getTBL() - tiStart)/3
                           , tiPrintUs = tiPrintNs / 1000;
                    tiPrintNs -= tiPrintUs * 1000;
                    fiprintf( stdout, "Time to print all the greetings: %u.%03u us\r\n"
                            , tiPrintUs, tiPrintNs
                            );
                    
                    tiStart = getTBL();
                    printf( "%s: cnt_=%i, time=%.3f min=%.3f h\r\n"
                          , "Floating point"
                          , cnt_
                          , f2d(mai_cntIntPIT0/60.0e3)
                          , f2d(mai_cntIntPIT0/3600.0e3)
                          );
                    tiPrintNs = 25*(getTBL() - tiStart)/3;
                    tiPrintUs = tiPrintNs / 1000;
                    fiprintf( stdout, "Time to print previous line: %u.%03u us\r\n"
                            , tiPrintUs, tiPrintNs
                            );

                    ++ cnt_;
                }

                cntIdleLoops = 0;
            }
            else
            {
                if(++cntIdleLoops >= 1000)
                {
                    puts("Type help to get software usage information\r\n");
                    cntIdleLoops = 0;
                }
                    
            } /* if(Got user input?) */

#if 0
            /* Echo meanwhile received input characters, but not that often. */
            if((cnt_ % 10) == 0)
            {
                char inputMsg[256];
                unsigned int u = 0;
                while(true)
                {
                    signed int c = sio_getChar();
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
#endif
        } /* End if(Next main loop) */

        /* Update the system time. This stupidly repeated operation wastes all remaining
           computation time. */
        tiSys = (float)mai_cntIntPIT0 / 1000.0;
#if 0
        /* Test of return from main: After 10s */
        if(mai_cntIntPIT0 >= 10000)
            break;
#endif
    } /* while(true) */
} /* End of main */
