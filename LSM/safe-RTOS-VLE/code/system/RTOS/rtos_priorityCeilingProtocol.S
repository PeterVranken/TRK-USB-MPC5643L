/**
 * @file rtos_priorityCeilingProtocol.c
 * Implementation of Priority Ceiling Protocol as system call, such that it becomes
 * available to the user task code.\n
 *   The implementation basically uses the PCP from the INTC, i.e. implemented in hardware.
 * It adds protection code, such that bad use of the system calls by the user task can't be
 * harmful. In particular, it must be avoided the the interrupt level can be lowered by a
 * task.
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
/* Module interface
 *   rtos_scBscHdlr_suspendAllInterruptsByPriority
 *   rtos_resumeAllInterruptsByPriority
 *   rtos_scBscHdlr_suspendAllInterruptsByPriority
 *   rtos_resumeAllInterruptsByPriority
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


/*
 * Defines
 */

/** Address of interrupt controller INTC0 in memory map. */
#define INTC0           0xfff48000

/** Address of INTC current priority register in memory map, for processor 0. */
#define INTC_CPR_PRC0   (INTC0+0x8)

/* Simple check of some configuration constraints. */
#if RTOS_PCP_KERNEL_PRIO > 15  ||  RTOS_PCP_MAX_LOCKABLE_PRIO+1 >= RTOS_PCP_KERNEL_PRIO
# error Invalid configuration of kernel priorities
#endif


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
 *   @func rtos_scBscHdlr_suspendAllInterruptsByPriority
 * Change the INTC priority of the calling user task context, only upwards and only up to a
 * value of #RTOS_PCP_MAX_LOCKABLE_PRIO.\n
 *    Any attempt to change \a suspendUpToThisPriority beyond #RTOS_PCP_MAX_LOCKABLE_PRIO
 * will lead to an #RTOS_CAUSE_TASK_ABBORTION_SYS_CALL_BAD_ARG exception.\n
 *    The attempt to lower the current priority is silently ignored.
 *   @return
 * On return, r3 contains the priority level at entry into this function (and into the
 * critical section). This priority level needs to be restored on exit from the critical
 * section, when rtos_scBscHdlr_resumeAllInterruptsByPriority is called.
 *   @param addressOfHandler
 * On entry, r3 holds the address of this system call handler. Not used.
 *   @param suspendUpToThisPriority
 * On entry, r4 holds the aimed priority level. All tasks/interrupts up to and including
 * this priority will be locked. The CPU will not handle them until the priority level is
 * lowered again.
 *   @remark
 * This is the implementation of a system call handler. The code must not be invoked
 * directly. The system call handler is compliant with the basic conformance class. It is
 * entirely executed with EE=0 and must not rely on sp, r2 and r13.
 */
    .section    .text.ivor
    .extern     rtos_osSystemCallBadArgument
    .globl      rtos_scBscHdlr_suspendAllInterruptsByPriority
    .balign     2

rtos_scBscHdlr_suspendAllInterruptsByPriority:
    /* The yielded interrupt level must be within the permitted range: Not at level of
       safety tasks or above and not below the level we had at entry into the task.
         The first condition needs to be checked, we would otherwise break the
       safety-concept.
         The second condition is always fulfilled, since we only change upwards. A check in
       DEBUG compilation is relative expensive and typical client code faults, which lead
       to this situation, are anyway caught by the DEBUG code in
       rtos_scBscHdlr_resumeAllInterruptsByPriority. So we don't do. */
    se_cmpli    r4, RTOS_PCP_MAX_LOCKABLE_PRIO  /* Upper boundary */
    e_bgt       rtos_osSystemCallBadArgument  /* r4>MAX: Abort task, demanded prio too high */
    e_lis       r5, INTC_CPR_PRC0@ha    /* Load upper half of INTC_CPR address */
    e_lwz       r3, INTC_CPR_PRC0@l(r5) /* r3: Current prio value, before change. For return */
    se_cmpl     r4, r3
    se_ble      sai_return              /* r4<=r3: No change of current prio */
    e_stw       r4, INTC_CPR_PRC0@l(r5) /* Write checked, demanded priority into INTC */
sai_return:
    se_rfi

/* End of rtos_scBscHdlr_suspendAllInterruptsByPriority */


/**
 *   @func rtos_suspendAllInterruptsByPriority
 * System call code for rtos_scBscHdlr_suspendAllInterruptsByPriority(). Function details
 * can be found there.
 *   @return
 * On return, r3 contains the priority level at entry into this function.
 *   @param suspendUpToThisPriority
 * On entry, r3 holds the aimed priority level.
 *   @remark
 * This code must be executed in user mode only. Any attempt to call from supervisor mode
 * can crash the system. In supervisor code, use rtos_osSuspendAllInterruptsByPriority()
 * instead.
 */
    .section    .text
    .globl      rtos_suspendAllInterruptsByPriority
    .balign     2
rtos_suspendAllInterruptsByPriority:
    se_mr       r4, r3
    se_li       r3, RTOS_SYSCALL_SUSPEND_ALL_INTERRUPTS_BY_PRIORITY
    se_sc
    se_blr
    
/* End of rtos_suspendAllInterruptsByPriority */



/**
 *   @func rtos_scBscHdlr_resumeAllInterruptsByPriority
 * Change the INTC priority of the calling user task context, only downwards and only down
 * to P0, where P0 is the initial priority of the system calling user task, which had been
 * statically agreed on by task configuration.\n
 *   Any attempt to change the priority beyond P0 will lead to an
 * #RTOS_CAUSE_TASK_ABBORTION_SYS_CALL_BAD_ARG exception.\n
 *   The attempt to raise the current priority is silently ignored.\n
 *   In DEBUG compilation only, it is validated that the aimed priority is not higher than
 * the allowed maximum #RTOS_PCP_MAX_LOCKABLE_PRIO. If so, it'll lead to an
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
 * On entry, r4 holds the aimed priority level. All tasks/interrupts at or above this level
 * will be schedulded again. Tasks/interrupts lower than this priority will still be
 * locked.
 *   @remark
 * This is the implementation of a system call handler. The code must not be invoked
 * directly. The system call handler is compliant with the basic conformance class. It is
 * entirely executed with EE=0 and must not rely on sp, r2 and r13.
 */
    .section    .text.ivor
    .extern     rtos_osSystemCallBadArgument
    .globl      rtos_scBscHdlr_resumeAllInterruptsByPriority
    .balign     2

rtos_scBscHdlr_resumeAllInterruptsByPriority:
    /* The yielded interrupt level must be within the permitted range: Not at level of
       safety tasks or above and not below the level we had at entry into the task.
         The first condition is always fulfilled, since we only change downwards but we can
       check it in DEBUG compilation to support debugging of the (then faulty) client code,
       which passes a wrong value.
         The second condition needs to be always checked, a violation is possible in case
       of faulty client code and it would be catastrophic. */
#ifdef DEBUG
    se_cmpli    r4, RTOS_PCP_MAX_LOCKABLE_PRIO  /* Upper boundary */
    e_bgt       rtos_osSystemCallBadArgument /* r4>MAX: Abort task, demanded prio too high */
#endif
    mfspr       r3, SPR_G0_SVSP     /* r3: SV sp to access stack frame of task start code. */
    se_lwz      r3, RUT_O_CPR(r3)   /* r3: Prio of task start */
    se_cmpl     r4, r3
    e_blt       rtos_osSystemCallBadArgument    /* r4<r3: Abort task: Demanded prio too low */

    /* The function allows to change the task priority only downwards. It is not an error
       if the aimed priority is above from or same as current one: This happens if several
       critical sections or get/release resource pairs are nested. */
    e_lis       r5, INTC_CPR_PRC0@ha    /* Load upper half of INTC_CPR address */
    e_lwz       r3, INTC_CPR_PRC0@l(r5) /* r3: Current prio value, before change */
    se_cmpl     r4, r3
    se_bge      rai_return              /* r4>=r3: No change of current prio */
    e_stw       r4, INTC_CPR_PRC0@l(r5) /* Write checked, demanded priority into INTC */
rai_return:
    se_rfi
    
/* End of rtos_scBscHdlr_resumeAllInterruptsByPriority */



/**
 *   @func rtos_resumeAllInterruptsByPriority
 * System call code for rtos_scBscHdlr_resumeAllInterruptsByPriority(). Function details
 * can be found there.
 *   @return
 * On return, r3 contains the priority level at entry into this function.
 *   @param resumeDownToThisPriority
 * On entry, r3 holds the aimed priority level.
 *   @remark
 * This code must be executed in user mode only. Any attempt to call from supervisor mode
 * can crash the system. In supervisor code, use rtos_osResumeAllInterruptsByPriority()
 * instead.
 */
    .section    .text
    .globl      rtos_resumeAllInterruptsByPriority
    .balign     2
rtos_resumeAllInterruptsByPriority:
    se_mr       r4, r3
    se_li       r3, RTOS_SYSCALL_RESUME_ALL_INTERRUPTS_BY_PRIORITY
    se_sc
    se_blr
    
/* End of rtos_resumeAllInterruptsByPriority */
