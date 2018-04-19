#ifndef TAC_MCUTESTANDCALIBRATIONDATA_INCLUDED
#define TAC_MCUTESTANDCALIBRATIONDATA_INCLUDED
/**
 * @file tac_mcuTestAndCalibrationData.h
 * Definition of global interface of module tac_mcuTestAndCalibrationData.c
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


/*
 * Defines
 */

/* The software is written as portable as reasonably possible. This requires the awareness
   of the C language standard it is compiled with. */
#if defined(__STDC_VERSION__)
# if (__STDC_VERSION__)/100 == 2011
#  define _STDC_VERSION_C11
# elif (__STDC_VERSION__)/100 == 1999
#  define _STDC_VERSION_C99
# endif
#endif

/** The test and calibration data is organized as an array of 16 Bit words. Here is its
    size. */
#define TAC_NO_TEST_AND_CAL_UINT16_DATA_WORDS   (0x6c/2)


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */

/** This array (implemented as a pointer) grants global read access to the test and
    calibration data of the MCU, the code is running on. I/O drivers and application code
    can read the data through this pointer.\n
      The array is organized in #TAC_NO_TEST_AND_CAL_UINT16_DATA_WORDS 16 Bit words.
      @note The words are stored as 16 Bit but many of them are effectively 12 Bit words.
    In the array all bits are stored as found in ROM. Since the default value of a flash
    ROM bit is one, the unused bits will mostly be ones and the client code may need to
    mask them. */
extern const uint16_t * const tac_mcuTestAndCalibrationDataAry;


/*
 * Global prototypes
 */

/** Module initialization. */
void tac_initTestAndCalibrationDataAry(void);

#endif  /* TAC_MCUTESTANDCALIBRATIONDATA_INCLUDED */
