#ifndef TYP_TYPES_INCLUDED
#define TYP_TYPES_INCLUDED
/**
 * @file typ_types.h
 * Definition of global, basic types.
 *
 * Copyright (C) 2013-2017 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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

#ifdef __GNUC__

/** Intentionally unused objects in the C code can be marked using this define. A compiler
    warning is avoided. */
# define ATTRIB_UNUSED __attribute__((unused))

# ifdef NDEBUG
/** Objects in the C code intentionally unused in the production compilation can be marked
    using this define. A compiler warning is avoided. */
#  define ATTRIB_DBG_ONLY ATTRIB_UNUSED
# else
/** Objects in the C code intentionally unused in the production compilation can be marked
    using this define. A compiler warning is avoided. */
#  define ATTRIB_DBG_ONLY
# endif

/** We need function pointers to be exchanged with assember code. Maybe better to
    explicitly disable inlining here. */
# define NO_INLINE __attribute__((noinline))

#else
# define ATTRIB_UNUSED
# define ATTRIB_DBG_ONLY
# define NO_INLINE
#endif

/** The number of elements of a one dimensional array. */
#define sizeOfAry(a)    (sizeof(a)/sizeof(a[0]))


/*
 * Global type definitions
 */

/* Some more basic types are defined using the naming scheme of stdint.h. */

/** Definition of Boolean values.
      @remark The definition of \a boolean needs to become unsigned char if using Windows
    stuff, otherwise the definition is incompatible with the header file windows.h. */
typedef bool boolean_t;

typedef float float32_t;
typedef double float64_t;


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */



#endif  /* TYP_TYPES_INCLUDED */
