/**
 * @file tac_mcuTestAndCalibrationData.c
 *   This module broadcasts the MCUs test and calibration data globally to the rest of the
 * application. The read mechanism for this data from the MCUs factory programmed ROM is
 * implemented in the assembler module rcc_readCalibrationConstants.S. A technical
 * constraint is that the read function must be used only once per power cycle. It is not
 * possible to let every client of the information read the data on demand. Instead, this
 * module uses the read function and stores the fetched information in a global array for
 * everybody else.
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
#include <assert.h>

#include "rcc_readCalibrationConstants.h"
#include "tac_mcuTestAndCalibrationData.h"


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
 
/** The test and calibration of the MCU instance the code is running on. The array is
    filled with the complete test data page at system startup. It can be considered const
    at run time. */
static uint16_t _mcuTestAndCalibrationDataAry[TAC_NO_TEST_AND_CAL_UINT16_DATA_WORDS] =
                                { [0 ... TAC_NO_TEST_AND_CAL_UINT16_DATA_WORDS-1] = 0 };

/** This array (implemented as a pointer) grants global read access to the test and
    calibration data of the MCU, the code is running on. I/O drivers and application code
    can read the data through this pointer.\n
      The referenced array is organized in #TAC_NO_TEST_AND_CAL_UINT16_DATA_WORDS 16 Bit
    words. */
const uint16_t * const tac_mcuTestAndCalibrationDataAry = &_mcuTestAndCalibrationDataAry[0];
    

/*
 * Function implementation
 */

/**
 * Module initialization. Call this function once after power-up and before your
 * application code (and maybe drivers) start up. The global array \a
 * initTestAndCalibrationDataAry is filled with the device specific test and calibration
 * data, which is stored in the ROM at production time.\n
 *   Application and drivers may later access the initTestAndCalibrationDataAry for
 * reading.
 */
void tac_initTestAndCalibrationDataAry(void)
{
    rcc_readTestData( _mcuTestAndCalibrationDataAry
                    , /* noUInt16Words */ sizeOfAry(_mcuTestAndCalibrationDataAry)
                    );
} /* End of initTestAndCalibrationDataAry */
