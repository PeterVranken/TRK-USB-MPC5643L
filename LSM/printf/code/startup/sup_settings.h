#ifndef SUP_SETTINGS_INCLUDED
#define SUP_SETTINGS_INCLUDED
/**
 * @file sup_settings.h
 * Definition of global symbols, that are used in C and assembler code. Mainly a list of
 * #define's
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

/** Define this macro if the application is compiled with VLE instruction set.
      @remark As of writing (Sep 2017) VLE instruction set is not yet supported by GCC. The
    option must not be enabled. */
//#define SUP_USE_VLE_INSTRUCTION_SET

/** Define this macro if the application is compiled in lock step mode (LSM).
      @remark As of writing (Sep 2017) this macro must be defined as decoupled processor
    mode (DP) is not yet supported by the implementation of the startup code. */
#define SUP_LOCK_STEP_MODE



/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */



#endif  /* SUP_SETTINGS_INCLUDED */
