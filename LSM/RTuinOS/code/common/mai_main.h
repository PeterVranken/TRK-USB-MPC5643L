#ifndef MAI_MAIN_INCLUDED
#define MAI_MAIN_INCLUDED
/**
 * @file mai_main.h
 * Definition of global interface of module mai_main.c
 *
 * Copyright (C) 2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Delay code execution for a number of Milliseconds of world time. */
void delay(unsigned int tiInMs);

/** System time elapsed since startup in Milliseconds. */
unsigned long millis();

/** System time elapsed since startup in Microseconds. */
unsigned long micros();

/** Flash the LED a number of times to give simple feedback. */
void mai_blink(unsigned int noFlashes);

/** Entry point into C code. */
void main(void);

#endif  /* MAI_MAIN_INCLUDED */
