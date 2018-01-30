/**
 * @file adc_eTimerClockedAdc.inc
 * This file implements the static configuration data tables required by the ADC driver.
 * The configuration is made with preprocessor defines and the run-time data structures can
 * be derived from these macros at compile time. Configuration changes at runtime are
 * impossible.\n
 *   The intention is to include this file from the implementation file. In order to not
 * degrade its readability and maintainability, the normal implementation file should not
 * be burdened with the lengthy, hard to overview preprocessor conditions, that are needed
 * to translate the user configuration at compile time into the required data structures.
 *
 * Copyright (C) 2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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

//#include "adc_eTimerClockedAdc.data.h"


/*
 * Defines
 */

/** The total number of ADC channels, which have been configured for sampling and
    conversion. This is the sum of #ADC_ADC_0_NO_ACTIVE_CHNS and #ADC_ADC_1_NO_ACTIVE_CHNS. */
#define ADC_NO_ACTIVE_CHNS          (C015+C115)

/** The number of channels of ADC 0, which have been configured for sampling and conversion. */
#define ADC_ADC_0_NO_ACTIVE_CHNS    C015

/** The number of channels of ADC 1, which have been configured for sampling and conversion. */
#define ADC_ADC_1_NO_ACTIVE_CHNS    C115

/** This is the number of conversions in one cycle; because both ADCs work in parallel it
    is the maximum of enabled channels in boths ADCs. */
#define ADC_NO_CONVERSIONS_PER_CYCLE                                                        \
            ((ADC_ADC_0_NO_ACTIVE_CHNS) > (ADC_ADC_1_NO_ACTIVE_CHNS)                        \
             ? (ADC_ADC_0_NO_ACTIVE_CHNS)                                                   \
             : (ADC_ADC_1_NO_ACTIVE_CHNS)                                                   \
            )

/* The next sequence of defines computes the number of enabled channels at compile time. */
#if ADC_USE_ADC_0_CHANNEL_00 == 1
# define C000 (1)
#else
# define C000 (0)
#endif
#if ADC_USE_ADC_0_CHANNEL_01 == 1
# define C001 (C000+1)
#else
# define C001 C000
#endif
#if ADC_USE_ADC_0_CHANNEL_02 == 1
# define C002 (C001+1)
#else
# define C002 C001
#endif
#if ADC_USE_ADC_0_CHANNEL_03 == 1
# define C003 (C002+1)
#else
# define C003 C002
#endif
#if ADC_USE_ADC_0_CHANNEL_04 == 1
# define C004 (C003+1)
#else
# define C004 C003
#endif
#if ADC_USE_ADC_0_CHANNEL_05 == 1
# define C005 (C004+1)
#else
# define C005 C004
#endif
#if ADC_USE_ADC_0_CHANNEL_06 == 1
# define C006 (C005+1)
#else
# define C006 C005
#endif
#if ADC_USE_ADC_0_CHANNEL_07 == 1
# define C007 (C006+1)
#else
# define C007 C006
#endif
#if ADC_USE_ADC_0_CHANNEL_08 == 1
# define C008 (C007+1)
#else
# define C008 C007
#endif
#if ADC_USE_ADC_0_CHANNEL_09 == 1
# define C009 (C008+1)
#else
# define C009 C008
#endif
#if ADC_USE_ADC_0_CHANNEL_10 == 1
# define C010 (C009+1)
#else
# define C010 C009
#endif
#if ADC_USE_ADC_0_CHANNEL_11 == 1
# define C011 (C010+1)
#else
# define C011 C010
#endif
#if ADC_USE_ADC_0_CHANNEL_12 == 1
# define C012 (C011+1)
#else
# define C012 C011
#endif
#if ADC_USE_ADC_0_CHANNEL_13 == 1
# define C013 (C012+1)
#else
# define C013 C012
#endif
#if ADC_USE_ADC_0_CHANNEL_14 == 1
# define C014 (C013+1)
#else
# define C014 C013
#endif
#if ADC_USE_ADC_0_CHANNEL_15 == 1
# define C015 (C014+1)
#else
# define C015 C014
#endif

#if ADC_USE_ADC_1_CHANNEL_00 == 1
# define C100 (1)
#else
# define C100 (0)
#endif
#if ADC_USE_ADC_1_CHANNEL_01 == 1
# define C101 (C100+1)
#else
# define C101 C100
#endif
#if ADC_USE_ADC_1_CHANNEL_02 == 1
# define C102 (C101+1)
#else
# define C102 C101
#endif
#if ADC_USE_ADC_1_CHANNEL_03 == 1
# define C103 (C102+1)
#else
# define C103 C102
#endif
#if ADC_USE_ADC_1_CHANNEL_04 == 1
# define C104 (C103+1)
#else
# define C104 C103
#endif
#if ADC_USE_ADC_1_CHANNEL_05 == 1
# define C105 (C104+1)
#else
# define C105 C104
#endif
#if ADC_USE_ADC_1_CHANNEL_06 == 1
# define C106 (C105+1)
#else
# define C106 C105
#endif
#if ADC_USE_ADC_1_CHANNEL_07 == 1
# define C107 (C106+1)
#else
# define C107 C106
#endif
#if ADC_USE_ADC_1_CHANNEL_08 == 1
# define C108 (C107+1)
#else
# define C108 C107
#endif
#if ADC_USE_ADC_1_CHANNEL_09 == 1
# define C109 (C108+1)
#else
# define C109 C108
#endif
#if ADC_USE_ADC_1_CHANNEL_10 == 1
# define C110 (C109+1)
#else
# define C110 C109
#endif
#if ADC_USE_ADC_1_CHANNEL_11 == 1
# define C111 (C110+1)
#else
# define C111 C110
#endif
#if ADC_USE_ADC_1_CHANNEL_12 == 1
# define C112 (C111+1)
#else
# define C112 C111
#endif
#if ADC_USE_ADC_1_CHANNEL_13 == 1
# define C113 (C112+1)
#else
# define C113 C112
#endif
#if ADC_USE_ADC_1_CHANNEL_14 == 1
# define C114 (C113+1)
#else
# define C114 C113
#endif
#if ADC_USE_ADC_1_CHANNEL_15 == 1
# define C115 (C114+1)
#else
# define C115 C114
#endif


/** The bit mask for ADC_0 registers CEOCFR0 nad CIMR0 based on the enabled channels. */
#define ADC_ADC_0_CHANNEL_BIT_MASK                      \
    (+ (ADC_USE_ADC_0_CHANNEL_00 == 1? (0x1u <<  0): 0) \
     + (ADC_USE_ADC_0_CHANNEL_01 == 1? (0x1u <<  1): 0) \
     + (ADC_USE_ADC_0_CHANNEL_02 == 1? (0x1u <<  2): 0) \
     + (ADC_USE_ADC_0_CHANNEL_03 == 1? (0x1u <<  3): 0) \
     + (ADC_USE_ADC_0_CHANNEL_04 == 1? (0x1u <<  4): 0) \
     + (ADC_USE_ADC_0_CHANNEL_05 == 1? (0x1u <<  5): 0) \
     + (ADC_USE_ADC_0_CHANNEL_06 == 1? (0x1u <<  6): 0) \
     + (ADC_USE_ADC_0_CHANNEL_07 == 1? (0x1u <<  7): 0) \
     + (ADC_USE_ADC_0_CHANNEL_08 == 1? (0x1u <<  8): 0) \
     + (ADC_USE_ADC_0_CHANNEL_09 == 1? (0x1u <<  9): 0) \
     + (ADC_USE_ADC_0_CHANNEL_10 == 1? (0x1u << 10): 0) \
     + (ADC_USE_ADC_0_CHANNEL_11 == 1? (0x1u << 11): 0) \
     + (ADC_USE_ADC_0_CHANNEL_12 == 1? (0x1u << 12): 0) \
     + (ADC_USE_ADC_0_CHANNEL_13 == 1? (0x1u << 13): 0) \
     + (ADC_USE_ADC_0_CHANNEL_14 == 1? (0x1u << 14): 0) \
     + (ADC_USE_ADC_0_CHANNEL_15 == 1? (0x1u << 15): 0) \
    )

/** The bit mask for ADC_1 registers CEOCFR0 nad CIMR0 based on the enabled channels. */
#define ADC_ADC_1_CHANNEL_BIT_MASK                      \
    (+ (ADC_USE_ADC_1_CHANNEL_00 == 1? (0x1u <<  0): 0) \
     + (ADC_USE_ADC_1_CHANNEL_01 == 1? (0x1u <<  1): 0) \
     + (ADC_USE_ADC_1_CHANNEL_02 == 1? (0x1u <<  2): 0) \
     + (ADC_USE_ADC_1_CHANNEL_03 == 1? (0x1u <<  3): 0) \
     + (ADC_USE_ADC_1_CHANNEL_04 == 1? (0x1u <<  4): 0) \
     + (ADC_USE_ADC_1_CHANNEL_05 == 1? (0x1u <<  5): 0) \
     + (ADC_USE_ADC_1_CHANNEL_06 == 1? (0x1u <<  6): 0) \
     + (ADC_USE_ADC_1_CHANNEL_07 == 1? (0x1u <<  7): 0) \
     + (ADC_USE_ADC_1_CHANNEL_08 == 1? (0x1u <<  8): 0) \
     + (ADC_USE_ADC_1_CHANNEL_09 == 1? (0x1u <<  9): 0) \
     + (ADC_USE_ADC_1_CHANNEL_10 == 1? (0x1u << 10): 0) \
     + (ADC_USE_ADC_1_CHANNEL_11 == 1? (0x1u << 11): 0) \
     + (ADC_USE_ADC_1_CHANNEL_12 == 1? (0x1u << 12): 0) \
     + (ADC_USE_ADC_1_CHANNEL_13 == 1? (0x1u << 13): 0) \
     + (ADC_USE_ADC_1_CHANNEL_14 == 1? (0x1u << 14): 0) \
     + (ADC_USE_ADC_1_CHANNEL_15 == 1? (0x1u << 15): 0) \
    )


/* At least one channel needs to be enabled. Otherwise this driver is useless and should
   not be used at all. */
#if ADC_ADC_0_NO_ACTIVE_CHNS == 0  &&  ADC_ADC_1_NO_ACTIVE_CHNS == 0
# error At least one channel needs to be enabled
#endif

/* Channel 9 must not be enabled. It is reserved to the factory test. */
#if ADC_USE_ADC_0_CHANNEL_09 || ADC_USE_ADC_1_CHANNEL_09
# error ADC_0/1 Channel 9 must not be enabled. It is reserved to the factory test
#endif

/* The shared channels must not be configured at both ADCs. */
#if ADC_USE_ADC_0_CHANNEL_11 && ADC_USE_ADC_1_CHANNEL_11        \
    ||  ADC_USE_ADC_0_CHANNEL_12 && ADC_USE_ADC_1_CHANNEL_12    \
    ||  ADC_USE_ADC_0_CHANNEL_13 && ADC_USE_ADC_1_CHANNEL_13    \
    ||  ADC_USE_ADC_0_CHANNEL_14 && ADC_USE_ADC_1_CHANNEL_14
# error The shared ADC channels 11..14 must not be used simultaneously with both ADCs
#endif


/*
 * Local type definitions
 */


/*
 * Local prototypes
 */


/*
 * Data definitions
 */

/** This array implements the list of indexes of channels activated in ADC_0. */
static const unsigned int adc_adc0_idxEnabledChannelAry[ADC_ADC_0_NO_ACTIVE_CHNS] =
{
#if ADC_USE_ADC_0_CHANNEL_00 == 1
    0,
#endif
#if ADC_USE_ADC_0_CHANNEL_01 == 1
    1,
#endif
#if ADC_USE_ADC_0_CHANNEL_02 == 1
    2,
#endif
#if ADC_USE_ADC_0_CHANNEL_03 == 1
    3,
#endif
#if ADC_USE_ADC_0_CHANNEL_04 == 1
    4,
#endif
#if ADC_USE_ADC_0_CHANNEL_05 == 1
    5,
#endif
#if ADC_USE_ADC_0_CHANNEL_06 == 1
    6,
#endif
#if ADC_USE_ADC_0_CHANNEL_07 == 1
    7,
#endif
#if ADC_USE_ADC_0_CHANNEL_08 == 1
    8,
#endif
#if ADC_USE_ADC_0_CHANNEL_09 == 1
    9,
#endif
#if ADC_USE_ADC_0_CHANNEL_10 == 1
    10,
#endif
#if ADC_USE_ADC_0_CHANNEL_11 == 1
    11,
#endif
#if ADC_USE_ADC_0_CHANNEL_12 == 1
    12,
#endif
#if ADC_USE_ADC_0_CHANNEL_13 == 1
    13,
#endif
#if ADC_USE_ADC_0_CHANNEL_14 == 1
    14,
#endif
#if ADC_USE_ADC_0_CHANNEL_15 == 1
    15,
#endif
};


/** This array implements the list of indexes of channels activated in ADC_1. */
static const unsigned int adc_adc1_idxEnabledChannelAry[ADC_ADC_1_NO_ACTIVE_CHNS] =
{
#if ADC_USE_ADC_1_CHANNEL_00 == 1
    0,
#endif
#if ADC_USE_ADC_1_CHANNEL_01 == 1
    1,
#endif
#if ADC_USE_ADC_1_CHANNEL_02 == 1
    2,
#endif
#if ADC_USE_ADC_1_CHANNEL_03 == 1
    3,
#endif
#if ADC_USE_ADC_1_CHANNEL_04 == 1
    4,
#endif
#if ADC_USE_ADC_1_CHANNEL_05 == 1
    5,
#endif
#if ADC_USE_ADC_1_CHANNEL_06 == 1
    6,
#endif
#if ADC_USE_ADC_1_CHANNEL_07 == 1
    7,
#endif
#if ADC_USE_ADC_1_CHANNEL_08 == 1
    8,
#endif
#if ADC_USE_ADC_1_CHANNEL_09 == 1
    9,
#endif
#if ADC_USE_ADC_1_CHANNEL_10 == 1
    10,
#endif
#if ADC_USE_ADC_1_CHANNEL_11 == 1
    11,
#endif
#if ADC_USE_ADC_1_CHANNEL_12 == 1
    12,
#endif
#if ADC_USE_ADC_1_CHANNEL_13 == 1
    13,
#endif
#if ADC_USE_ADC_1_CHANNEL_14 == 1
    14,
#endif
#if ADC_USE_ADC_1_CHANNEL_15 == 1
    15,
#endif
};


/** Data fetching in the on-conversion-complete ISR is speeded up by this array holding
    the pointers to all data result registers of ADC_0 and ADC_1 to be read. */
static const volatile uint32_t * const adc_pCDRAry[ADC_NO_ACTIVE_CHNS] =
{
#if ADC_USE_ADC_0_CHANNEL_00 == 1
    &ADC_0.CDR[0].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_01 == 1
    &ADC_0.CDR[1].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_02 == 1
    &ADC_0.CDR[2].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_03 == 1
    &ADC_0.CDR[3].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_04 == 1
    &ADC_0.CDR[4].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_05 == 1
    &ADC_0.CDR[5].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_06 == 1
    &ADC_0.CDR[6].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_07 == 1
    &ADC_0.CDR[7].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_08 == 1
    &ADC_0.CDR[8].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_09 == 1
    &ADC_0.CDR[9].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_10 == 1
    &ADC_0.CDR[10].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_11 == 1
    &ADC_0.CDR[11].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_12 == 1
    &ADC_0.CDR[12].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_13 == 1
    &ADC_0.CDR[13].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_14 == 1
    &ADC_0.CDR[14].R,
#endif
#if ADC_USE_ADC_0_CHANNEL_15 == 1
    &ADC_0.CDR[15].R,
#endif

#if ADC_USE_ADC_1_CHANNEL_00 == 1
    &ADC_1.CDR[0].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_01 == 1
    &ADC_1.CDR[1].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_02 == 1
    &ADC_1.CDR[2].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_03 == 1
    &ADC_1.CDR[3].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_04 == 1
    &ADC_1.CDR[4].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_05 == 1
    &ADC_1.CDR[5].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_06 == 1
    &ADC_1.CDR[6].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_07 == 1
    &ADC_1.CDR[7].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_08 == 1
    &ADC_1.CDR[8].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_09 == 1
    &ADC_1.CDR[9].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_10 == 1
    &ADC_1.CDR[10].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_11 == 1
    &ADC_1.CDR[11].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_12 == 1
    &ADC_1.CDR[12].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_13 == 1
    &ADC_1.CDR[13].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_14 == 1
    &ADC_1.CDR[14].R,
#endif
#if ADC_USE_ADC_1_CHANNEL_15 == 1
    &ADC_1.CDR[15].R,
#endif
};

/* To support maintainability of the preprocessor constructs in the file, we let the
   compiler write some preprocessing results into memory for inspection. This data is meant
   only for debugging but not required by the runtime code and we generate it in DEBUG
   compilation only. Open the assembler listing to see the values; the linker won't add the
   variables to the executable and the debugger can't display the values. */
#ifdef DEBUG
/** The total number of ADC channels, which have been configured for sampling and
   conversion. This is the sum of \a adc_adc0_noActiveChn and \a adc_adc1_noActiveChn. */
const unsigned int adc_noActiveChn = ADC_NO_ACTIVE_CHNS;

/** The number of channels of ADC 0, which have been configured for sampling and conversion. */
const unsigned int adc_adc0_noActiveChn = ADC_ADC_0_NO_ACTIVE_CHNS;

/** The number of channels of ADC 1, which have been configured for sampling and conversion. */
const unsigned int adc_adc1_noActiveChn = ADC_ADC_1_NO_ACTIVE_CHNS;

/** This is the number of conversions in one cycle; because both ADCs work in parallel it
    is the maximum of enabled channels in boths ADCs. */
const unsigned int adc_noConversionsPerCycle = ADC_NO_CONVERSIONS_PER_CYCLE;
#endif


/*
 * Function implementation
 */

