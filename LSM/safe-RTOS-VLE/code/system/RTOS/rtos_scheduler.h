#ifndef RTOS_SCHEDULER_INCLUDED
#define RTOS_SCHEDULER_INCLUDED
/**
 * @file rtos_scheduler.h
 * Definition of global interface of module rtos_scheduler.c
 *
 * Copyright (C) 2019-2020 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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

#include "MPC5643L.h"
#include "rtos.h"


/*
 * Defines
 */

/** System call index of function rtos_triggerEvent(), offered by this module. */
#define RTOS_SYSCALL_TRIGGER_EVENT                      3


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global inline functions
 */

/**
 * Query the current interrupt priority level.
 *   @return
 * Get the interrupt level in the range 0..15. 1..15 are returned to ISRs running on that
 * level and 0 ist returned if the function is called from an OS task.
 *   @remark
 * This function must be called from the OS context only. User tasks don't have the
 * privileges to call this function.
 */
static inline unsigned int rtos_osGetCurrentInterruptPriority(void)
{
    /* We query the INTC to find out on which interrupt level we are busy. */
    return (unsigned int)INTC.CPR_PRC0.R;

} /* End of rtos_osGetCurrentInterruptPriority */




/*
 * Global prototypes
 */

/** Creation of an event. The event can be cyclically triggering or software triggerd. */
rtos_errorCode_t rtos_osCreateEvent( unsigned int *pEventId
                                   , unsigned int tiCycleInMs
                                   , unsigned int tiFirstActivationInMs
                                   , unsigned int priority
                                   , unsigned int minPIDToTriggerThisEvent
                                   );

/** Task registration for user mode or operating system initialization task. */
rtos_errorCode_t rtos_osRegisterInitTask( int32_t (*initTaskFct)(uint32_t PID)
                                        , unsigned int PID
                                        , unsigned int tiMaxInUs
                                        );

/** Task registration for scheduled user mode tasks. */
rtos_errorCode_t rtos_osRegisterUserTask( unsigned int idEvent
                                        , int32_t (*userModeTaskFct)(uint32_t PID)
                                        , unsigned int PID
                                        , unsigned int tiMaxInUs
                                        );

/** Task registration for scheduled operating system tasks. */
rtos_errorCode_t rtos_osRegisterOSTask(unsigned int idEvent, void (*osTaskFct)(void));

/** Kernel initialization. */
rtos_errorCode_t rtos_osInitKernel(void);

/** Enter critcal section; partially suspend task scheduling. */
uint32_t rtos_osSuspendAllTasksByPriority(uint32_t suspendUpToThisTaskPriority);
 
/** Leave critical section; resume scheduling of tasks. */
void rtos_osResumeAllTasksByPriority(uint32_t resumeDownToThisTaskPriority);

/** Get the current number of failed task activations since start of the RTOS scheduler. */
unsigned int rtos_getNoActivationLoss(unsigned int idTask);

/** A cyclic or event task can query its base priority. */
unsigned int rtos_getTaskBasePriority(void);

/** A task can query the current task scheduling priority. */
unsigned int rtos_getCurrentTaskPriority(void);

#endif  /* RTOS_SCHEDULER_INCLUDED */
