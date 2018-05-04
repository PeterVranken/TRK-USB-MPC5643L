#ifndef TSK_TASKFUNCTION_INCLUDED
#define TSK_TASKFUNCTION_INCLUDED
/**
 * @file tsk_taskFunction.h
 * Definition of global interface of module tsk_taskFunction.c
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


/*
 * Global type definitions
 */

/** Data object exchanged by producer and consumer. */
typedef struct tsk_artifact_t
{
    unsigned int noCycles;
    unsigned int x, y;

} tsk_artifact_t;


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Task A: Is permanently spinning at 100Hz. We can print some time information. */
void tsk_taskA_reportTime(void);

/** Task B: Single shot task, triggered every 2ms. Reports counter value. */
void tsk_taskB(unsigned int idxCycle);

/** Task C: Single shot task, triggered every 7ms. Reports counter value. */
void tsk_taskC(unsigned int idxCycle);

/** Task D, producer: Produce next artifact. */
const tsk_artifact_t *tsk_taskD_produce(unsigned int idxCycle);

/** Task E, consumer: Validate the received artifact. */
signed int tsk_taskE_consume(const tsk_artifact_t *pA);

#endif  /* TSK_TASKFUNCTION_INCLUDED */
