/**
 * @file adc_eTimerClockedAdc.c
 * Driver for analog-to-digital conversions with the MPC5643L.\n
 *   Naive, software controlled regular sampling of an analog input cannot guarantee the
 * correct sampling time; SW control can only ensure the correct mean value of the timing
 * but the particular sampling times undergo a jitter. Such a jitter means a sampling
 * error, which increases in first order with the frequency of the input information.
 * Reliable sampling requires triggering the sampling times by a hardware timer without SW
 * interaction.\n
 *   For the MPC5643L, the ADC can be triggered from different I/O devices. The trigger
 * clock is routed from these devices to the ADC by the CTU. This ADC driver configures
 * four devices:\n
 *   - It uses one channel of one of the two Enhanced Motor Control Timer modules
 *     eTimer_0 or eTimer_1 (module eTimer_2 is not connected to the CTU)\n
 *   - The CTU is configured to command the ADCs to do the sampling. It has the eTimer
 *     module as input\n
 *   - The two ADCs are identical configured such that they wait for the CTU commands and
 *     do the conversions\n
 *   The ADC conversion results are written into a global array of values, which serves as
 * API of the module. The array is updated after each conversion cycle. The application
 * code can be notified by callback about the availability of a new result. This supports
 * synchronous processing of the sampled analog input signal.\n
 *   The external inputs to the ADCs are not configured by this driver. The programming of
 * the SIUL to route the MCU pins to the ADC inputs needs to be done by the client code
 * prior to starting the conversions with adc_startConversions().\n
 *   The set of channels, which is converted in every cycle is statically configured at
 * compile time. It is not possible to define or change this set at run-time. Moreover, it
 * is not possible to sample different channels at different rates. The conversion cycle is
 * defined (set of channels, cycle time and settings for a single conversion) and this
 * cycle is repeated all time long. The configuration is done with preprocessor macros,
 * mainly found in the header file adc_eTimerClockedAdc.h.\n
 *   The internal signals (TSENS_0 and TSENS_1) and VREG_1.2V can be configured for
 * conversion as any other channel, however this has specific side effects:\n
 *   - If and only if the temperature signals TSENS_0 and TSENS_1, channels 15, ADC_0 and
 *     ADC_1, respectively, are element of the set of converted channels then there are
 *     APIs to read the temperature signals in degree Celsius\n
 *   - If the reference voltage VREG_1.2V, channels 10, ADC_0 and ADC_1, respectively, are
 *     element of the set of converted channels then the averaged measured reference
 *     voltage is used to calibrate all the other channels in Volt\n
 *   @remark
 * Some configuration items of the driver are hard-coded and not modelled as compile-time
 * #define's. Depending on the application, these settings and thus the implementation of
 * the driver can become subject to modifications. A prominent example is the conversion
 * timing. The related settings are chosen for a rather slow sampling rate but better
 * accuracy; a rate of a few Kilohertz is targeted. Higher rates may require another
 * timing configuration and much higher rates could even require structural code changes,
 * like DMA support.
 *   @remark
 * The configuration settings depend on one another. There are several constraints like: A
 * very short cycle time is impossible if the number of active channels is large. Because
 * we use preprocessor macros for configuration most of the constraints can be checked at
 * compile time using preprocessor conditions and the _Static_assert keyword. Therefore,
 * there are barely run-time errors recognized and reported by the code. A bad
 * configuration will simply not compile.
 *   @remark
 * More documentation about this ADC driver for MPC5643L can be found at
 * https://github.com/PeterVranken/TRK-USB-MPC5643L/tree/master/LSM/ADC.
 *   @remark
 * This module is a modification of the original file
 * https://github.com/PeterVranken/TRK-USB-MPC5643L/blob/master/LSM/ADC/code/ADC/adc_eTimerClockedAdc.c.
 * The end-of-conversion interrupt has been redefined. Optionally, it can become a kernel
 * relevant interrupt, i.e. it may decide to end with context switch. Use case: A scheduler
 * can decide to activate a task, which immediately evaluates the new ADC result.\n
 *   Note, this feature is compilable and useful only if you implement a kernel with the
 * package kernelBuilder, see
 * https://github.com/PeterVranken/TRK-USB-MPC5643L/tree/master/LSM/kernelBuilder for
 * details.
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
 *   adc_initDriver
 *   adc_startConversions
 *   adc_getChannelRawValue
 *   adc_getChannelVoltage
 *   adc_getTsens0
 *   adc_getTsens1
 * Local functions
 *   initETimer
 *   initCTU
 *   compileAdcCommandList
 *   initADC
 *   isrCtuAllConversionsDone
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "rcc_readCalibrationConstants.h"
#include "adc_eTimerClockedAdc.h"
#include "adc_eTimerClockedAdc.inc"


/*
 * Defines
 */

/** The CTU is connected to channel 2 of the timer modules 0 and 1. The module to use can
    be selected. Either state ETIMER_0 or ETIMER_1. */
#define ETIMER  (ETIMER_1)

/* The channel of the eTimer module is fixed to 2; the CTU is not connected to the outputs
   of the other counters in a timer module. Do not change this macro. */
#define TIMER_CHN   (2)

/** For development and debug purpose, the output of eTimer_1, which can be used to trigger
    the ADC conversion, can be routed to a MCU pin, which is port D[1] (pin number depends
    on package). For the TRK-USB-MPC5643L, this pin is connected to connector J1A, A60.
      @remark This setting has no effect in PRODUCTION compilation. In production
    compilation the pin is always disabled.
      @remark If module eTimer_0 is used (see #ETIMER) then no pin is available, which can
    reasonbly be used with board TRK-USB-MPC5643L and this macro needs to be configured 0. */
#define ENABLE_OUTPUT_OF_ETIMER_CHN2    1

/** For development and debug purpose, the trigger output of the CTU can be routed to a MCU
    pin, which is port C[14] (pin number depends on package). For the TRK-USB-MPC5643L,
    this pin is connected to connector J1A, B22.
      @remark This setting has no effect in PRODUCTION compilation. In production
    compilation the pin is always disabled. */
#define ENABLE_OUTPUT_OF_CTU_TRIGGER    1

/* The peripheral clock rate as configured in the startup code. Unit is Hz and value is an
   unsigned long literal. Do not change. */
#define PERIPHERAL_CLOCK_RATE   (120000000ul)


/*
 * Local type definitions
 */


/*
 * Local prototypes
 */


/*
 * Data definitions
 */

/** This callback is invoked from the end-of-conversion interrupt after each CTU cycle and
    after fetching the conversion results from the ADCs. */
#if ADC_ENABLE_INTERRUPT_AS_KERNEL_RELEVANT == 0
static void (*_cbEndOfConversion)(void) = NULL;
#else
static int_ivor4KernelIsr_t _cbEndOfConversion = NULL;
#endif

/** Diagnosis: The number of ever failed conversion cycles since startup. A failed cycle is
    one, where at least one conversion result was not properly available. */
static unsigned int _noFailedConversions = 0;

/** Diagnosis: The number of conversion cycles since the last successful cycle. Normally
    zero. */
static unsigned short _ageOfConversionResults = USHRT_MAX;

/** The conversion results of boths ADCs. ADC_0 comes first, followed by the results of
    ADC_1. */
static uint16_t _conversionResAry[ADC_NO_ACTIVE_CHNS] = {[0 ... ADC_NO_ACTIVE_CHNS-1] = 0};

#if ADC_USE_ADC_0_CHANNEL_10 == 1
/** The averaged and scaled reading of channel 10, ADC_0, which is internally connected to
    signal VREG_1.2V. */
static float _adc0_chn10 = 65536.0f / ADC_ADC_0_REF_VOLTAGE;
#endif

#if ADC_USE_ADC_1_CHANNEL_10 == 1
/** The averaged and scaled reading of channel 10, ADC_1, which is internally connected to
    signal VREG_1.2V. */
static float _adc1_chn10 = 65536.0f / ADC_ADC_1_REF_VOLTAGE;
#endif

/** The TSENS input, chn 15, requires alternating voltage measurement in two sensor modes,
    linear and inverse to lower errors and to make the measurement independent of the
    reference voltage. After each measurement we toggle the mode. Variable _TSENSOR_SEL
    holds the currently used mode. */
#if ADC_USE_ADC_0_CHANNEL_15 == 1  ||  ADC_USE_ADC_1_CHANNEL_15 == 1
static unsigned int _TSENSOR_SEL = 0;
#endif

#if ADC_USE_ADC_0_CHANNEL_15 == 1
/** The averaged two TSENS_0 readings. The initial values are chosen such that the
    computation starts with a value of about 30 degrees Celsius (the actual value is
    significantly device dependent). */
static float _TSENS_0[2] = {[0] = 33500.0f, [1] = 30000.0f};
#endif

#if ADC_USE_ADC_1_CHANNEL_15 == 1
/** The averaged two TSENS_1 readings. The initial values are chosen such that the
    computation starts with a value of about 30 degrees Celsius. */
static float _TSENS_1[2] = {[0] = 33500.0f, [1] = 30000.0f};
#endif

/* Some constants from the device individual test and calibration data, required later for
   the computation of the chip temperature. */
#if ADC_USE_ADC_0_CHANNEL_15 == 1
static float TSENS_0_P1 = 1.0f
           , TSENS_0_P2 = 0.0f
           , TSENS_0_C1 = 1.0f
           , TSENS_0_C2 = 0.0f;
#endif
#if ADC_USE_ADC_1_CHANNEL_15 == 1
static float TSENS_1_P1 = 1.0f
           , TSENS_1_P2 = 0.0f
           , TSENS_1_C1 = 1.0f
           , TSENS_1_C2 = 0.0f;
#endif
#if ADC_USE_ADC_0_CHANNEL_15 == 1  ||  ADC_USE_ADC_1_CHANNEL_15 == 1
# define TSENS_T2   (-40)
# define TSENS_T1   (150)
#endif


/*
 * Function implementation
 */

/**
 * Initialize the common part of the eTimer module and its counter channel 2.\n
 *   Which eTimer module to use is configured by #ETIMER at compile time.\n
 *   The cycle time is configured as #ADC_T_CYCLE_IN_US at compile time.\n
 *   The timer is not yet started. Later use startETimer() to do so.
 */
static void initETimer()
{
    /* The macros down here implicitly assumes a particular clock rate. The boundaries for
       the selection of the prescaler need adaptation for another clock rate. */
    #if PERIPHERAL_CLOCK_RATE != 120000000l
    # error The configuration of the eTimer is based on a fixed clock rate of 120 MHz
    #endif

    /* Which prescaler do we need to implement the wanted cycle time? The lower boundary of
       10us is weak and a bit arbitrary. It will work only if the client code demands only
       a single conversion per cycle and if the data evaluation is very fast. */
    #if ADC_T_CYCLE_IN_US < 10
    # define DIV_AS_PWR_OF_2 0
    # error Cycle time ADC_T_CYCLE_IN_US is configured too little. Values below 10us are \
            not supported
    #elif ADC_T_CYCLE_IN_US <= 1092
    # define DIV_AS_PWR_OF_2 0
    #elif ADC_T_CYCLE_IN_US <= 2184
    # define DIV_AS_PWR_OF_2 1
    #elif ADC_T_CYCLE_IN_US <= 4368
    # define DIV_AS_PWR_OF_2 2
    #elif ADC_T_CYCLE_IN_US <= 8737
    # define DIV_AS_PWR_OF_2 3
    #elif ADC_T_CYCLE_IN_US <= 17475
    # define DIV_AS_PWR_OF_2 4
    #elif ADC_T_CYCLE_IN_US <= 34951
    # define DIV_AS_PWR_OF_2 5
    #elif ADC_T_CYCLE_IN_US <= 69903
    # define DIV_AS_PWR_OF_2 6
    #elif ADC_T_CYCLE_IN_US <= 139807
    # define DIV_AS_PWR_OF_2 7
    #else
    # define DIV_AS_PWR_OF_2 7
    # error Cycle time ADC_T_CYCLE_IN_US is configured too large. Values above 139ms are \
            not supported
    #endif

#if ENABLE_OUTPUT_OF_ETIMER_CHN2 == 1  &&  defined(DEBUG)
    /* Note, this setting is useful for testing but correct only if the macros select
       counter/channel 2 of eTimer 1.
         SMC, 0x4000: Output enable in SoC safe mode
         APC, 0x2000: Enable as analog input
         PA, 0x0c00: 0: GPIO (ALT0), otherwise index of alternate mode, eTimer1, T5 is ALT2
         OBE, 0x0200: Output buffer enable for GPIO mode
         IBE, 0x0100: Input buffer enable
         ODE, 0x0020: 1: Open drain enable, 0: Push/pull driver
         SRC, 0x0004: Slew rate, 0: slow, 1: fast
         WPE, 0x0002: Enable pull up or down
         WPS, 0x0001: 1: pull up, 0: pull down (if enabled with WPE) */
    _Static_assert( &ETIMER.CHANNEL[TIMER_CHN] == &ETIMER_1.CHANNEL[2]
                  , "SIUL configuration for output of eTimer is available only for device"
                    " eTimer_1, channel 2"
                  );
    SIU.PCR[49].R = 0x0800; /* D[1] pin configured as Timer_1, external counter output 2. */
#endif

#define COUNTER (ETIMER.CHANNEL[TIMER_CHN])

    /* All timers are enabled by default and start operation as soon as CTRL1.CNTMODE is
       set. We inhibit this early start. */
    ETIMER.ENBL.R &= ~(1<<TIMER_CHN);

    /* Control register CTRL1:
         CNTMODE, 0xe000: 1 means count rising edges
         PRISRC, 0x1f00, count source: 0x3bbb means peripheral clock devided by 2^0xbbb
         SECSRC, 0x0014, secondary source: not applied */
    COUNTER.CTRL1.B.CNTMODE = 1;
    COUNTER.CTRL1.B.PRISRC = 0x18 + DIV_AS_PWR_OF_2; /* Clock by 8=2^3 */
    COUNTER.CTRL1.B.ONCE = 0; /* Count continuously. */
    COUNTER.CTRL1.B.LENGTH = 1; /* Control period by compare register. Reload from register
                                   LOAD. */
    COUNTER.CTRL1.B.DIR = 1; /* 0: upwards, 1: downwards */
    COUNTER.CTRL1.B.SECSRC = 0; /* default, not applied */

    COUNTER.CTRL2.B.OEN = 1; /* Enable output, is used to trigger CTU. */
    COUNTER.CTRL2.B.COINIT = 0; /* The channel works independently of the others. */
    COUNTER.CTRL2.B.MSTR = 0; /* The channel works independently of the others. */
    COUNTER.CTRL2.B.OUTMODE = 3; /* Toggle output on comp1 or comp2, whatever comes first. */

    COUNTER.CTRL3.B.STPEN = 0; /* Output is still valid when stopped. */
    COUNTER.CTRL3.B.ROC = 0; /* We don't use the captering mechanism and don't reload the
                                compare registers. */
    COUNTER.CTRL3.B.DBGEN = 1; /* Halt counter in debug mode. */

    /* All counter value capturing under control of the secondary input is disabled.
         CMPMODE, 0x0300: Compare registers are connected to count directions. We only use
       comp1 on counting down. */
    COUNTER.CCCTRL.R = 0x0100;

    /* The period of the output signal is configured.
         +1: Because of the output toggle mode the period is twice the counter cycle time.
         The compare registers are set such that they don't affect the timing. COMP1
       triggers the reload at counter value 0 and comp2 never matches at all. */
    #define T_CYCLE_REGVAL(type)                                                            \
                ((type)(0.5 + ((float)PERIPHERAL_CLOCK_RATE / (1u<<(DIV_AS_PWR_OF_2+1)))    \
                              * ((float)ADC_T_CYCLE_IN_US/1e6)                              \
                       )                                                                    \
                 - 1                                                                        \
                )
    _Static_assert( T_CYCLE_REGVAL(int64_t) > 0  &&  T_CYCLE_REGVAL(int64_t) <= 65535
                  , "Cycle time out of range"
                  );
    COUNTER.LOAD.R = T_CYCLE_REGVAL(uint16_t);
    COUNTER.COMP1.R = 0;
    COUNTER.COMP2.R = 0xffff;
    #undef T_CYCLE_REGVAL

    /* Initialize the counter. */
    COUNTER.CNTR.R = 1;

    /* Reset the status bits, including the interrupt flags. */
    COUNTER.STS.R = 0x03ff;

    /* Test only: Enable interrupt on reload. */
    //COUNTER.INTDMA.B.TCF1IE = 1;

#undef COUNTER
#undef DIV_AS_PWR_OF_2
} /* End of initETimer */



/**
 * All ADC conversions are started by the CTU according to the ADC command list it stores
 * in its register file CLR[]. This function fills the command list based on the user
 * configuration.
 */
static void compileAdcCommandList()
{
    /* ADC commands:
       0x8000: CIR, interrupt on done
       0x4000: First command in sequence
       0x2000: 0 for single ADC, 1 for dual conversion
       0x0020: ADC index in single conversion mode
       0x000f: Channel number for ADC 0
       0x0170: Channel number for ADC 1 in dual conversion mode */
    CTU.CLR[0].R = 0x4001;  /* Command 0 - Convert chn1 with ADC 0. */
    CTU.CLR[1].R = 0x000a;  /* Command 1 - Convert chn10 with ADC 0. */
    CTU.CLR[2].R = 0x000f;  /* Command 2 - Convert chn15 with ADC 0. */

    unsigned int idxCmd
               , idxChnAdc0 = 0
               , idxChnAdc1 = 0;
    const unsigned int noCmds = ADC_ADC_0_NO_ACTIVE_CHNS >= ADC_ADC_1_NO_ACTIVE_CHNS
                                ? ADC_ADC_0_NO_ACTIVE_CHNS
                                : ADC_ADC_1_NO_ACTIVE_CHNS
                     , noDualConversions = ADC_ADC_0_NO_ACTIVE_CHNS <= ADC_ADC_1_NO_ACTIVE_CHNS
                                           ? ADC_ADC_0_NO_ACTIVE_CHNS
                                           : ADC_ADC_1_NO_ACTIVE_CHNS
                     , SU = ADC_ADC_0_NO_ACTIVE_CHNS >= ADC_ADC_1_NO_ACTIVE_CHNS? 0: 1;
    _Static_assert(noCmds <= 15, "No more than 15 channels must be configured per ADC unit");
    for(idxCmd=0; idxCmd<noCmds; ++idxCmd)
    {
        /* The warning "-Wtype-limits" is locally switched off. We intentionally permit
           arrays of size zero (no channel configured for single ADC) but this leads to
           never visited code paths. */
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wtype-limits"

        uint16_t CLR = 0;
        if(idxCmd < noDualConversions)
        {
            /* We begin the command list with dual mode conversions. Both ADC are
               simultaneously converting a channel. */

            assert(idxChnAdc0 < sizeOfAry(adc_adc0_idxEnabledChannelAry)
                   &&  idxChnAdc1 < sizeOfAry(adc_adc1_idxEnabledChannelAry)
                  );
            const unsigned int chnAdc0 = adc_adc0_idxEnabledChannelAry[idxChnAdc0++]
                             , chnAdc1 = adc_adc1_idxEnabledChannelAry[idxChnAdc1++];

            /* Set CMS to dual conversion. */
            CLR |= 0x2000;

            /* Set CH_B, the channel index of ADC_1. */
            CLR |= chnAdc1 << 5;

            /* Set CH_A, the channel index of ADC_0. */
            CLR |= chnAdc0;
        }
        else
        {
            /* One ADC has more active channels than the other. The command list ends with
               a number of single mode conversion commands. */

            unsigned int chnAdc;
            if(SU == 0)
            {
                assert(idxChnAdc0 < sizeOfAry(adc_adc0_idxEnabledChannelAry));
                chnAdc = adc_adc0_idxEnabledChannelAry[idxChnAdc0++];
            }
            else
            {
                assert(idxChnAdc1 < sizeOfAry(adc_adc1_idxEnabledChannelAry));
                chnAdc = adc_adc1_idxEnabledChannelAry[idxChnAdc1++];
            }

            /* ST is set to 0 as it needs to be. */

            /* Set SU, the selection of the ADC unit. */
            CLR |= SU << 5;

            /* Set CH, the channel index of the selected ADC unit. */
            CLR |= chnAdc;

        } /* End if(Is there a conversion still for both ADC units?) */

        #pragma GCC diagnostic pop

        /* The field CIR is not used. The related interrupt is raised on successful
           submission of the command but not after the end of the demanded conversion -
           this interrupt must not be used as an end-of-conversion notification. */

        /* Set FC, the indication of the first command in a sequence. */
        if(idxCmd == 0)
            CLR |= 0x4000;

        /* The field FIFO doesn't care, we fetch the results by ISR from the ADCs
           themselves. We leave it at zero. */

        /* CLR now holds the next command. Write it into the CTU. */
        CTU.CLR[idxCmd].R = CLR;

    } /* for(All conversions required to fulfill the user configuration demands) */

    /* Finalize the list of commands by another, no more executed command with bit FC
       (first bit) set.
         Note, the reference manual is unclear about the use of this bit. The text in
       section 13.5.1 says the bit means whether the command is the first one or not, while
       the register description says the bit marks the last command. However, the register
       description also says that the bit is sometimes referred to as FC instead of LC;
       this may support the "first bit" interpretation. By try and error we found that the
       first bit interpretation is the correct one and so we require a dummy command, which
       is the first command of a subsequent (but never used) sequence. */
    CTU.CLR[idxCmd].R = 0x4000;

} /* End of compileAdcCommandList */



/**
 * Initialization of the CTU. The clock source is connected to the following CTU "program"
 * and triggers its execution regularly:\n
 *   Both ADCs are commanded to do m conversions in parallel, where
 * m=min(#ADC_ADC_0_NO_ACTIVE_CHNS, #ADC_ADC_1_NO_ACTIVE_CHNS), then the last M-m
 * conversions are done by the ADC, which has more active channels, where
 * M=max(#ADC_ADC_0_NO_ACTIVE_CHNS, #ADC_ADC_1_NO_ACTIVE_CHNS).\n
 *   An upper bounds for the duration of the M consecutive conversions is computed and an
 * interrupt is programmed after this time span. Although timer controlled has this
 * interrupt the meaning of an all-conversions-compete notification.
 *   @remark
 * The configuration of the CTU is difficult because there is no explicit start and stop of
 * the core counter. The counter is immediately running and interrupts can occur even
 * before we see the first true trigger (i.e. master reload) of a cycle. Therefore, the
 * configuration does not yet enable the External Interrupts of the CTU. This is done
 * later, when the cyclic processing is explicitly started under control of the
 * application. See adc_startConversions().
 *   @remark
 * The computation of the duration of the sequence of conversions requires the knowledge
 * of the duration of a single conversion. This knowldge is hardcoded in this function but
 * it depends on the configuration of the ADC in initADC(). Other timing settings of the
 * ADC will require maintenance of this function
 */
static void initCTU(void)
{
    /* Enable input */
    CTU.TGSISR.R = 0;
    if(&ETIMER == &ETIMER_0)
    {
        CTU.TGSISR.B.I13_RE = 1; /* The CTU master reload is triggered by each period of the
                                    eTimer_0 generated clock signal. */
    }
    else
        CTU.TGSISR.B.I14_RE = 1; /* Same, but eTimer_1. */

    /* Control register:
         0x0001: 0 for triggered mode
         0x00c0: Prescaler: We use the undevided peripheral clock rate of 120MHz
         0x0100: Enable toggle mode for external trigger output */
    CTU.TGSCR.R = 0x0100;

    /* Compare registers. The first one initiates the sequence of ADC conversions. A second
       one is used after execution of all of these conversions in order to signal
       conversion complete. The delay between these two depends on the number of
       conversions. It must not be too little (ISR occurs while last conversion is not yet
       ready) and not too long (ISR needs to have all data fetched prior to start over with
       the next cycle). A rough estimation as implemented here is fine as long as we don't
       have a very high sampling rate, there's far enough marging to stay on the safe side.
       For ADC_T_CYCLE_IN_US being close to the raw conversion time the code here and the
       concept behind will likely fail.\n
         The timing settings of the ADC (see initADC()) yield a conversion time of about 2us
       per channel, rather less. */

    /* First compare register: Start the sequence of conversions immediately. */
    CTU.TCR[0].R = 0x0001;

    /* Here, we give an upper bounds of the complete ADC operation of one cycle,
       including a margin. Note, this is an estimation only but the true time is constant
       (no timing variability from cycle to cycle) and rather below; our margin can be
       small.
         Note, the computation is based on an hard-coded upper bounds for the single
       conversion. This code requires maintenance when the ADC timing is changed in
       initADC(). */
    #define T_CONV_CYCLE_REGVAL(type)                                                       \
                (type)(0.5 + (2e-6*ADC_NO_CONVERSIONS_PER_CYCLE /* approx conversion time */\
                              + 2e-6  /* margin */                                          \
                             )                                                              \
                             * (float)PERIPHERAL_CLOCK_RATE                                 \
                      )
    _Static_assert( T_CONV_CYCLE_REGVAL(int64_t) > 0  &&  T_CONV_CYCLE_REGVAL(int64_t) < 0xffff
                  , "Internal error, conversion time out of range"
                  );
    _Static_assert( T_CONV_CYCLE_REGVAL(int64_t)
                    + (int64_t)(5e-6 * (float)PERIPHERAL_CLOCK_RATE) /* data fetch in ISR */
                    < (int64_t)(ADC_T_CYCLE_IN_US*1e-6 * (float)PERIPHERAL_CLOCK_RATE)
                  , "ADC_T_CYCLE_IN_US is chosen too little for the configured number of ADC"
                    " channels"
                  );

    /* Second compare register for raising conversion complete IRQ when all ADC is done. */
    CTU.TCR[1].R = T_CONV_CYCLE_REGVAL(uint16_t);

    /* Counter: We let it count fom zero to the implementation maximum. It doesn't matter,
       if the conversion cycle is shorter than counting till the end. */
    CTU.TGSCCR.R = 0xffff;
    CTU.TGSCRR.R = 0x0000;

    /* Enable triggers: The trigger from the first compare register starts the ADC command
       sequence. The second trigger starts nothing but is only used as interrupt source.
         However, if the external trigger output of the CTU is enabled for debugging then
       both triggers additionally set this output. Since we configure the toggle mode the
       pulse width of the periodic signal is identical with the complete conversion time. */
    CTU.THCR1.B.T0_E = 1;    /* Enable trigger 0 */
    CTU.THCR1.B.T0_ADCE = 1; /* and let it start the ADC command sequence. */
    CTU.THCR1.B.T1_E = 1;    /* Enable trigger 1 */
#if ENABLE_OUTPUT_OF_CTU_TRIGGER == 1  &&  defined(DEBUG)
    CTU.THCR1.B.T0_ETE = 1;  /* Set external trigger output to 1 on start of conversion */
    CTU.THCR1.B.T1_ETE = 1;  /* Reset external trigger output to 0 on end-of-conversion IRQ */

    /* The use of the external ouput requires pin configuration. */
    SIU.PCR[46].R = 0x0A04; /* Port C[14] configured as external trigger output of the CTU. */
#endif

    /* All conversions are started by the CTU according to the ADC command list it stores
       in its register file CLR[]. The command list is filled based on the user
       configuration. */
    compileAdcCommandList();

    /* COTR: The length of the impulses in the external trigger output in clock ticks
       impacts their visibility on the scope. Irrelevant for us since we use the toggle
       mode. */
    CTU.COTR.R = 0; 

    /* Clear all error and interrupt flag bits, if any. */
    CTU.CTUIFR.R = 0x0fff;
    CTU.CTUEFR.R = 0x1fff;

} /* End of initCTU */



/**
 * Initialization of the ADC device.
 *   @param idxAdc
 * One of the two ADC devices ADC_1 or ADC_1 is passed in by index for initialization.
 */
static void initADC(unsigned int idxAdc)
{
    assert(idxAdc <= 1);
    volatile ADC_tag * const pADC = idxAdc == 0? &ADC_0: &ADC_1;
    
    /* Normally, after reset we expect to be in power down mode. To be safe, e.g. after a
       warm start, we wait for it. */
    pADC->MCR.B.PWDN = 1;
    while(pADC->MSR.B.ADCSTATUS != 1)
        ;

    /* Overwrite conversion result if we are too slow to fetch the previous one and report
       this situation. */
    pADC->MCR.B.OWREN = 1;

    /* Have left aligned result: This makes code more portable, scaling becomes independent
       of actual ADC resolution. */
    pADC->MCR.B.WLSIDE = 1;

    /* Use the single shot mode. */
    pADC->MCR.B.MODE = 0;

    /* Reference voltage selection: On the TRK-USB board both reference voltage inputs are
       connected to the 3.3V supply. We cannot choose for the 5V reference.
         Note, while documented in the reference manual of the MCU (section 8.3.2.1, p.143)
       is the bit not defined in the MCU header file and nor shown by the debugger. We
       can't access it by name. The reset value 0 is what we need and we don't insist on
       the access. */
    //pADC->MCR.B.REF_RANGE_EXP = (idxAdc == 0? ADC_ADC_0_REF_VOLTAGE: ADC_ADC_1_REF_VOLTAGE)
    //                            <= (3.6 + 4.5)/2.0
    //                            ? 0
    //                            : 1;

    /* Enable the cross trigger unit (CTU) for timing control. */
    pADC->MCR.B.CTUEN = 1;

    /* Use the slower out of two possible clock rates. Maximum speed of conversion is not a
       matter for this driver. Slower speed will generally be better for accuracy. */
    pADC->MCR.B.ADCLKSEL = 0;

    /* No interrupts are enabled. The end of the conversions is signalled by the CTU. */
    pADC->IMR.R = 0;
    pADC->CIMR0.R = 0;

    /* Don't use DMA. */
    pADC->DMAE.B.DMAEN = 0;

    /* Timing of conversion. We use a slow timing for this driver.
         Sample time is (INSAMP-1)/60e6 [s].
         OFFSHIFT: Symmetric rounding.
         Note, any change of the timing requires consistent maintenance on the other
       function initCTU(). See there for details. */
    pADC->CTR0.B.INPLATCH = 1;
    pADC->CTR0.B.OFFSHIFT = 1;
    pADC->CTR0.B.INPCMP = 0;
    pADC->CTR0.B.INSAMP = 61 /* 1us */;

    /* Channels 10 and 15 use a separate register. Here, bit 0 of INSAMP has an impact on
       the timing but holds the value of TSENSOR_SEL at the same time. This makes the bit
       effectively unusable if the temperature sensor is read out.
         Caution, the modelling of the doubly used bit is wrong in the MCU header: The
       header defines only seven bits of INPSAMP as "INSAMP" while the 8th bit is
       separately defined as TSENSOR_SEL. The value of INPSAMP will be too large by factor
       2 if naively set through "INSAMP".
         Note, OFFSHIFT is not available for channels 10 and 15. */
    pADC->CTR1.B.INPLATCH = 1;
    pADC->CTR1.B.INPCMP = 0;
    pADC->CTR1.B.INSAMP = 61 /* 1us */ / 2;
#if ADC_USE_ADC_0_CHANNEL_15 == 1  ||  ADC_USE_ADC_1_CHANNEL_15 == 1
    _TSENSOR_SEL = 0;
    pADC->CTR1.B.TSENSOR_SEL = _TSENSOR_SEL;
#else
    pADC->CTR1.B.TSENSOR_SEL = 0;
#endif

    /* Selection of channels, pADC->NCMR0.R: Out of scope, the channel selection is
       commanded by the CTU and there configured. */

    /* Leave power down mode, goto operation. */
    pADC->MCR.B.PWDN = 0;
    while(pADC->MSR.B.ADCSTATUS != 0)
        ;

} /* End of initADC */



/// @todo doc
#if ADC_ENABLE_INTERRUPT_AS_KERNEL_RELEVANT != 0
static bool isrCtuAllConversionsDone(int_cmdContextSwitch_t *pCmdContextSwitch)
#else
static void isrCtuAllConversionsDone(void)
#endif
{
    /* Check status of the ADCs. We expect the bit CTUSTART to be set but most other bits
       unset, indicating the idle/ready state.
         Note, we maintain a single success status for all ADCs and all channels together,
       although we have ADC specific and even channel specific information. We could
       provide success information on a per channel base. Not doing so is justified by the
       very low likelyhood of ever seeing a problem. In this very rare case we are still on
       the safe side - we don't use a bad result but only loose some maybe still alright
       results. */
       
    bool success =
#if ADC_ADC_0_NO_ACTIVE_CHNS > 0
        (((ADC_0.MSR.R) ^ 0x10000u) & 0x019d0027u) == 0
#endif
#if ADC_ADC_0_NO_ACTIVE_CHNS > 0  &&  ADC_ADC_1_NO_ACTIVE_CHNS > 0
        &&
#endif
#if ADC_ADC_1_NO_ACTIVE_CHNS > 0
        (((ADC_1.MSR.R) ^ 0x10000u) & 0x019d0027u) == 0
#endif
        ;

    if(success)
    {
        _Static_assert( sizeOfAry(_conversionResAry) == sizeOfAry(adc_pCDRAry)
                      , "Internal implementation error"
                      );
        unsigned int idxChn;
        const volatile uint32_t * const *ppCDR = &adc_pCDRAry[0];
        uint16_t *pConvRes = &_conversionResAry[0];
        for(idxChn=0; idxChn<ADC_NO_ACTIVE_CHNS; ++idxChn)
        {
            const uint32_t CDR = ** ppCDR++;
            if((CDR & 0x000f0000) == 0x000a0000)
                * pConvRes++ = (uint16_t)(CDR & 0x0000ffff);
            else
                success = false;
        }        
        
        if(success)
        {
            /* Channel 10 is hardwired to the internal reference voltage source VREG_1.2V.
               We store the averaged reading of this channel, know the nominal voltage and
               can thus compute a calibration factor. This factor is then applied to all
               the channels of the given ADC.
                 Nominal voltage: The signal name says 1.2 V but we have measured 1.24 V at
               two real devices and literature mostly says around 1.25 V for a bandgap
               reference. Moreover, most purchaseable reference voltage products have a
               nominal voltage of 1.24 V. This value is what we use. */
# if ADC_USE_ADC_0_CHANNEL_10 == 1  ||  ADC_USE_ADC_1_CHANNEL_10 == 1
            _Static_assert( ADC_FILTER_COEF_VREG_1_2V >= 0.0f
                            &&  ADC_FILTER_COEF_VREG_1_2V < 1.0f
                          , "Bad filter constant configured for smoothing VREG_1.2V"
                          );
#endif
# if ADC_USE_ADC_0_CHANNEL_10 == 1
            _adc0_chn10 = _adc0_chn10 * ADC_FILTER_COEF_VREG_1_2V
                          + (1.0f - ADC_FILTER_COEF_VREG_1_2V) / 1.24f
                            * (float)_conversionResAry[adc_adc0_idxChn10];

# endif
# if ADC_USE_ADC_1_CHANNEL_10 == 1
            _adc1_chn10 = _adc1_chn10 * ADC_FILTER_COEF_VREG_1_2V
                          + (1.0f - ADC_FILTER_COEF_VREG_1_2V) / 1.24f
                            * (float)_conversionResAry[adc_adc1_idxChn10];
# endif

            /* The reading of the TSENS channels is averaged with a simple first order low
               pass. */
# if ADC_USE_ADC_0_CHANNEL_15 == 1  ||  ADC_USE_ADC_1_CHANNEL_15 == 1
            _Static_assert( ADC_FILTER_COEF_TSENS >= 0.0f  &&  ADC_FILTER_COEF_TSENS < 1.0f
                          , "Bad filter constant configured for smoothing TSENS"
                          );
            assert((_TSENSOR_SEL & ~0x1u) == 0);
#endif
# if ADC_USE_ADC_0_CHANNEL_15 == 1
            _TSENS_0[_TSENSOR_SEL] = _TSENS_0[_TSENSOR_SEL] * ADC_FILTER_COEF_TSENS
                                    + (1.0f - ADC_FILTER_COEF_TSENS)
                                      * (float)_conversionResAry[adc_adc0_idxChn15];
#endif
# if ADC_USE_ADC_1_CHANNEL_15 == 1
            _TSENS_1[_TSENSOR_SEL] = _TSENS_1[_TSENSOR_SEL] * ADC_FILTER_COEF_TSENS
                                    + (1.0f - ADC_FILTER_COEF_TSENS)
                                      * (float)_conversionResAry[adc_adc1_idxChn15];
#endif
# if ADC_USE_ADC_0_CHANNEL_15 == 1  ||  ADC_USE_ADC_1_CHANNEL_15 == 1
            _TSENSOR_SEL = (_TSENSOR_SEL+1) & 0x1;
# endif
# if ADC_USE_ADC_0_CHANNEL_15 == 1
            ADC_0.CTR1.B.TSENSOR_SEL = _TSENSOR_SEL;
# endif
# if ADC_USE_ADC_1_CHANNEL_15 == 1
            ADC_1.CTR1.B.TSENSOR_SEL = _TSENSOR_SEL;
# endif
        } /* End if(Processing of data of channels 10 and 15 possible?) */
    } /* End if(MSR shows idle state of ADC?) */


    /* Diagnosis: Keep track of ever failed conversions (debugging) and provide a measure
       how long it is ago that we could successfully sample all channels. */
    if(success)
        _ageOfConversionResults = 0;
    else
    {
        unsigned int noFailedConversions = _noFailedConversions + 1;
        if(noFailedConversions != 0)
            _noFailedConversions = noFailedConversions;
        
        unsigned short ageOfConversionResults = _ageOfConversionResults + 1;
        if(ageOfConversionResults != 0)
            _ageOfConversionResults = ageOfConversionResults;
    }
    
    /* Clear the interrupt flag to be ready for the next conversion cycle. */
    assert(CTU_0.CTUIFR.B.T1_I == 1);
    CTU_0.CTUIFR.R = 0x4;

#if ADC_ENABLE_INTERRUPT_AS_KERNEL_RELEVANT != 0
    /* For kernel relevant interrupt configuration the notification is connected to the
       decision whether to continue the preempted context or to suspend it and to resume
       another one. */
    return _cbEndOfConversion(pCmdContextSwitch);
#else
    /* Call the user's notification function - if any. */
    if(_cbEndOfConversion != NULL)
        (*_cbEndOfConversion)();
#endif
} /* End of isrCtuAllConversionsDone */



/**
 * Initialization of ADC driver. This function needs to be called before use of any of the
 * other functions offered by this driver.\n
 *   Note, most settings of the driver are either hardcoded (e.g. conversion timing) or
 * made by compile-time configuration switches (preprocessor macros). Particularly, the set
 * of channels to convert are made in the latter way. It is not possible to select specific
 * ADC channels at run-time.\n
 *   The configuration and initialization only relates to ADC channels but not to MCU pins.
 * The routing of signals from external accessible MCU pins to the ADC channel inputs needs
 * to be one by the client code (see SIUL programming in the MCU reference manual),
 * otherwise the ADC readings will stay arbitrary with the few exceptions of converting
 * internal signals.\n
 *   Note, SIUL pin configuration is done by this function for the one or two output
 * signals, which can be routet to MCU pins for development support and debugging purpose
 * (and in DEBUG compilation only). See #ENABLE_OUTPUT_OF_ETIMER_CHN2 and
 * #ENABLE_OUTPUT_OF_CTU_TRIGGER for more.
 *   @param priorityOfIRQ
 * The priority of the end-of-conversion interrupt. The range is 1..15, it is a priority as
 * handled by the INTC (see MCU reference manual, section 28, p. 911).\n
 *   Note, this priority has direct impact on your callback \a cbEndOfConversion: It is run
 * from the same interrupt context.\n
 *   Note, this parameter doesn't belong to the function signature if the end-of-conversion
 * interrupt is configured a kernel interrupt (see #ADC_ENABLE_INTERRUPT_AS_KERNEL_RELEVANT).
 *   @param cbEndOfConversion
 * A callback can be passed to the driver which is invoked from the context of the
 * end-of-conversion interrupt every time a new set of conversion result has been read from
 * the ADCs. Pass NULL if not required.\n
 *   Note, the type of this parameter depends on whether the end-of-conversion
 * interrupt is configured a kernel interrupt (see #ADC_ENABLE_INTERRUPT_AS_KERNEL_RELEVANT) or
 * not. NULL is useless for kernel interrupts and caught by assertion.
 *   @remark
 * This function must be called once and only once at system startup time. Reconfiguration
 * of the driver is not supported.
 *   @remark
 * If channel 15 of one or both ADCs is enabled for conversion, i.e. if the internal chip
 * temperature sensors are configured for use, then the driver depends on the availability
 * of the test and calibration data and the initialization of module \a mcuTestAndCalData
 * needs to be done prior to the initialization of the ADC driver. See
 * tac_initTestAndCalibrationDataAry() for details.
 */
#if ADC_ENABLE_INTERRUPT_AS_KERNEL_RELEVANT == 0
void adc_initDriver(unsigned int priorityOfIRQ, void (*cbEndOfConversion)(void))
#else
void adc_initDriver(int_ivor4KernelIsr_t cbEndOfConversion)
#endif
{
    _Static_assert( ADC_ADC_0_REF_VOLTAGE >= 3.0f  &&  ADC_ADC_0_REF_VOLTAGE <= 5.5f
                    &&  ADC_ADC_1_REF_VOLTAGE >= 3.0f  &&  ADC_ADC_1_REF_VOLTAGE <= 5.5f
                  , "Reference voltage out of range"
                  );
    _Static_assert( adc_adc0_noActiveChns == ADC_ADC_0_NO_ACTIVE_CHNS
                    &&  adc_adc1_noActiveChns == ADC_ADC_1_NO_ACTIVE_CHNS
                  , "Internal error, inconsistencies in configuration data"
                  );
    assert(_cbEndOfConversion == NULL);
    _cbEndOfConversion = cbEndOfConversion;
#if ADC_ENABLE_INTERRUPT_AS_KERNEL_RELEVANT != 0
    assert(_cbEndOfConversion != NULL);
#endif

    initETimer();
    initCTU();
#if ADC_ADC_0_NO_ACTIVE_CHNS > 0
    initADC(/* idxAdc */ 0);
#endif
#if ADC_ADC_1_NO_ACTIVE_CHNS > 0
    initADC(/* idxAdc */ 1);
#endif

    /* The CTU is programmed to raise an interrupt on the last ADC command completed.
       Install service routine. */
#if ADC_ENABLE_INTERRUPT_AS_KERNEL_RELEVANT == 0
    assert(priorityOfIRQ > 0  &&  priorityOfIRQ <= 15);
#endif
    ihw_installINTCInterruptHandler(
#if ADC_ENABLE_FOR_BUILD_WITH_KERNEL_BUILDER == 0
                                     isrCtuAllConversionsDone
#else
                                     (int_externalInterruptHandler_t)
#if ADC_ENABLE_INTERRUPT_AS_KERNEL_RELEVANT == 0
                                     {.simpleIsr = isrCtuAllConversionsDone}
#else
                                     {.kernelIsr = isrCtuAllConversionsDone}
# endif
#endif
                                   , /* vectorNum */ 195 /* Trigger 1 */
#if ADC_ENABLE_INTERRUPT_AS_KERNEL_RELEVANT == 0
                                   , /* psrPriority */ priorityOfIRQ
#else
                                   , /* psrPriority */ 1
#endif
                                   , /* isPreemptable */ true
#if ADC_ENABLE_FOR_BUILD_WITH_KERNEL_BUILDER != 0
                                   , /* isKernelInterrupt */
                                     ADC_ENABLE_INTERRUPT_AS_KERNEL_RELEVANT != 0? true: false
#endif
                                   );

    /* Prepare the constants for the computation of the chip temperature. */
#if ADC_USE_ADC_0_CHANNEL_15 == 1
    /* Initialization of module mcuTestAndCalData done? */
    assert(tac_mcuTestAndCalibrationDataAry[0] != 0
           &&  tac_mcuTestAndCalibrationDataAry[1] != 0
          );
    TSENS_0_P1 = (signed long)(tac_mcuTestAndCalibrationDataAry[4] & 0x0fff);
    TSENS_0_P2 = (signed long)(tac_mcuTestAndCalibrationDataAry[0] & 0x0fff);
    TSENS_0_C1 = (signed long)(tac_mcuTestAndCalibrationDataAry[5] & 0x0fff);
    TSENS_0_C2 = (signed long)(tac_mcuTestAndCalibrationDataAry[1] & 0x0fff);
#endif
#if ADC_USE_ADC_1_CHANNEL_15 == 1
    /* Initialization of module mcuTestAndCalData done? */
    assert(tac_mcuTestAndCalibrationDataAry[2] != 0
           &&  tac_mcuTestAndCalibrationDataAry[3] != 0
          );
    TSENS_1_P1 = (signed long)(tac_mcuTestAndCalibrationDataAry[6] & 0x0fff);
    TSENS_1_P2 = (signed long)(tac_mcuTestAndCalibrationDataAry[2] & 0x0fff);
    TSENS_1_C1 = (signed long)(tac_mcuTestAndCalibrationDataAry[7] & 0x0fff);
    TSENS_1_C2 = (signed long)(tac_mcuTestAndCalibrationDataAry[3] & 0x0fff);
#endif
} /* End of adc_initDriver */



/**
 * After driver initialization everything is configured but the main clock, the eTimer
 * counter, is not running. This function starts the timer and the first conversion is
 * immediately initiated.\n
 *   The intention is to use this function once after all I/O initialization, when all ISRs
 * are registered and immediately after the External Interrupt processing by the CPU is
 * enabled. Regular data processing starts.
 */
void adc_startConversions()
{
     /* Enable general reload and TGS input selection reload. By experience, this sets all
        the trigger interrupt flags and some of the error bits. We have to clear them
        before we can safely enable interrupt handling.
          A memory barrier is applied to make all changes take effect before we enter the
        code to "repair" the unwanted artifacts. */
    CTU.CTUCR.R = 0xff87;
    atomic_thread_fence(memory_order_seq_cst);
    CTU.CTUIFR.R = 0x0fff;
    CTU.CTUEFR.R = 0x1fff;

    /* Enable interrupt on trigger 1 (end of conversions). */
    CTU.CTUIR.B.T1_I = 1;

    /* Start the timer channel we are working with. */
    ETIMER.ENBL.R |= (1<<TIMER_CHN);

} /* End of adc_startConversions */




/** 
 * The channel result of a conversion cycle is updated only if no error has been detected
 * for that channel in the given cycle. If at least one channel result from a cycle has
 * not been updated due to a recognized problem then the age of the conversion results is
 * incremented. As long as no conversion problem appears, the returned age is always zero
 * and all channel results are up-to-date. A non zero value indicates how long it is ago
 * that a fully correct conversion cycle has been completed. The unit of this time is the
 * period of the regular conversion cycles, see #ADC_T_CYCLE_IN_US.
 *   Note, because a single age is used to all channels of the conversion cycle there may
 * be single results in the cycle, which are actually more recent as the returned age says.
 *   The use of a common age for all enabled ADC channels has been decided because of the
 * very low likelihood of a recognized conversion error.
 *   @return
 * Get the age of the conversion results in the unit #ADC_T_CYCLE_IN_US. Should normally
 * zero if no problem is apparent.\n
 *   The value is saturated at its implementation maximum. The same vale is returned after
 * driver initialization and before the first conversion result is available.
 *   @remark
 * Coherent reading of ADC channel result(s) and the age of the conversion result is
 * subject to the design of the client code. If coherency is an issue then it needs to
 * implement a critical section, which contains the retrieval of all channel results and
 * their (common) age or it can use the end-of-conversion notification callback to read the
 * required results race condition free in sync with the conversion cycle.
 */
unsigned short adc_getChannelAge()
{
    return _ageOfConversionResults;

} /* End of adc_getChannelAge */



/**
 * Get the last recent uncalibrated conversion result for a single channel. (See
 * adc_getChannelVoltage() and adc_getChannelVoltageAbdAge() for getting calibrated
 * results.)
 *   @return
 * Get the conversion result in as ADC counts as read from the ADC register. The scaling is
 * lienar, zero means zero and 0x10000 means input voltage is same as reference voltage
 * supplied to the MCU. (3.3 V in case of the TRK-UBS-MPC5643L.)
 *   @param idxChn
 * The index of the channel to be read. Note, this index doesn't relate to the sixteen ADC
 * channels available in hardware but to the set of user configured channels. A
 * configuration dependent enumeration is offered for that.
 *   @remark
 * Coherent reading of several ADC channels or of a channel and the age of the conversion
 * result is subject to the design of the client code. If the conversion results, that are
 * fetched for more than one channel, need to be acquired in one and the same conversion
 * cycle then the client code can implement a critical section, which contains the
 * necessary number of calls of this function or it can use the end-of-conversion
 * notification callback to read the required results race condition free in sync with the
 * conversion cycle.
 *   @remark
 * Using this function for channel 15 of either ADC_0 or ADC_1 is rather useless. This
 * channel is alternatingly connected to two (internal) voltage sources for measuring the
 * internal chip temperature. If using this function it is undefined which one of both is
 * fetched. Only use adc_getTsens0() or adc_getTsens1() to directly read the temperature.
 */
uint16_t adc_getChannelRawValue(adc_idxEnabledChannel_t idxChn)
{
    assert((unsigned)idxChn < sizeOfAry(_conversionResAry));
    return _conversionResAry[(unsigned)idxChn];

} /* End of adc_getChannelRawValue */



/**
 * Get the last recent conversion result for a single channel.
 *   @return
 * Get the conversion result in Volt. The calibration of the reading of an ADC counter
 * register can be based either directly on the nominal reference voltage
 * #ADC_ADC_0_REF_VOLTAGE or #ADC_ADC_1_REF_VOLTAGE or on the filtered readings of the
 * internal reference voltage source VREG_1.2V. Which one applies depends on
 * #ADC_USE_ADC_0_CHANNEL_10 and #ADC_USE_ADC_1_CHANNEL_10.\n
 *   See adc_getChannelRawValue() for getting uncalibrated raw ADC counts as conversion
 * result.
 *   @param idxChn
 * The index of the channel to be read. Note, this index doesn't relate to the sixteen ADC
 * channels available in hardware but to the set of user configured channels. A
 * configuration dependent enumeration is offered for that.
 *   @remark
 * Coherent reading of several ADC channels is subject to the design of the client code. If
 * the conversion results for more than one channel need to be acquired in one and the same
 * conversion cycle then the client code can implement a critical section, which contains
 * the necessary number of calls of this function or it can use the end-of-conversion
 * notification callback to read the required results race condition free in sync with the
 * conversion cycle.
 *   @remark
 * The coherent reading of several channel values and validity (using adc_getChannelAge()) is
 * subject to the design of the client code. For a single channel the dedicated function
 * adc_getChannelVoltageAndAge() is offered.
 *   @remark
 * Using this function for channel 15 of either ADC_0 or ADC_1 is rather useless. This
 * channel is alternatingly connected to two (internal) voltage sources for measuring the
 * internal chip temperature. If using this function it is undefined which one of both is
 * fetched. Only use adc_getTsens0() or adc_getTsens1() to directly read the temperature.
 */
float adc_getChannelVoltage(adc_idxEnabledChannel_t idxChn)
{
    /* If channel 10 of an ADC is activate by configuration: The reference voltage for all
       channels of this ADC is the averaged and scaled reading of the internal band gap
       reference voltage source VREG_1.2V.
         Otherwise: The reference voltage is the nominal external reference voltage supplied
       to both the ADCs. */
    float R;
    if(idxChn < ADC_ADC_0_NO_ACTIVE_CHNS)
    {
#if ADC_USE_ADC_0_CHANNEL_10 == 1
        R = 1.0f / _adc0_chn10;
#else
        R = ((float)ADC_ADC_0_REF_VOLTAGE / 65536.0f);
#endif
    }
    else
    {
#if ADC_USE_ADC_1_CHANNEL_10 == 1
        R = 1.0f / _adc1_chn10;
#else
        R = ((float)ADC_ADC_1_REF_VOLTAGE / 65536.0f);
#endif
    }
    
    assert((unsigned)idxChn < sizeOfAry(_conversionResAry));
    return R * (float)_conversionResAry[(unsigned)idxChn];
    
} /* End of adc_getChannelVoltage */




/**
 * Get the last recent conversion result for a single channel together with the coherently
 * read validity information.
 *   @return
 * Get the conversion result in Volt. The calibration of the reading of an ADC counter
 * register can be based either directly on the nominal reference voltage
 * #ADC_ADC_0_REF_VOLTAGE or #ADC_ADC_1_REF_VOLTAGE or on the filtered readings of the
 * internal reference voltage source VREG_1.2V. Which one applies depends on
 * #ADC_USE_ADC_0_CHANNEL_10 and #ADC_USE_ADC_1_CHANNEL_10.\n
 *   See adc_getChannelRawValue() for getting uncalibrated raw ADC counts as conversion
 * result.
 *   @param pAge
 * The "age" of the result can be read coherently with the result itself. The unit of the
 * returned value is the duration of one conversion cycle, see #ADC_T_CYCLE_IN_US.\n
 *   If the data fetching task runs synchronously with the conversion cycle then this
 * value would always be zero as long as no conversions fail. Otherwise the value would
 * indicate how many cycles ago the returned conversion result has been acquired.
 *   @param idxChn
 * The index of the channel to be read. Note, this index doesn't relate to the sixteen ADC
 * channels available in hardware but to the set of user configured channels. A
 * configuration dependent enumeration is offered for that.
 *   @remark
 * Coherent reading of several ADC channels is subject to the design of the client code. If
 * the conversion results for more than one channel need to be acquired in one and the same
 * conversion cycle then the client code can implement a critical section, which contains
 * the necessary number of calls of this function or it can use the end-of-conversion
 * notification callback to read the required results race condition free in sync with the
 * conversion cycle.
 *   Note that the validity information * \a pAge is identical for all channel results of
 * one and the same conversion cycle. Therefore and because this function requires a
 * critical section for each invokation, it is more performant to use
 * adc_getChannelVoltage() repeatedly and adc_getChannelAge() once if the client code
 * implements a critical section to read more than one channel coherently.
 *   @remark
 * The coherent reading of value and age requires some implementation of a critical
 * section. This is necessarily code that depends on the environment, which this driver is
 * integrated in. We use the implementation from the startup code of the TRK-USB-MPC5643L
 * project. The integration of this driver is generally possible into other environments
 * but this function will require adaptations.
 *   @remark
 * Using this function for channel 15 of either ADC_0 or ADC_1 is rather useless. This
 * channel is alternatingly connected to two (internal) voltage sources for measuring the
 * internal chip temperature. If using this function it is undefined which one of both is
 * fetched. Only use adc_getTsens0() or adc_getTsens1() to directly read the temperature.
 */
float adc_getChannelVoltageAndAge(unsigned short *pAge, adc_idxEnabledChannel_t idxChn)
{
    assert((unsigned)idxChn < sizeOfAry(_conversionResAry));
    
    uint16_t adcCnt;
    uint32_t msr = ihw_enterCriticalSection();
    {
        adcCnt = _conversionResAry[(unsigned)idxChn];
        *pAge = _ageOfConversionResults;
    }
    ihw_leaveCriticalSection(msr);
    
    /* If channel 10 of an ADC is activate by configuration: The reference voltage for all
       channels of this ADC is the averaged and scaled reading of the internal band gap
       reference voltage source VREG_1.2V.
         Otherwise: The reference voltage is the nominal external reference voltage supplied
       to both the ADCs. */
    float R;
    if(idxChn < ADC_ADC_0_NO_ACTIVE_CHNS)
    {
#if ADC_USE_ADC_0_CHANNEL_10 == 1
        R = 1.0f / _adc0_chn10;
#else
        R = ((float)ADC_ADC_0_REF_VOLTAGE / 65536.0f);
#endif
    }
    else
    {
#if ADC_USE_ADC_1_CHANNEL_10 == 1
        R = 1.0f / _adc1_chn10;
#else
        R = ((float)ADC_ADC_1_REF_VOLTAGE / 65536.0f);
#endif
    }
    
    return R * (float)adcCnt;
    
} /* End of adc_getChannelVoltageAndAge */




#if ADC_USE_ADC_0_CHANNEL_15 == 1
/**
 * Get the current chip temperature TSENS_0.
 *   @return
 * Get the temperature in degrees Celsius.
 */
float adc_getTsens0(void)
{
    /* TSENS temperature calculation. */
    const float A = _TSENS_0[0]*TSENS_0_C2 - TSENS_0_P2*_TSENS_0[1]
              , B = _TSENS_0[1]*TSENS_0_P1 - _TSENS_0[0]*TSENS_0_C1;

    return (float)TSENS_T2 + ((float)(TSENS_T1-TSENS_T2)*A)/(float)(A+B);

} /* End of adc_getTsens0 */
#endif


#if ADC_USE_ADC_1_CHANNEL_15 == 1
/**
 * Get the current chip temperature TSENS_1.
 *   @return
 * Get the temperature in degrees Celsius.
 */
float adc_getTsens1(void)
{
    /* TSENS temperature calculation. */
    const float A = _TSENS_1[0]*TSENS_1_C2 - TSENS_1_P2*_TSENS_1[1]
              , B = _TSENS_1[1]*TSENS_1_P1 - _TSENS_1[0]*TSENS_1_C1;

    return (float)TSENS_T2 + ((float)(TSENS_T1-TSENS_T2)*A)/(float)(A+B);

} /* End of adc_getTsens1 */
#endif


