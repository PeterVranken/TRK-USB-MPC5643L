#ifndef RCC_READCALIBRATIONCONSTANTS_INCLUDED
#define RCC_READCALIBRATIONCONSTANTS_INCLUDED
/**
 * @file rcc_readCalibrationConstants.h
 * Definition of global interface of module rcc_readCalibrationConstants.c
 *
 * Copyright (C) 2017 FEV Europe GmbH (mailto:Peter.Vranken@FEV.de)
 *
 * All rights reserved. Reproduction in whole or in part is prohibited without the written
 * consent of the copyright owner.
 */

/*
 * Include files
 */

#include "typ_types.h"


/*
 * Defines
 */


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/**
 * Copy the MCU's individual test and calibration data into RAM.
 *   @param testDataAry
 * The MCU test and calibration data is placed in this array.
 *   @param noUInt16Words
 * The test and calibration data is organized in words of this size. \a noUInt16Words the
 * number of 16 Bit words to copy into the array. Copying always starts with the first word
 * from the test and calibration data area but the client code doesn't need to copy all of
 * the data.\n
 *   To know, how many words are needed to copy you can refer to the MCU reference manual,
 * section 23.1.8, p. 591. The buildup of the test data area is shown in table 23-24.
 *   @remark
 * This function must be called no more than once per operation cycle of the MCU. The test
 * and calibration data area can be mapped into the CPU's address space exactly once. The
 * behavior of the function is undefined for subsequent calls. You need to read all data at
 * once, which is required by all your application code.
 */
void rcc_readTestData(uint16_t testDataAry[], unsigned int noUInt16Words);

#endif  /* RCC_READCALIBRATIONCONSTANTS_INCLUDED */