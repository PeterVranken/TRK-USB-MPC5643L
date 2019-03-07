#ifndef PRR_PROCESSREPORTING_INCLUDED
#define PRR_PROCESSREPORTING_INCLUDED
/**
 * @file prr_processReporting.h
 * Definition of global interface of module prr_processReporting.c
 *
 * Copyright (C) 2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** User task doing some regular status reporting via serial output. */
int32_t prr_taskReporting(uint32_t PID);

/** User task doing some invisible background testing of context switches. */
int32_t prr_taskTestContextSwitches(uint32_t PID);


#endif  /* PRR_PROCESSREPORTING_INCLUDED */
