#ifndef RTOS_SCHEDULER_INCLUDED
#define RTOS_SCHEDULER_INCLUDED
/**
 * @file rtos_scheduler.h
 * Definition of global interface of module rtos_scheduler.c
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

/** System call index of function rtos_triggerEvent(), offered by this module. */
#define RTOS_SYSCALL_TRIGGER_EVENT                      5

/** System call index of function rtos_runTask(), offered by this module. */
#define RTOS_SYSCALL_RUN_TASK                           10


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Creation of an event. The event can be cyclically triggering or software triggerd. */
unsigned int rtos_osCreateEvent( unsigned int tiCycleInMs
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

/** Grant permission to particular processes for using the service rtos_runTask(). */
void rtos_osGrantPermissionRunTask(unsigned int pidOfCallingTask, unsigned int targetPID);

/** Kernel initialization. */
rtos_errorCode_t rtos_osInitKernel(void);

/** Get the current number of failed task activations since start of the RTOS scheduler. */
unsigned int rtos_getNoActivationLoss(unsigned int idTask);


#endif  /* RTOS_SCHEDULER_INCLUDED */
