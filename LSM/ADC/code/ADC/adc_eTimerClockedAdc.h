#ifndef ADC_ETIMERCLOCKEDADC_INCLUDED
#define ADC_ETIMERCLOCKEDADC_INCLUDED
/**
 * @file adc_eTimerClockedAdc.h
 * Definition of global interface of module adc_eTimerClockedAdc.c
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

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>

#include "tac_mcuTestAndCalibrationData.h"


/*
 * Defines
 */

/** Driver configuration: Set each of these values to 1 if the analog channel should be
    sampled and converted. Otherwise set it to 0.\n
      The channels 9 of each ADC must not be enabled, see MCU reference manual, section 8,
    for details.\n
      The channels 10 and 15 of each ADC unit have a side effect if they are enabled.\n
      A band-gap reference voltage source is connected to channel 10 of both ADCs. If this
    channel is enabled for ADC i then the measured and smoothed voltage of the band-gap
    source is used to calibrate the voltage result of all channels of ADC i. Calibration
    means, that the ratio of the nominal voltage of the band-gap source and the smoothed
    reading of channel 10 is used as scaling factor for all channels of ADC i.\n
      If channel 10 is disabled for ADC i then the ratio of nominal reference voltage to
    maximum count of the ADC is used as scaling factor for all channels of ADC i. Which
    mode performs better mainly depends on the quality of the connected, external reference
    voltage.\n
      A chip-internal temperature sensor is connected to channel 15 of both ADCs. If this
    channel is enabled for ADC i then the computation of the temperature from the readings
    of the ADC is compiled and an API is provided, which permits the application code to
    get the chip temperature TSENS_i in degrees centigrade.\n
      Note, the shared channels 11 .. 14 must not be enabled at both ADC units. */
#define ADC_USE_ADC_0_CHANNEL_00     0   /// Enable ADC_0, channel 0
#define ADC_USE_ADC_0_CHANNEL_01     1   /// Enable ADC_0, channel 1
#define ADC_USE_ADC_0_CHANNEL_02     0   /// Enable ADC_0, channel 2
#define ADC_USE_ADC_0_CHANNEL_03     0   /// Enable ADC_0, channel 3
#define ADC_USE_ADC_0_CHANNEL_04     0   /// Enable ADC_0, channel 4
#define ADC_USE_ADC_0_CHANNEL_05     0   /// Enable ADC_0, channel 5
#define ADC_USE_ADC_0_CHANNEL_06     0   /// Enable ADC_0, channel 6
#define ADC_USE_ADC_0_CHANNEL_07     0   /// Enable ADC_0, channel 7
#define ADC_USE_ADC_0_CHANNEL_08     0   /// Enable ADC_0, channel 8
#define ADC_USE_ADC_0_CHANNEL_09     0   /// Enable ADC_0, channel 9: Reserved, never enable it
#define ADC_USE_ADC_0_CHANNEL_10     1   /// Enable ADC_0, channel 10: Enable it for calibration
#define ADC_USE_ADC_0_CHANNEL_11     0   /// Enable ADC_0, channel 11
#define ADC_USE_ADC_0_CHANNEL_12     0   /// Enable ADC_0, channel 12
#define ADC_USE_ADC_0_CHANNEL_13     0   /// Enable ADC_0, channel 13
#define ADC_USE_ADC_0_CHANNEL_14     0   /// Enable ADC_0, channel 14
#define ADC_USE_ADC_0_CHANNEL_15     1   /// Enable ADC_0, channel 15

#define ADC_USE_ADC_1_CHANNEL_00     0   /// Enable ADC_1, channel 0
#define ADC_USE_ADC_1_CHANNEL_01     0   /// Enable ADC_1, channel 1
#define ADC_USE_ADC_1_CHANNEL_02     0   /// Enable ADC_1, channel 2
#define ADC_USE_ADC_1_CHANNEL_03     0   /// Enable ADC_1, channel 3
#define ADC_USE_ADC_1_CHANNEL_04     0   /// Enable ADC_1, channel 4
#define ADC_USE_ADC_1_CHANNEL_05     0   /// Enable ADC_1, channel 5
#define ADC_USE_ADC_1_CHANNEL_06     0   /// Enable ADC_1, channel 6
#define ADC_USE_ADC_1_CHANNEL_07     0   /// Enable ADC_1, channel 7
#define ADC_USE_ADC_1_CHANNEL_08     0   /// Enable ADC_1, channel 8
#define ADC_USE_ADC_1_CHANNEL_09     0   /// Enable ADC_1, channel 9: Reserved, never enable it
#define ADC_USE_ADC_1_CHANNEL_10     1   /// Enable ADC_1, channel 10: Enable it for calibration
#define ADC_USE_ADC_1_CHANNEL_11     1   /// Enable ADC_1, channel 11
#define ADC_USE_ADC_1_CHANNEL_12     1   /// Enable ADC_1, channel 12
#define ADC_USE_ADC_1_CHANNEL_13     1   /// Enable ADC_1, channel 13
#define ADC_USE_ADC_1_CHANNEL_14     1   /// Enable ADC_1, channel 14
#define ADC_USE_ADC_1_CHANNEL_15     1   /// Enable ADC_1, channel 15


/** The cycle time of the ADC conversions. The range is from about 10us to nearly 140ms, the
    resolution varies from 17ns for short period times to about 2us for long period times.
    The value of the macro is an unsigned long literal, unit is Microsecond. */
#define ADC_T_CYCLE_IN_US   (1000ul)

/** The reference voltage of ADC_0. This value is used for proper configuration of ADC_0
    and for the aboslute calibration of the ADC readings in Volt. See
    adc_getChannelVoltage().\n
      Note, the ADC has narrow ranges for valid reference voltages. These are basically
    values around 3.3 and 5 Volt. See MCU manual for more details, section 8.2, p. 139. */
#define ADC_ADC_0_REF_VOLTAGE    (3.3f)

/** The reference voltage of ADC_1. This value is used for proper configuration of ADC_1
    and for the aboslute calibration of the ADC readings in Volt. See
    adc_getChannelVoltage().\n
      Note, the ADC has narrow ranges for valid reference voltages. These are basically
    values around 3.3 and 5 Volt. See MCU manual for more details, section 8.2, p. 139. */
#define ADC_ADC_1_REF_VOLTAGE    (3.3f)

/** The reading of TSENS (if enabled) is smoothed with a low pass filter of first order.
    The filter coefficient is configured here. It needs to stay below one and the closer it
    gets to one the more the ADC readings of TSENS are smoothed. A value of zero is the
    permitted minimum. All smoothing would be turned off and the temperature will be
    computed based on a single pair of ADC readings. */
#define ADC_FILTER_COEF_TSENS       (0.99f)

/** The reading of channel 10, VREG_1.2V, (if enabled) is averaged with a low pass filter
    of first order. The filter coefficient is configured here. It needs to stay below one
    and the closer it gets to one the more the ADC readings of VREG_1.2V are smoothed. A
    value of zero is the permitted minimum. Averaging would be turned off and the
    calibration of the ADC readings would always be based only on the reading of VREG1.2V
    from the same conversion cycle. */
#define ADC_FILTER_COEF_VREG_1_2V   (0.99f)


/* The software is written as portable as reasonably possible. This requires the awareness
   of the C language standard it is compiled with. */
#if defined(__STDC_VERSION__)
# if (__STDC_VERSION__)/100 == 2011
#  define _STDC_VERSION_C11
# elif (__STDC_VERSION__)/100 == 1999
#  define _STDC_VERSION_C99
# endif
#endif


/*
 * Global type definitions
 */

/** The API functions address to an ADC channel by index. However, only the configured
    channels are represented in the SW and the index i relates to the i-th enabled channel.
    Here is an enumeration, which holds all configured indexes in a way that the
    relationship to the physical ADC channels is still apparent. */
typedef enum adc_idxEnabledChannel_t
{
#if ADC_USE_ADC_0_CHANNEL_00 == 1
    adc_adc0_idxChn00,  /// Index of channel 0, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_01 == 1
    adc_adc0_idxChn01,  /// Index of channel 1, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_02 == 1
    adc_adc0_idxChn02,  /// Index of channel 2, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_03 == 1
    adc_adc0_idxChn03,  /// Index of channel 3, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_04 == 1
    adc_adc0_idxChn04,  /// Index of channel 4, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_05 == 1
    adc_adc0_idxChn05,  /// Index of channel 5, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_06 == 1
    adc_adc0_idxChn06,  /// Index of channel 6, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_07 == 1
    adc_adc0_idxChn07,  /// Index of channel 7, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_08 == 1
    adc_adc0_idxChn08,  /// Index of channel 8, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_09 == 1
    adc_adc0_idxChn09,  /// Index of channel 9, factory test, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_10 == 1
    adc_adc0_idxChn10,  /// Index of channel 10, VREG_1.2V, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_11 == 1
    adc_adc0_idxChn11,  /// Index of shared channel 11, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_12 == 1
    adc_adc0_idxChn12,  /// Index of shared channel 12, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_13 == 1
    adc_adc0_idxChn13,  /// Index of shared channel 13, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_14 == 1
    adc_adc0_idxChn14,  /// Index of shared channel 14, ADC_0, in result data structures
#endif
#if ADC_USE_ADC_0_CHANNEL_15 == 1
    adc_adc0_idxChn15,  /// Index of channel 15, TSENS, ADC_0, in result data structures
#endif

    adc_adc0_noActiveChns, /// The number of channels activated in ADC_0
    adc_linkElement0 = adc_adc0_noActiveChns-1, /** Dummy element needed to properly
                                                    continue enumeration with second ADC */
    
#if ADC_USE_ADC_1_CHANNEL_00 == 1
    adc_adc1_idxChn00,  /// Index of channel 0, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_01 == 1
    adc_adc1_idxChn01,  /// Index of channel 1, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_02 == 1
    adc_adc1_idxChn02,  /// Index of channel 2, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_03 == 1
    adc_adc1_idxChn03,  /// Index of channel 3, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_04 == 1
    adc_adc1_idxChn04,  /// Index of channel 4, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_05 == 1
    adc_adc1_idxChn05,  /// Index of channel 5, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_06 == 1
    adc_adc1_idxChn06,  /// Index of channel 6, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_07 == 1
    adc_adc1_idxChn07,  /// Index of channel 7, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_08 == 1
    adc_adc1_idxChn08,  /// Index of channel 8, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_09 == 1
    adc_adc1_idxChn09,  /// Index of channel 9, factory test, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_10 == 1
    adc_adc1_idxChn10,  /// Index of channel 10, VREG_1.2V, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_11 == 1
    adc_adc1_idxChn11,  /// Index of shared channel 11, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_12 == 1
    adc_adc1_idxChn12,  /// Index of shared channel 12, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_13 == 1
    adc_adc1_idxChn13,  /// Index of shared channel 13, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_14 == 1
    adc_adc1_idxChn14,  /// Index of shared channel 14, ADC_1, in result data structures
#endif
#if ADC_USE_ADC_1_CHANNEL_15 == 1
    adc_adc1_idxChn15,  /// Index of channel 15, TSENS, ADC_1, in result data structures
#endif

    adc_linkElement1,   /// Dummy element needed to properly continue enumeration
    adc_adc1_noActiveChns = adc_linkElement1 - adc_adc0_noActiveChns /// The number of channels activated in ADC_1

} adc_idxEnabledChannel_t;


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Initialize module prior to use and prior to enabling the External Interrupts. */
void adc_initDriver(unsigned int priorityOfIRQ, void (*cbEndOfConversion)(void));

/** Start conversions after all I/O is initialized and the External Interrupts are enabled. */
void adc_startConversions(void);

/** Validity of channel results: Get the age of the currently available results. */
unsigned short adc_getChannelAge(void);

/** Get the last recent uncalibrated conversion result for a single channel. */
uint16_t adc_getChannelRawValue(adc_idxEnabledChannel_t idxChn);

/** Get the last recent conversion result for a single channel. */
float adc_getChannelVoltage(adc_idxEnabledChannel_t idxChn);

/** Get the last recent conversion result for a single channel together with its age. */
float adc_getChannelVoltageAndAge(unsigned short *pAge, adc_idxEnabledChannel_t idxChn);

#if ADC_USE_ADC_0_CHANNEL_15 == 1
/** Get the current chip temperature TSENS_0. */
float adc_getTsens0(void);
#endif

#if ADC_USE_ADC_1_CHANNEL_15 == 1
/** Get the current chip temperature TSENS_1. */
float adc_getTsens1(void);
#endif

#endif  /* ADC_ETIMERCLOCKEDADC_INCLUDED */
