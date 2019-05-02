/**
 * @file mai_main.c
 *   The main entry point of the C code. The startup code of the MCU is identical to sample
 * "startup"; refer to that sample for details.\n
 *   In this sample the main function applies the API of the startup code to install a
 * regular timer interrupt. The interrupt is used to let the LED on the evaluation board
 * blink as alive indication.\n
 *   This module includes the header \a f2d_float2Double.h and links file \a prf_printf.c
 * in order to provide full support of the stdout functionality of the C library. Function
 * main prints a greeting through RS 232 and USB to the host machine after it has completed
 * the hardware setup and once the interrupts are running. Then it enters an infinite loop,
 * which is used to regularly check the serial input buffer for newly received user input.
 * If a new line of input is available it is interpreted as user command. Different
 * responses are written to the serial output and different actions are taken depending on
 * the command. The actions are related to control of the blinking LED.\n
 *   The (virtual) RS 232 serial connection is implemented through the USB connection you
 * anyway have with the evaluation board. To run the sample you need to run a terminal
 * software on the host and open the connection to the board. The settings are 19200 Bd, 8
 * Bit, no parity, 1 start and stop bit. After reset of the evaluation board, begin with
 * typing help in the terminal program.
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
 *   setD4Frequency
 *   setD4DutyCycle
 *   tokenizeCmdLine
 *   showW
 *   showC
 *   help
 *   version
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
#include "sio_serialIO.h"
#include "f2d_float2Double.h"
#include "tcc_testCppCompilation.h"
#include "mai_main.h"


/*
 * Defines
 */

/** Software version */
#define VERSION "0.12.0"

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

/** The off-time of the period of the regularly blinking LED D4 in unit 2ms. */
static volatile signed int _ledD4_tiOffInMs = 250;

/** The on-time of the period of the regularly blinking LED D4 in unit 2ms. */
static volatile signed int _ledD4_tiOnInMs = 250;

/** The color currently used by the interrupt handler is controlled through selection of
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

    static int cntIsOn_ = 0;
    if(++cntIsOn_ >= _ledD4_tiOnInMs)
        cntIsOn_ = -_ledD4_tiOffInMs;
    lbd_setLED(_ledPIT0Handler, /* isOn */ cntIsOn_ >= 0);

} /* End of interruptPIT0Handler */



/**
 * Change frequency of blinking LED.
 *   @param strTiInMs
 * The desired frequency is specified by a string holding an integer that is interpreted as
 * wanted period time in ms.
 */
static void setD4Frequency(const char strTiInMs[])
{
    signed int i = atoi(strTiInMs);
    if(i == 0)
        i = 1000;
    else if(i < 10)
        i = 10;
    else if(i > 50000)
        i = 50000;

    /* The duty cycle is reset to 50%. */
    float dutyCycle = (float)_ledD4_tiOnInMs
                       / (float)(_ledD4_tiOnInMs + _ledD4_tiOffInMs);
    _ledD4_tiOnInMs = dutyCycle * (float)i;
    _ledD4_tiOffInMs = i - _ledD4_tiOnInMs;
    assert(_ledD4_tiOnInMs >= 0  &&  _ledD4_tiOffInMs >= 0);
    
} /* End of setD4Frequency */



/**
 * Change duty cyle of blinking LED.
 *   @param strDutyCycleInPercent
 * The desired duty cycle is specified by a string holding an integer that is interpreted as
 * as percent of on time in relation to period time.
 */
static void setD4DutyCycle(const char strDutyCycleInPercent[])
{
    signed int i = atoi(strDutyCycleInPercent);
    if(i < 0)
        i = 0;
    else if(i > 100)
        i = 100;

    signed int tiPeriod = _ledD4_tiOnInMs + _ledD4_tiOffInMs;
    _ledD4_tiOnInMs = (signed int)((float)i/100.0 * (float)tiPeriod);
    _ledD4_tiOffInMs = tiPeriod - _ledD4_tiOnInMs;
    assert(_ledD4_tiOnInMs >= 0  &&  _ledD4_tiOffInMs >= 0);

} /* End of setD4DutyCycle */



/**
 * Simple command line parsing. Replace white space in the command line by string
 * termination characters and record the beginnings of the non white space regions.
 *   @param pArgC
 * Prior to call: * \a pArgC is set by the caller to the number of entries available in
 * argV.\n
 *   After return: The number of found arguments, i.e. the number of non white space
 * regions in the command line.
 *   @param argV
 * The vector of arguments, i.e. pointers to the non white space regions in the command
 * line.
 *   @param cmdLine
 * Prior to call: The original command line.\n
 *   After return: White space in the command line is replaced by zero bytes. Note, not
 * necessarily all white space due to the restriction superimposed by \a pArgC.
 */
static void tokenizeCmdLine( unsigned int * const pArgC
                           , const char *argV[]
                           , char * const cmdLine
                           )
{
    char *pC = cmdLine;
    unsigned int noArgsFound = 0;
    while(noArgsFound < *pArgC)
    {
        /* Look for beginning of next argument. */
        while(isspace((int)*pC))
            ++ pC;

        /* Decide if we found a new argument of if we reached the end of the command line. */
        if(*pC != '\0')
        {
            /* New argument found. Record the beginning. */
            argV[noArgsFound++] = pC;

            /* Look for its end. */
            do
            {
                ++ pC;
            }
            while(*pC != '\0'  && !isspace((int)*pC));

            if(*pC != '\0')
            {
                /* There are characters left in the command line. Terminate the found
                   argument and continue with the outer loop. */
                * pC++ = '\0';
            }
            else
            {
                /* Command line has been parsed completely, leave outer loop and return. */
                break;
            }
        }
        else
        {
            /* Command line has been parsed completely, leave outer loop and return. */
            break;

        } /* End if(Further non white space region found?) */

    } /* End while(Still room left in ArgV) */

    *pArgC = noArgsFound;

} /* End of tokenizeCmdLine */



/**
 * GPL proposes 'show w', see http://www.gnu.org/licenses/gpl-3.0.html (downloaded
 * Oct 27, 2017)
 */
static void showW()
{
    static const char gplShowW[] =
    "\rGNU LESSER GENERAL PUBLIC LICENSE\r\n"
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

} /* End of showW */



/**
 * GPL proposes 'show c', see http://www.gnu.org/licenses/gpl-3.0.html (downloaded
 * Oct 27, 2017)
 */
static void showC()
{
    static const char gplShowC[] =
    "\rTRK-USB-MPC5643LAtGitHub - printf, demonstrate use of C lib's stdout with serial"
    " interface\r\n"
    "Copyright (C) 2017-2019  Peter Vranken\r\n"
    "\r\n"
    "This program is free software: you can redistribute it and/or modify\r\n"
    "it under the terms of the GNU Lesser General Public License as published\r\n"
    "by the Free Software Foundation, either version 3 of the License, or\r\n"
    "(at your option) any later version.\r\n"
    "\r\n"
    "This program is distributed in the hope that it will be useful,\r\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\r\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\r\n"
    "GNU Lesser General Public License for more details.\r\n"
    "\r\n"
    "You should have received a copy of the GNU Lesser General Public License\r\n"
    "along with this program.  If not, see <https://www.gnu.org/licenses/>.\r\n";

    puts(gplShowC);

} /* End of showC */



/**
 * Print version designation.
 */
static void version()
{
    static const char version[] =
    "\rTRK-USB-MPC5643LAtGitHub - printf, demonstrate use of C lib's stdout with serial"
    " interface\r\n"
    "Copyright (C) 2017-2019  Peter Vranken\r\n"
    "Version " VERSION "\r\n";

    puts(version);
    
} /* End of version */


/**
 * Print usage text.
 */
static void help()
{
    static const char help[] =
    "\rTRK-USB-MPC5643LAtGitHub - printf, demonstrate use of C lib's stdout with serial"
    " interface\r\n"
    "Copyright (C) 2017-2019  Peter Vranken\r\n"
    "Type:\r\n"
    "help: Get this help text\r\n"
    "show c, show w: Show details of software license\r\n"
    "green, red: Switch LED color. The color may be followed by the desired period time"
      " in ms and the duty cycle in percent\r\n"
    "hello en, hello de: Call C++ code to print a greeting\r\n"
    "time: Print current time\r\n"
    "timing: Do some output and measure execution time\r\n"
    "version: Print software version designation\r\n";

    fputs(help, stderr);

} /* End of help */



/**
 * Entry point into C code. The C main function is entered without arguments and despite of
 * its return code definition it must never be left. (Returning from main would enter an
 * infinite loop in the calling assembler startup code.)
 */
void main()
{
    /* Init core HW of MCU so that it can be safely operated. */
    ihw_initMcuCoreHW();

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
    sio_initSerialInterface(/* baudRate */ 19200);

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

    /* Test the call of C++ implemented functionality. */
    unsigned int cntCppCalls ATTRIB_DBG_ONLY = tcc_sayHello(/* isEnglish */ true);
    assert(cntCppCalls == 1);
    cntCppCalls = tcc_sayHello(/* isEnglish */ false);
    assert(cntCppCalls == 2);
    cntCppCalls = tcc_sayHello(/* isEnglish */ false);
    assert(cntCppCalls == 3);
    cntCppCalls = tcc_sayHello(/* isEnglish */ true);
    assert(cntCppCalls == 4);
    
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
            char inputMsg[80+1];
            if(sio_getLine(inputMsg, sizeOfAry(inputMsg)) != NULL)
            {
                const char *argV[10];
                unsigned int argC = sizeOfAry(argV);
                tokenizeCmdLine(&argC, argV, inputMsg);
                if(argC >= 1)
                {
                    /* Echo user input. */
                    sio_writeSerial("You've typed: ", sizeof("You've typed: ")-1);
                    unsigned int u;
                    for(u=0; u<argC; ++u)
                    {
                        sio_writeSerial(argV[u], strlen(argV[u]));
                        sio_writeSerial(" ", 1);
                    }
                    sio_writeSerial("\r\n", 2);

                    /* Interpret the input as possible command. */
                    if(strcmp(argV[0], "green") == 0)
                    {
                        /* To avoid race conditions with the interrupt, we require a
                           critial section. */
                        /// @todo Double-check, is likely wrong, read-modify-write involved?
                        uint32_t msr = ihw_enterCriticalSection();
                        {
                            lbd_setLED(_ledPIT0Handler, /* isOn */ false);
                            _ledPIT0Handler = lbd_led_D4_grn;
                        }
                        ihw_leaveCriticalSection(msr);

                        /* Color followed by period time? Change frequency accordingly. */
                        if(argC >= 2)
                            setD4Frequency(argV[1]);

                        /* Period time followed by duty cyle? Change DC accordingly. */
                        if(argC >= 3)
                            setD4DutyCycle(argV[2]);
                    }
                    else if(strcmp(argV[0], "red") == 0)
                    {
                        uint32_t msr = ihw_enterCriticalSection();
                        {
                            lbd_setLED(_ledPIT0Handler, /* isOn */ false);
                            _ledPIT0Handler = lbd_led_D4_red;
                        }
                        ihw_leaveCriticalSection(msr);

                        /* Color followed by period time? Change frequency accordingly. */
                        if(argC >= 2)
                            setD4Frequency(argV[1]);

                        /* Period time followed by duty cyle? Change DC accordingly. */
                        if(argC >= 3)
                            setD4DutyCycle(argV[2]);
                    }
                    else if(strcmp(argV[0], "show") == 0)
                    {
                        if(argC >= 2)
                        {
                            if(strcmp(argV[1], "c") == 0)
                                showC();
                            else if(strcmp(argV[1], "w") == 0)
                                showW();
                        }
                    }
                    else if(strcmp(argV[0], "hello") == 0)
                    {
                        bool isEnglish = true;
                        
                        /* Language demanded? */
                        if(argC >= 2)
                        {
                            if(strcmp(argV[1], "de") == 0)
                                isEnglish = false;
                            else if(strcmp(argV[1], "en") != 0)
                            {
                                iprintf( "Command C++: Language is either English (\"en\") or"
                                         " German (\"de\") but got \"%s\"\r\n"
                                       , argV[1]
                                       );
                            }
                        }
                        tcc_sayHello(isEnglish);
                    }
                    else if(strcmp(argV[0], "help") == 0)
                        help();
                    else if(strcmp(argV[0], "version") == 0)
                        version();
                    else if(strcmp(argV[0], "time") == 0)
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
                    else if(strcmp(argV[0], "timing") == 0)
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

                    } /* End if/else if(Command) */

                    cntIdleLoops = 0;

                } /* End if(User input contains possible command) */
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
