#ifndef RTOS_PRIORITY_CEILING_PROTOCOL_INCLUDED
#define RTOS_PRIORITY_CEILING_PROTOCOL_INCLUDED
/**
 * @file rtos_priorityCeilingProtocol.h
 * Definition of global interface of module rtos_priorityCeilingProtocol.S
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

#ifdef __STDC_VERSION__
# include <stdint.h>
# include <stdbool.h>
#endif

#include <rtos.config.h>


/*
 * Defines
 */

/** Index of implemented system call for raising a user context's current priority. The
    system call is wrapped in the inline function rtos_suspendAllTasksByPriority().
    Please refer to this function for details. */
#define RTOS_SYSCALL_SUSPEND_ALL_TASKS_BY_PRIORITY      1

/** Index of implemented system call for lowering a user context's current priority. The
    system call is wrapped in the inline function rtos_resumeAllTasksByPriority().
    Please refer to this function for details. */
#define RTOS_SYSCALL_RESUME_ALL_TASKS_BY_PRIORITY       2


/* Define the field offsets and enumeration values in struct eventDesc_t. */
#define PCP_SIZE_OF_EV_DESC         36
#define PCP_O_EVDESC_prio           12
#define PCP_O_EVDESC_state          0
#define PCP_E_EVST_idle             0
#define PCP_E_EVST_triggered        1


/** This is an expression, which needs to be used in a C unit as condition for a static
    assertion. It double-checks the interface between C and assembly code and shapes a kind
    of minimalistic type-safety. See #RTOS_PCP_INTERFACE_CONSTRAINTS_C_AS, too. */
#define RTOS_PCP_INTERFACE_STATIC_CONSTRAINTS_C_AS (                \
            sizeof(rtos_mapPrioToEvent[0]) == 4                     \
            && sizeof(rtos_currentPrio) == 4                        \
            && PCP_SIZE_OF_EV_DESC == sizeof(eventDesc_t)           \
            && PCP_O_EVDESC_state == offsetof(eventDesc_t, state)   \
            && PCP_O_EVDESC_prio == offsetof(eventDesc_t, priority) \
            && sizeoffield(eventDesc_t, state) == 1                 \
            && sizeoffield(eventDesc_t, priority) == 4              \
        )

/** This is an expression, which needs to be used in a C unit as condition for a runtime
    assertion. It double-checks the interface between C and assembly code and shapes a kind
    of minimalistic type-safety. See #RTOS_PCP_INTERFACE_STATIC_CONSTRAINTS_C_AS, too. */
#define RTOS_PCP_INTERFACE_CONSTRAINTS_C_AS (                       \
            true                                                    \
        )

#ifdef __STDC_VERSION__

/// @todo Move this to global location, multiply used
/** Companion of C's offsetof: The size of a field inside a struct. */
#define sizeoffield(type, fieldName) (sizeof(((type*)0)->fieldName))


/*
 * Global type definitions
 */

/** Hidden type, owned by and local to rtos_scheduler.c, but needed here as opaque pointer
    type. */
struct eventDesc_t;


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** PCP: Enter a critical section. Mutual exclusion with other non safety tasks. */
uint32_t rtos_suspendAllTasksByPriority(uint32_t suspendUpToThisPriority);

/** PCP: leave a critical section. */
void rtos_resumeAllTasksByPriority(uint32_t resumeDownToThisPriority);

/**
 * Get the first event in the global event list, which has the given or a smaller
 * priority. All its successors in the list will have either the same or a lower priority
 * as the returned event. All its predecessors will have a higher priority.
 *   @return
 * Get the address of the wanted event.\n
 *   Note, \a priority may be a non existing value and the returned event's priority is
 * lower than \a priority.\n
 *   Note, if all actual events have a priority higher than \a priority then the returned
 * event will be the guard element at the end of the list, which has the impossible low
 * task priority value 0.
 *   @param priority
 * The wanted priority. It needs to be a possbible value in the range
 * 1..#RTOS_MAX_TASK_PRIORITY. It is not necessary that there is at least one event having
 * this priority.
 *   @remark
 * This function must be called only after successful kernel initialization , see
 * rtos_osInitKernel(). The results are undefined otherwise.
 *   @remark
 * The function can be called from assembly and C code and from any context, OS and user
 * tasks and ISRs.
 */
struct eventDesc_t *rtos_getEventByPriority(uint32_t priority);

#endif  /* For C code compilation only */
#endif  /* RTOS_PRIORITY_CEILING_PROTOCOL_INCLUDED */
