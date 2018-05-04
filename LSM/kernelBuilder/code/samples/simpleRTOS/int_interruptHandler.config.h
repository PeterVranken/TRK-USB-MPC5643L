#ifndef INT_INTERRUPTHANDLER_CONFIG_INCLUDED
#define INT_INTERRUPTHANDLER_CONFIG_INCLUDED
/**
 * @file int_interruptHandler.config.h
 * Compile time configuration settings of module int_interruptHandler
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

/** The support of stack sharing can be turned on or off. The assembly code of the IVOR
    handlers is a bit better performing if it is turned off. In practice, stack sharing
    will only be applied to kernels, which make use of single-shot tasks, otherwise it can
    normally be turned off. */
#define INT_USE_SHARED_STACKS   1

    
/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */



#endif  /* INT_INTERRUPTHANDLER_CONFIG_INCLUDED */
