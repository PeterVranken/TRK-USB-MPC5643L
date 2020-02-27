/**
 * @file rtos_priorityCeilingProtocol.c
 * Implementation of Priority Ceiling Protocol as system call, such that it becomes
 * available to the user task code.\n
 *   The implementation basically changes the current priority variable of the scheduler.
 * It adds protection code, such that bad use of the system calls by the user task can't be
 * harmful. In particular, it must be avoided that the current priority can be lowered by a
 * task.
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
/* Module interface
 *   rtos_scBscHdlr_suspendAllTasksByPriority
 *   rtos_suspendAllTasksByPriority
 *   rtos_scFlHdlr_resumeAllTasksByPriority
 *   rtos_resumeAllTasksByPriority
 * Local functions
 */

#ifndef __VLE__
# error This file is intended for compilation for VLE instruction set only
#endif

/*
 * Include files
 */

#include "rtos_priorityCeilingProtocol.h"
#include "rtos_ivorHandler.h"
#include "rtos.config.h"


/*
 * Defines
 */


/*
 * External function declarations
 */


/*
 * Data declarations
 */



/*
 * Function implementation
 */

/**
 *   @func rtos_scBscHdlr_suspendAllTasksByPriority
 * Change the current priority of the calling user task context, only upwards and only up
 * to a value of #RTOS_MAX_LOCKABLE_TASK_PRIORITY.\n
 *    Any attempt to change \a suspendUpToThisPriority beyond #RTOS_MAX_LOCKABLE_TASK_PRIORITY
 * will lead to an #RTOS_CAUSE_TASK_ABBORTION_SYS_CALL_BAD_ARG exception.\n
 *    The attempt to lower the current priority is silently ignored.
 *   @return
 * On return, r3 contains the priority level at entry into this function (and into the
 * critical section). This priority level needs to be restored on exit from the critical
 * section, when rtos_scFlHdlr_resumeAllTasksByPriority is called.
 *   @param addressOfHandler
 * On entry, r3 holds the address of this system call handler. Not used.
 *   @param suspendUpToThisPriority
 * On entry, r4 holds the aimed priority level. All tasks up to and including this priority
 * will be locked. The CPU will not handle them until the priority level is lowered again.
 *   @remark
 * This is the implementation of a system call handler. The code must not be invoked
 * directly. The system call handler is compliant with the basic conformance class. It is
 * entirely executed with EE=0 and must not rely on sp, r2 and r13.
 */
    .section    .text.ivor
    .extern     rtos_osSystemCallBadArgument
    .globl      rtos_scBscHdlr_suspendAllTasksByPriority
    .balign     2

rtos_scBscHdlr_suspendAllTasksByPriority:
    
    /* The yielded priority must be within the permitted range: Not at level of safety
       tasks or above and not below the level we had at entry into the task.
         The first condition needs to be checked, we would otherwise break the
       safety-concept.
         The second condition is always fulfilled, since we only change upwards. A check in
       DEBUG compilation is relative expensive and typical client code faults, which lead
       to this situation, are anyway caught by the DEBUG code in
       rtos_scFlHdlr_resumeAllTasksByPriority. So we don't do. */
#if RTOS_MAX_LOCKABLE_TASK_PRIORITY <= 32
    se_cmpli    r4, RTOS_MAX_LOCKABLE_TASK_PRIORITY /* Upper boundary */
#else
    e_cmpl16i   r4, RTOS_MAX_LOCKABLE_TASK_PRIORITY /* Upper boundary */
#endif
    e_bgt       rtos_osSystemCallBadArgument  /* r4>MAX: Abort task, demanded prio too high */
   
    mfspr       r13, SPR_G1_SDA     /* Basic handlers need to repair possibly destroyed SDA
                                       pointers prior to first use. */
    e_lwz       r3, rtos_currentPrio@sda21(r13) /* r3: Current prio value, before change.
                                                   For return */
    se_cmpl     r4, r3
    se_ble      sat_return              /* r4<=r3: No change of current prio */
    e_stw       r4, rtos_currentPrio@sda21(r13) /* Store checked, demanded val in prio var */
sat_return:
    se_rfi

/* End of rtos_scBscHdlr_suspendAllTasksByPriority */


    

/**
 *   @func rtos_suspendAllTasksByPriority
 * System call code for rtos_scBscHdlr_suspendAllTasksByPriority(). Function details
 * can be found there.
 *   @return
 * On return, r3 contains the priority level at entry into this function.
 *   @param suspendUpToThisPriority
 * On entry, r3 holds the aimed priority level.
 *   @remark
 * This code must be executed in user mode only. Any attempt to call from supervisor mode
 * can crash the system. In supervisor code, use rtos_osSuspendAllTasksByPriority()
 * instead.
 */
    .section    .text.rtos_suspendAllTasksByPriority
    .globl      rtos_suspendAllTasksByPriority
    .balign     2
rtos_suspendAllTasksByPriority:
    se_mr       r4, r3
    se_li       r3, RTOS_SYSCALL_SUSPEND_ALL_TASKS_BY_PRIORITY
    se_sc
    se_blr
    
/* End of rtos_suspendAllTasksByPriority */



/**
 *   @func rtos_scFlHdlr_resumeAllTasksByPriority
 * Change the current priority of the calling user task context, only downwards and only down
 * to P0, where P0 is the initial priority of the system calling user task, which had been
 * statically agreed on by task configuration.\n
 *   Any attempt to change the priority beyond P0 will lead to an
 * #RTOS_CAUSE_TASK_ABBORTION_SYS_CALL_BAD_ARG exception.\n
 *   The attempt to raise the current priority is silently ignored.\n
 *   In DEBUG compilation only, it is validated that the aimed priority is not higher than
 * the allowed maximum #RTOS_MAX_LOCKABLE_TASK_PRIORITY. If so, it'll lead to an
 * #RTOS_CAUSE_TASK_ABBORTION_SYS_CALL_BAD_ARG exception. The assertion is meant to find
 * typical client code errors, when a task of too high priority tries getting a resource,
 * while resource sharing with such a task is forbidden (in order to not inhibit a safety
 * supervisory task from being accurately scheduled).
 *   @return
 * On return, r3 contains the priority level at entry into this function. This priority
 * level will mostly be of no particular interest.
 *   @param addressOfHandler
 * On entry, r3 holds the address of this system call handler. Not used.
 *   @param resumeDownToThisPriority
 * On entry, r4 holds the aimed priority level. All tasks at or above this level will be
 * schedulded again. Tasks lower than this priority will still be locked.
 *   @remark
 * This is the implementation of a system call handler. The code must not be invoked
 * directly. The system call handler is compliant with the full conformance class.
 *   @remark
 * The ancestor revision of the RTOS with the hardware scheduler could implement this
 * function as a high performing basic handler, which requires assembly programming.
 * However, the meanwhile used software scheduler requires a handler of full conformance
 * class, which could be implemented in C. There's no strong justification any more to have
 * this function in assembly code.
 */
    .section    .text.ivor
    .extern     rtos_osSystemCallBadArgument
    .globl      rtos_scFlHdlr_resumeAllTasksByPriority
    .balign     2

rtos_scFlHdlr_resumeAllTasksByPriority:

    /* The yielded priority must be within the permitted range: Not at level of safety
       tasks or above and not below the level we had at entry into the task.
         The first condition is always fulfilled, since we only change downwards but we can
       check it in DEBUG compilation to support debugging of the (then faulty) client code,
       which passes a wrong value.
         The second condition needs to be always checked, a violation is possible in case
       of faulty client code and it would be catastrophic. */
#ifdef DEBUG
# if RTOS_MAX_LOCKABLE_TASK_PRIORITY <= 32
    se_cmpli    r4, RTOS_MAX_LOCKABLE_TASK_PRIORITY /* Upper boundary */
# else
    e_cmpl16i   r4, RTOS_MAX_LOCKABLE_TASK_PRIORITY /* Upper boundary */
# endif
    e_bgt       rtos_osSystemCallBadArgument /* r4>MAX: Abort task, demanded prio too high */
#endif
    se_lwz      r3, (IV8_SIZE_OF_SF+RUT_O_CPR)(sp)  /* r3: Prio of task start */
    se_cmpl     r4, r3
    e_blt       rtos_osSystemCallBadArgument    /* r4<r3: Abort task: Demanded prio too low */

    /* The function allows to change the task priority only downwards. It is not an error
       if the aimed priority is above from or same as current one: This happens if several
       critical sections or get/release resource pairs are nested. */
    e_lwz       r3, rtos_currentPrio@sda21(13) /* r3: Current prio value, before change */
    se_cmpl     r4, r3
    se_bge      rai_return              /* r4>=r3: No change of current prio */
    e_stw       r4, rtos_currentPrio@sda21(13) /* Store checked, demanded value in prio var */
    e_b         rtos_processTriggeredEvents /* Check for possible task switches and return. */

rai_return:
    se_blr
    
/* End of rtos_scFlHdlr_resumeAllTasksByPriority */



/**
 *   @func rtos_resumeAllTasksByPriority
 * System call code for rtos_scFlHdlr_resumeAllTasksByPriority(). Function details
 * can be found there.
 *   @return
 * On return, r3 contains the priority level at entry into this function.
 *   @param resumeDownToThisPriority
 * On entry, r3 holds the aimed priority level.
 *   @remark
 * This code must be executed in user mode only. Any attempt to call from supervisor mode
 * can crash the system. In supervisor code, use rtos_osResumeAllTasksByPriority()
 * instead.
 */
    .section    .text.rtos_resumeAllTasksByPriority
    .globl      rtos_resumeAllTasksByPriority
    .balign     2
    
    /* Define the stack frame contents as offsets of data objects. Note the minimum offset
       of 8 due to the storage of stack pointer and link register. */
    //#define O_myVar         (8+0)
    #define SIZE_OF_PAYLOAD 0  /* Size of user data in stack frame */

    /* Size of stack frame: Room for SP and LR and uprounding to multiple of 8. */
    #define SIZE_OF_SF  ((((SIZE_OF_PAYLOAD)+15)/8)*8)
    #define O_LR        (SIZE_OF_SF+4)

rtos_resumeAllTasksByPriority:
    /* Create stack frame and save stack pointer and return address. */
    e_stwu      sp, -SIZE_OF_SF(sp)
    se_mflr     r0
    se_stw      r0, O_LR(sp)
    
    se_mr       r4, r3
    se_li       r3, RTOS_SYSCALL_RESUME_ALL_TASKS_BY_PRIORITY
    se_sc

    /* Remove the no longer needed stack frame and return. */
    se_lwz      r0, O_LR(sp)
    se_mtlr     r0
    se_addi     sp, SIZE_OF_SF
    se_blr

    #undef SIZE_OF_PAYLOAD
    #undef SIZE_OF_SF
    #undef O_LR

/* End of rtos_resumeAllTasksByPriority */
