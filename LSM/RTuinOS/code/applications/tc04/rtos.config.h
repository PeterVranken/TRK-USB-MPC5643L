#ifndef RTOS_CONFIG_INCLUDED
#define RTOS_CONFIG_INCLUDED
/**
 * @file rtos.config.h
 * Switches to define the most relevant compile-time settings of RTuinOS in an application
 * specific way.
 *
 * Copyright (C) 2012-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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

/** Does the task scheduling concept support time slices of limited length for activated
    tasks? If on, the overhead of the scheduler slightly increases.\n
      Select either RTOS_FEATURE_OFF or RTOS_FEATURE_ON. */
#define RTOS_ROUND_ROBIN_MODE_SUPPORTED     RTOS_FEATURE_OFF


/** Number of tasks in the system. Tasks aren't created dynamically. This number of tasks
    will always be existent and alive. Permitted range is 0..127.\n
      A runtime check is not done. The code will crash in case of a bad setting. */
#define RTOS_NO_TASKS    2


/** Number of distinct priorities of tasks. Since several tasks may share the same
    priority, this number is lower or equal to NO_TASKS. Permitted range is 0..NO_TASKS,
    but 1..NO_TASKS if at least one task is defined.\n
      A runtime check is not done. The code will crash in case of a bad setting. */
#define RTOS_NO_PRIO_CLASSES 1


/** Since many tasks will belong to distinct priority classes, the maximum number of tasks
    belonging to the same class will be significantly lower than the number of tasks. This
    setting is used to reduce the required memory size for the statically allocated data
    structures. Set the value as low as possible. Permitted range is min(1, NO_TASKS)..127,
    but a value greater than NO_TASKS is not reasonable.\n
      A runtime check is not done. The code will crash in case of a bad setting. */
#define RTOS_MAX_NO_TASKS_IN_PRIO_CLASS 2


/** The number of events, which behave like semaphores. When posted, they are not
    broadcasted like ordinary events but only posted to the very task, which is the one of
    highest priority that is currently waiting for this event. If no such task exists,
    the semaphore-event is counted in the related semaphore for future requests of the
    semaphore by any task.\n
      Having semaphores in the application increases the overhead of RTuinOS significantly.
    The number should be zero as long as semaphores are not essential to the application.
    In particular, one should not use semaphores where mutexes suffice. Mutexes are a
    sub-set of semaphores; it are semaphores with start value one and they can be
    implemented much more efficient by bit operations.
      The use case of a semaphore pre-determines its initial value. To make it most easy
    and efficient for the application the array of semaphores is declared extern to
    RTuinOS. Please refer to rtos.h for the declaration of \a rtos_semaphoreAry and define
    \b and \b initialize this array in your application code. */
#define RTOS_NO_SEMAPHORE_EVENTS    0


/** The number of events, which behave like mutexes. When posted, they are not broadcasted
    like ordinary events but only posted to the very task, which is the one of highest
    priority that is currently waiting for this event. If no such task exists, the
    mutex-event is saved until the first task requests it.\n
      Having mutexes in the application increases the overhead of RTuinOS. It should be
    zero as long as mutexes are not essential to the application. */
#define RTOS_NO_MUTEX_EVENTS    0


/** The period of the system timer tick is defined here as floating point constant. The
    unit is s. The permitted range is 10us ... 30s. The constant is considered by the
    intialization of the system timer at RTOS initialization time. */
#define RTOS_TICK (1e-3f)


/** Enable the application defined interrupt 0. (Two such interrupts are pre-configured in
    the code and more can be implemented by taking these two as a code template.)\n
      To install an application interrupt, this define is set to #RTOS_FEATURE_ON.\n
      Secondary, you will define #RTOS_ISR_USER_00 in order to specify the interrupt
    source.\n
      Then, you will implement the callback \a rtos_enableIRQUser00(void) which enables the
    interrupt, typically by accessing the interrupt control register of some peripheral.\n
      Now the interrupt is enabled and if it occurs it'll post the event
    #RTOS_EVT_ISR_USER_00. You will probably have a task of high priority which is waiting
    for this event in order to handle the interrupt when it is resumed by the event. */
#define RTOS_USE_APPL_INTERRUPT_00 RTOS_FEATURE_OFF

/// @todo To be discarded
/** The index of the interrupt which is assigned to application interrupt 0.\n
      All possible external interrupt sources are hardwired to the interrupt controller.
    They are indentified by index. The table, which interrupt source (mostly I/O device) is
    connected to the controller at which index can be found in the MCU reference manual,
    section 28.7, table 28-4. */
#define RTOS_ISR_USER_00    -1


/** Enable the application defined interrupt 1. See #RTOS_USE_APPL_INTERRUPT_00 for
    details. */
#define RTOS_USE_APPL_INTERRUPT_01 RTOS_FEATURE_OFF

/// @todo To be discarded
/** The name of the interrupt vector which is assigned to application interrupt 1. See
    #RTOS_ISR_USER_00 for details. */
#define RTOS_ISR_USER_01    -1


/** The default EOL character sequence used in the startup message is overridden. Our
    serial interface doesn't do newline conversion and requires carriage return and
    newline. However, this depends on the terminal software on the host machine, too,
    and might be changed in other environments.
      @todo Find the default EOL, which is compatible with Eclipse' terminal - this should
    be the reference. */
#define RTOS_EOL "\r\n"


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */



#endif  /* RTOS_CONFIG_INCLUDED */
