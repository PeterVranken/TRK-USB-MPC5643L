#ifndef RTOS_INCLUDED
#define RTOS_INCLUDED
/**
 * @file rtos.h
 * Definition of global interface of module rtos.c
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

typedef struct rtos_taskDesc_t
{
    /** The task function pointer. This function is regularly executed under control of the
        RTOS kernel. */
    void (*taskFct)(uintptr_t contextData);
    
    /** The task function is called with some user provided context data. The user can
        install different tasks with the same function and the diffreent invokations can be
        distinguished by the context information. */
    uintptr_t contextData;

    /** The period time of the task activation in ms. The permitted range is 0..2^30-1. 0
        means no regular, timer controlled activation. This task is only enabled for
        software triggered activation (by interrupts or other tasks). */
    unsigned int tiCycleInMs;
    
    /** The first activation of the task in ms after start of kernel. The permitted range
        is 0..2^30-1. */
    unsigned int tiFirstActivationInMs;
    
    /** The priority of the task in the range 1..15. Different tasks can share the same
        priority or have different priorities. The execution of tasks, which share the
        priority will be sequenced when they become due at same time or with overlap. */
    unsigned int priority;
    
} rtos_taskDesc_t;



/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Task registration. */
unsigned int rtos_registerTask(const rtos_taskDesc_t *pTaskDesc);

/** Kernel initialization. */
void rtos_initKernel(void);

/** Software triggered task activation. Can be called from other tasks or interrupts. */
void rtos_activateTask(unsigned int id);

#endif  /* RTOS_INCLUDED */
