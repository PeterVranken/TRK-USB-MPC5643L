/**
 * @file mai_main.c
 * The main entry point of the C code. This sample demonstrates the use of the ADC driver. A
 * number of channels is activated under which the three channels, which are connected to
 * the temperature sensors available on the TRK-USB-MPC5643L.\n
 *   The conversion results are printed to the RS-232 connection. A terminal can be
 * connected using a Baud rate of 115200 Bd, 8 Bits, no parity, 1 stop bit.\n
 *   The two LEDs are controlled by temperature decrease. Normally, they are red but if the
 * temperature suddenly drops they change temporarily to green. The color can be changed
 * e.g. by blowing at the chips mounted on the PCB.\n
 *   Besides using the ADC API to read the conversion results the sample code demonstrates
 * how application code that evaluates the ADC readings can be synchronized with the
 * conversions. To avoid sampling errors at higher signal frequencies it is generally a
 * must to have a hardware timer to regularly trigger all conversions. If the same clock
 * can be used for the scheduler of the application tasks then a design becomes possible,
 * where the ADC results are synchronously acquired by hardware and processed by software
 * and this without fearing any race conditions. This is, what the sample shows.
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
 *   main
 * Local functions
 *   task1ms
 *   task5ms
 *   task10ms
 *   task100ms
 *   printAllChannelResults
 *   getTBL
 *   task1000ms
 *   task10000ms
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "sio_serialIO.h"
#include "f2d_float2Double.h"
#include "RTOS.h"
#include "gsl_systemLoad.h"
#include "lbd_ledAndButtonDriver.h"
#include "tac_mcuTestAndCalibrationData.h"
#include "adc_eTimerClockedAdc.h"


/*
 * Defines
 */


/*
 * Local type definitions
 */

/** The enumeration of all tasks; the values are the task IDs. Actually, the ID is provided
    by the RTOS at runtime, when registering the task. However, it is guaranteed that the
    IDs, which are dealt out by rtos_registerTask() form the series 0, 1, 2, ..., 7. So
    we don't need to have a dynamic storage of the IDs; we define them as constants and
    double-check by assertion that we got the correct, expected IDs. Note, this requires
    that the order of registering the tasks follows the order here in the enumeration. */
enum
{
    idTask1ms = 0
    , idTask5ms
    , idTask10ms
    , idTask100ms
    , idTask1000ms
    , idTask10000ms

    /** The number of tasks to register. */
    , noTasks

    /** The idle task is not a task under control of the RTOS and it doesn't have an ID.
        We assign it a pseudo task ID that is used to store some task related data in the
        same array here in this sample application as we do by true task ID for all true
        tasks. */
    , pseudoIdTaskIdle = noTasks
};


/** The RTOS uses constant task priorities, which are defined here. (The concept and
    architecture of the RTOS allows dynamic changing of a task's priority at runtime, but
    we didn't provide an API for that yet; where are the use cases?) */
enum
{
    prioTaskIdle = 0
    , prioTask10000ms
    , prioTask1000ms
    , prioTask100ms
    , prioTask10ms
    , prioTask5ms
    , prioTask1ms

    /* Interrupt service routine use typically priorities higher than any task. Make this
       information available. */
    , prioIsrLowest
    , prioTaskHighest = prioIsrLowest-1
};



/*
 * Local prototypes
 */


/*
 * Data definitions
 */

/** The average CPU load produced by all tasks and interrupts in tens of percent. */
unsigned int mai_cpuLoad = 1000;

/** A cycle counter for the idle task's main loop. */
volatile unsigned int mai_cntTaskIdle = 0;

/* Statistics on the number of successful and unsuccessful conversions. */
volatile unsigned long mai_noConversionsFailed = 0
                     , mai_noConversionsOk = 0;


/*
 * Function implementation
 */

/**
 * Task function, cyclically activated every n Milliseconds. Does nothing.
 */
#define TASK(tiCycleInMs)                                \
static void task##tiCycleInMs##ms(void)                  \
{}
//TASK(1)
TASK(5)
TASK(10)
TASK(100)
//TASK(1000)
TASK(10000)
#undef TASK



/**
 * Task function, cyclically activated every 1 ms. It checks the up and down of the
 * measured temperatures and controls the LEDs accordingly.
 */
static void task1ms(void)
{
#define FILTER_COEF_TEMP_SLOW   0.999f
#define FILTER_COEF_TEMP_FAST   0.9f
#define TEMP_DELTA_MIN          0.1 /* degree Celsius */
    static float _TSENS_slow = 25.0
               , _T_u4_slow = 25.0
               , _TSENS_fast = 25.0
               , _T_u4_fast = 25.0;
               
    /* Read the current chip temperature. */
    const float TSENS = (adc_getTsens0() + adc_getTsens1()) / 2.0f;
    
    /* Smooth the reading and/or average the readings. */
    _TSENS_slow = FILTER_COEF_TEMP_SLOW*_TSENS_slow + (1.0f - FILTER_COEF_TEMP_SLOW)*TSENS;
    _TSENS_fast = FILTER_COEF_TEMP_FAST*_TSENS_fast + (1.0f - FILTER_COEF_TEMP_FAST)*TSENS;
    
    /* Get last reading of temperature measure by external chip u4. */
    const float T_u4 = (adc_getChannelVoltage(adc_adc0_idxChn01) - 0.6f) / 0.01f;
    
    /* Smooth the reading and/or average the readings. */
    _T_u4_slow = FILTER_COEF_TEMP_SLOW*_T_u4_slow + (1.0f - FILTER_COEF_TEMP_SLOW)*T_u4;
    _T_u4_fast = FILTER_COEF_TEMP_FAST*_T_u4_fast + (1.0f - FILTER_COEF_TEMP_FAST)*T_u4;

    /* Compare the current reading with the averaged reading in order to detect the
       declination. The declination controls the color of the LEDs. A hysteresis avoids
       flickering. */
    if(_T_u4_fast > _T_u4_slow/ + TEMP_DELTA_MIN/10.0f)
    {
        lbd_setLED(lbd_led_D4_grn, /* isOn */ false);
        lbd_setLED(lbd_led_D4_red, /* isOn */ true);
    }
    else if(_T_u4_fast < _T_u4_slow - TEMP_DELTA_MIN)
    {
        lbd_setLED(lbd_led_D4_grn, /* isOn */ true);
        lbd_setLED(lbd_led_D4_red, /* isOn */ false);
    }

    if(_TSENS_fast > _TSENS_slow + TEMP_DELTA_MIN/10.0f)
    {
        lbd_setLED(lbd_led_D5_grn, /* isOn */ false);
        lbd_setLED(lbd_led_D5_red, /* isOn */ true);
    }
    else if(_TSENS_fast < _TSENS_slow - TEMP_DELTA_MIN)
    {
        lbd_setLED(lbd_led_D5_grn, /* isOn */ true);
        lbd_setLED(lbd_led_D5_red, /* isOn */ false);
    }

#undef TEMP_DELTA_MIN
#undef FILTER_COEF_TEMP
} /* End of task1ms */



/**
 * Print the conversion results of all channels that are enabled by configuration.
 */
static void printAllChannelResults(void)
{
#if ADC_USE_ADC_0_CHANNEL_00 == 1
    printf( "ADC_0, Chn  0: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn00))
          );
#endif
#if ADC_USE_ADC_0_CHANNEL_01 == 1
    printf( "ADC_0, Chn  1: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn01))
          );
#endif
#if ADC_USE_ADC_0_CHANNEL_02 == 1
    printf( "ADC_0, Chn  2: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn02))
          );
#endif
#if ADC_USE_ADC_0_CHANNEL_03 == 1
    printf( "ADC_0, Chn  3: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn03))
          );
#endif
#if ADC_USE_ADC_0_CHANNEL_04 == 1
    printf( "ADC_0, Chn  4: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn04))
          );
#endif
#if ADC_USE_ADC_0_CHANNEL_05 == 1
    printf( "ADC_0, Chn  5: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn05))
          );
#endif
#if ADC_USE_ADC_0_CHANNEL_06 == 1
    printf( "ADC_0, Chn  6: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn06))
          );
#endif
#if ADC_USE_ADC_0_CHANNEL_07 == 1
    printf( "ADC_0, Chn  7: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn07))
          );
#endif
#if ADC_USE_ADC_0_CHANNEL_08 == 1
    printf( "ADC_0, Chn  8: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn08))
          );
#endif
#if ADC_USE_ADC_0_CHANNEL_10 == 1
    printf( "ADC_0, Chn 10: %.3fV (%hu)\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn10))
          , adc_getChannelRawValue(adc_adc0_idxChn10)
          );
#endif
#if ADC_USE_ADC_0_CHANNEL_11 == 1
    printf( "ADC_0, Chn 11: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn11))
          );
#endif
#if ADC_USE_ADC_0_CHANNEL_12 == 1
    printf( "ADC_0, Chn 12: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn12))
          );
#endif
#if ADC_USE_ADC_0_CHANNEL_13 == 1
    printf( "ADC_0, Chn 13: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn13))
          );
#endif
#if ADC_USE_ADC_0_CHANNEL_14 == 1
    printf( "ADC_0, Chn 14: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc0_idxChn14))
          );
#endif


#if ADC_USE_ADC_1_CHANNEL_00 == 1
    printf( "ADC_1, Chn  0: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn00))
          );
#endif
#if ADC_USE_ADC_1_CHANNEL_01 == 1
    printf( "ADC_1, Chn  1: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn01))
          );
#endif
#if ADC_USE_ADC_1_CHANNEL_02 == 1
    printf( "ADC_1, Chn  2: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn02))
          );
#endif
#if ADC_USE_ADC_1_CHANNEL_03 == 1
    printf( "ADC_1, Chn  3: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn03))
          );
#endif
#if ADC_USE_ADC_1_CHANNEL_04 == 1
    printf( "ADC_1, Chn  4: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn04))
          );
#endif
#if ADC_USE_ADC_1_CHANNEL_05 == 1
    printf( "ADC_1, Chn  5: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn05))
          );
#endif
#if ADC_USE_ADC_1_CHANNEL_06 == 1
    printf( "ADC_1, Chn  6: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn06))
          );
#endif
#if ADC_USE_ADC_1_CHANNEL_07 == 1
    printf( "ADC_1, Chn  7: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn07))
          );
#endif
#if ADC_USE_ADC_1_CHANNEL_08 == 1
    printf( "ADC_1, Chn  8: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn08))
          );
#endif
#if ADC_USE_ADC_1_CHANNEL_10 == 1
    printf( "ADC_1, Chn 10: %.3fV (%hu)\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn10))
          , adc_getChannelRawValue(adc_adc1_idxChn10)
          );
#endif
#if ADC_USE_ADC_1_CHANNEL_11 == 1
    printf( "ADC_1, Chn 11: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn11))
          );
#endif
#if ADC_USE_ADC_1_CHANNEL_12 == 1
    printf( "ADC_1, Chn 12: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn12))
          );
#endif
#if ADC_USE_ADC_1_CHANNEL_13 == 1
    printf( "ADC_1, Chn 13: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn13))
          );
#endif
#if ADC_USE_ADC_1_CHANNEL_14 == 1
    printf( "ADC_1, Chn 14: %.3fV\r\n"
          , f2d(adc_getChannelVoltage(adc_adc1_idxChn14))
          );
#endif
} /* End of printAllChannelResults */



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
 * Task function, cyclically activated every 1000 ms. Prints some measurement results.
 */
static void task1000ms(void)
{
    static uint32_t tiLastCycle_ = 0;
    const uint32_t tiNow = getTBL();
    float tiCycleInS = 8.33333333e-9*(tiNow - tiLastCycle_);
    tiLastCycle_ = tiNow;

    unsigned short ageOfConversionResult = USHRT_MAX;
    float chn01, chn10, chn15, tsens0, tsens1;
    
#if ADC_USE_ADC_0_CHANNEL_01 == 1
    chn01 = adc_getChannelVoltageAndAge(&ageOfConversionResult, adc_adc0_idxChn01);
#else
    chn01 = -1.0f;
#endif
#if ADC_USE_ADC_0_CHANNEL_10 == 1
    chn10 = adc_getChannelVoltageAndAge(&ageOfConversionResult, adc_adc0_idxChn10);
#else
    chn10 = -1.0f;
#endif
#if ADC_USE_ADC_0_CHANNEL_15 == 1
    chn15 = adc_getChannelVoltageAndAge(&ageOfConversionResult, adc_adc0_idxChn15);
#else
    chn15 = -1.0f;
#endif

#if ADC_USE_ADC_0_CHANNEL_15 == 1
    tsens0 = adc_getTsens0();
#else
    tsens0 = -1.0f;
#endif
#if ADC_USE_ADC_1_CHANNEL_15 == 1
    tsens1 = adc_getTsens1();
#else
    tsens1 = -1.0f;
#endif

    if(ageOfConversionResult < 1)
    {
        ++ mai_noConversionsOk;
        printf( "%.6fs: chn 1: %.3fV = %.1fC, chn 10: %.3fV (%hu), chn 15: %.3fV,"
                " TSENS_0=%.1fC, TSENS_1=%.1fC\r\n"
              , f2d(tiCycleInS)
              , f2d(chn01)
              , f2d((chn01-0.6)/0.01)
              , f2d(chn10)
#if ADC_USE_ADC_0_CHANNEL_10 == 1
              , adc_getChannelRawValue(adc_adc0_idxChn10)
#else
              , USHRT_MAX
#endif
              , f2d(chn15)
              , f2d(tsens0)
              , f2d(tsens1)
              );
        printAllChannelResults();
    }
    else
    {
        ++ mai_noConversionsFailed;
        printf( "%.6fs: Conversion result is stale: %hu cycles\r\n"
              , f2d(tiCycleInS)
              , ageOfConversionResult
              );
    }
} /* End of task1000ms */



/**
 * Entry point into C code. The C main function is entered without arguments and despite of
 * its return code definition it must never be left. (Returning from main would enter an
 * infinite loop in the calling assembler startup code.)
 */
void main(void)
{
    /* The first operation of the main function is the call of ihw_initMcuCoreHW(). The
       assembler implemented startup code has brought the MCU in a preliminary working
       state, such that the C compiler constructs can safely work (e.g. stack pointer is
       initialized, memory access through MMU is enabled).
         ihw_initMcuCoreHW() does the remaining hardware initialization, that is still
       needed to bring the MCU in a basic stable working state. The main difference to the
       preliminary working state of the assembler startup code is the selection of
       appropriate clock rates. Furthermore, the interrupt controller is configured.
         This part of the hardware configuration is widely application independent. The
       only reason, why this code has not been called directly from the assembler code
       prior to entry into main() is code transparency. It would mean to have a lot of C
       code without an obvious point, where it is called. */
    ihw_initMcuCoreHW();

    /* Read the test and calibration data of the MCU. The data is specific to the device
       the code is running on and stored in the flash ROM individually at production time. */
    tac_initTestAndCalibrationDataAry();

    /* Initialize the serial output channel as prerequisite of using printf. */
    sio_initSerialInterface(/* baudRate */ 115200);

    /* Initialize the driver for the LEDs and buttons mounted on the TRK-USB-MPC5643L. */
    lbd_initLEDAndButtonDriver();
    lbd_setLED(lbd_led_D4_grn, /* isOn */ true);
    lbd_setLED(lbd_led_D5_grn, /* isOn */ true);

    /* Initialize ADC hardware for measuring the temperatures.\n
         The RTOS' main clock tick function is specified as callback, which is called from
       the end-of-conversion interrupt of the ADC driver, after it has fetched the new data
       from the ADC hardware; this way the application tasks are running strictly in sync
       with the conversions (and an application task can access the data without fearing
       any race condition).\n
         Note, we use the notification callback to trigger the RTOS scheduler. This
       requires the highest available interrupt priority. */
    adc_initDriver(/* priorityOfIRQ */ 15, /* cbEndOfConversion */ rtos_onOsTimerTick);
    
    /* Route analog input voltage to ADC. We use AN1 of ADC_0, port B8, PCR[24]. This
       connects the output of temperature chip u4 on the TRK-USB-MPC5643L to the ADC. */
    SIUL.PCR[24].B.APC = 1;

    /* The RTOS is restricted to eight tasks at maximum. */
    _Static_assert(noTasks <= 8, "RTOS only supports eight tasks");

    /* Register the application tasks at the RTOS. Note, we do not really respect the ID,
       which is assigned to the task by the RTOS API rtos_registerTask(). The returned
       value is redundant. This technique requires that we register the task in the right
       order and this requires in practice a double-check by assertion - later maintenance
       errors are unavoidable otherwise. */
    unsigned int idTask;
#define REGISTER_TASK(tiCycle)                                                      \
    {                                                                               \
        idTask =                                                                    \
        rtos_registerTask( &(rtos_taskDesc_t){ .taskFct = task##tiCycle##ms         \
                                             , .tiCycleInMs = (tiCycle)             \
                                             , .priority = prioTask##tiCycle##ms    \
                                             }                                      \
                         , /* tiFirstActivationInMs */ 0                            \
                         );                                                         \
        assert(idTask == idTask##tiCycle##ms);                                      \
    }

REGISTER_TASK(1)
REGISTER_TASK(5)
REGISTER_TASK(10)
REGISTER_TASK(100)
REGISTER_TASK(1000)
REGISTER_TASK(10000)
#undef REGISTER_TASK

    /* The last check ensures that we didn't forget to register a task. */
    assert(idTask == noTasks-1);

    /* The external interrupts are enabled after configuring the I/O devices. */
    ihw_resumeAllInterrupts();

    /* Start the timers once all interrupts are configured. ADC conversion and RTOS kernel
       start coincidentally because in this sample the ADC complete notification callback
       is used to clock the RTOS. */
    adc_startConversions();

    /* The code down here becomes our idle task. It is executed when and only when no
       application task is running. */

    while(true)
    {
        ++ mai_cntTaskIdle;

        /* Compute the average CPU load. Note, this operation lasts about 1.5s and has a
           significant impact on the cycling speed of this infinite loop. Furthermore, it
           measures only the load produced by the tasks and system interrupts but not that
           of the rest of the code in the idle loop. */
        mai_cpuLoad = gsl_getSystemLoad();

        /* We modify the measured CPU load a bit in order to enable a most simple
           pseudo-floating point output using formatted integer output only. */
        unsigned int cpuLoad = mai_cpuLoad;
        if(cpuLoad >= 1000)
            cpuLoad = 999;
        iprintf("CPU load is 0.%03u\r\n", cpuLoad);
    }
} /* End of main */
