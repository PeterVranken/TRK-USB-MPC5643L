#ifndef MAI_MAIN_INCLUDED
#define MAI_MAIN_INCLUDED
/**
 * @file mai_main.h
 * Definition of global interface of module mai_main.c
 *
 * Copyright (C) 2018-2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Include files
 */


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

/** Delay code execution for a number of Milliseconds of world time. */
void delay(unsigned int tiInMs);

/** System time elapsed since startup in Milliseconds. */
unsigned long millis(void);

/** System time elapsed since startup in Microseconds. */
unsigned long micros(void);

/** Flash the LED a number of times to give simple feedback. */
void mai_blink(unsigned int noFlashes);

/** Entry point into C code. */
void main(void);

#endif  /* MAI_MAIN_INCLUDED */
