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


/*
 * Defines
 */

/** Interrupt priority of the scheduler. Needed here for assembler implementation of
    priority ceiling protocol.
      @remark The C code contains preprocessor code that ensures consistency with the
    according definition in the C code. */
#define RTOS_PCP_KERNEL_PRIO        12

/** This the highest priority that ISRs and tasks can have, which user code can shape a
    critical section with. If a task or ISR has a higher priority then user code can't
    hinder it from being scheduled at any time and race conditions with these ISRs or tasks
    can't be avoided.\n
      The configured value needs to be at least two less than #RTOS_PCP_KERNEL_PRIO and at
    least one safety supervisory task needs to have a priority between
    #RTOS_PCP_MAX_LOCKABLE_PRIO and RTOS_PCP_KERNEL_PRIO (neither including) - otherwise
    the safety concept is broken. */
#define RTOS_PCP_MAX_LOCKABLE_PRIO  ((RTOS_PCP_KERNEL_PRIO)-2)

/** Index of implemented system call for raising a user context's current priority. The
    system call is wrapped in the inline function rtos_suspendAllInterruptsByPriority().
    Please refer to this function for details. */
#define RTOS_SYSCALL_SUSPEND_ALL_INTERRUPTS_BY_PRIORITY     1

/** Index of implemented system call for lowering a user context's current priority. The
    system call is wrapped in the inline function rtos_resumeAllInterruptsByPriority().
    Please refer to this function for details. */
#define RTOS_SYSCALL_RESUME_ALL_INTERRUPTS_BY_PRIORITY      2


#ifdef __STDC_VERSION__
/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */



#endif  /* For C code compilation only */
#endif  /* RTOS_PRIORITY_CEILING_PROTOCOL_INCLUDED */
