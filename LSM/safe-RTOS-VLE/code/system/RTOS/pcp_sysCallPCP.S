/**
 * @file pcp_sysCallPCP.c
 * Implementation of Priority Ceiling Protocol as system call, such taht it becomes
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
 *   pcp_scBscHdlr_suspendAllInterruptsByPriority
 *   pcp_suspendAllInterruptsByPriority
 * Local functions
 */

#ifndef __VLE__
# error This file is intended for compilation for VLE instruction set only
#endif

/*
 * Include files
 */

#include "pcp_sysCallPCP.h"
#include "ivr_ivorHandler.h"


/*
 * Defines
 */

/** Address of interrupt controller INTC0 in memory map. */
#define INTC0           0xfff48000

/** Address of INTC current priority register in memory map, for processor 0. */
#define INTC_CPR_PRC0   (INTC0+0x8)


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
 *   @func pcp_scBscHdlr_suspendAllInterruptsByPriority
 * Change the INTC priority of the calling user task context in the range from P0 to
 * (#RTOS_KERNEL_PRIORITY-2). P0 is the initial priority of the user task, statically
 * agreed by task configuration.\n
 *    Any attempt to change \a suspendUpToThisPriority beyond these limits will lead to an
 * #IVR_CAUSE_TASK_ABBORTION_SYS_CALL_BAD_ARG exception.
 *   @return
 * On return, r3 contains the priority level at entry into this function (and into the
 * critical section). This priority level needs to be restored on exit from the critical
 * section.
 *   @param suspendUpToThisPriority
 * On entry, r4 holds the aimed priority level. All tasks/interrupts up to and including
 * this priority will be locked. The CPU will not handle them until the priority level is
 * lowered again.
 *   @remark
 * This is the implementation of a system call handler. The code must not be invoked
 * directly. The system call handler is compliant with the basic conformance class. It is
 * executed with EE=0 and must not rely on sp, r2 and r13
 */
    .section    .text.ivor
    .extern     ivr_systemCallBadArgument
    .globl      pcp_scBscHdlr_suspendAllInterruptsByPriority
    .align      2

pcp_scBscHdlr_suspendAllInterruptsByPriority:
    se_mr       r0, sp                  /* Temporary storage of user sp on entry. */
    mfspr       sp, SPR_G0_SVSP         /* Restore SV sp to access stack frame of task start
                                           code. */
    /* The user code demanded interrupt level must be within the permitted range: Not at
       level of safety task and not below the level we had at entry into the task. */
    se_cmpli    r4, PCP_KERNEL_PRIORITY-1   /* -1: One level is reserved for safety tasks */
    e_bge       ivr_systemCallBadArgument   /* Abort task: Demanded prio too high */
    se_lwz      r3, RUT_O_CPR(sp)       /* r3: Prio of task start */
    se_cmpl     r4, r3
    e_blt       ivr_systemCallBadArgument   /* Abort task: Demanded prio too low */
    e_lis       r5, INTC_CPR_PRC0@ha    /* Load upper half of INTC_CPR address */
    e_lwz       r3, INTC_CPR_PRC0@l(r5) /* r3: Current prio value, before change. For return */
    e_stw       r4, INTC_CPR_PRC0@l(r5) /* Store checked, demanded priority in INTC */

    /* Restore the user stack pointer and return to the user task. */
    se_mr       sp, r0
    se_rfi
    
/* End of pcp_scBscHdlr_suspendAllInterruptsByPriority */


