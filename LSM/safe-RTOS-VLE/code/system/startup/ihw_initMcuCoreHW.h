#ifndef IHW_INITMCUCOREHW_INCLUDED
#define IHW_INITMCUCOREHW_INCLUDED
/**
 * @file ihw_initMcuCoreHW.h
 * Definition of global interface of module ihw_initMcuCoreHW.c
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
 *   ihw_initMcuCoreHW
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>

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
 * Global inline functions
 */

/*
 * Global prototypes
 */

/** Init core HW of MCU so that it can be safely operated. */
void ihw_initMcuCoreHW(void);

#endif  /* IHW_INITMCUCOREHW_INCLUDED */
