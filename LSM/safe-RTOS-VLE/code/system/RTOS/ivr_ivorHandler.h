#ifndef IVR_IVORHANDLER_INCLUDED
#define IVR_IVORHANDLER_INCLUDED
/**
 * @file ivr_ivorHandler.h
 * Part of definition of global interface of module ivr_ivorHandler.S.\n
 *   This file contains all global difinitions, which are used by the assembler
 * implementation. The file is shared between C and assembler code.
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
//# include "prc_process.h"
#endif

#include "sc_systemCall.h"


/*
 * Defines
 */

/** The number of different causes of non graceful task terminations. */
#define IVR_NO_CAUSES_TASK_ABORTION 13

/* The enumeration of casues of task termination. */
/// @todo Move this into rtos.h, the user level header file
#define IVR_CAUSE_TASK_ABBORTION_PROCESS_ABORT      0 /* Process abort from user/scheduler */
#define IVR_CAUSE_TASK_ABBORTION_MACHINE_CHECK      1 /* IVOR #1, Machine check, mostly memory
                                                         protection */
#define IVR_CAUSE_TASK_ABBORTION_DEADLINE           2 /* Task exceeded deadline */
#define IVR_CAUSE_TASK_ABBORTION_SYS_CALL_BAD_ARG   3 /* Task referred to invalid system
                                                         call */ 
#define IVR_CAUSE_TASK_ABBORTION_USER_ABORT         4 /* User code returned error code */
#define IVR_CAUSE_TASK_ABBORTION_ALIGNMENT          5 /* IVOR #5, Alignment */
#define IVR_CAUSE_TASK_ABBORTION_PROGRAM_INTERRUPT  6 /* IVOR #6, mostly illegal instruction */
#define IVR_CAUSE_TASK_ABBORTION_FPU_UNAVAIL        7 /* IVOR #7, Book E FPU instructions */
#define IVR_CAUSE_TASK_ABBORTION_TBL_DATA           8 /* IVOR #13, TBL data access mismatch */
#define IVR_CAUSE_TASK_ABBORTION_TBL_INSTRUCTION    9 /* IVOR #14, TBL instr access mismatch */
#define IVR_CAUSE_TASK_ABBORTION_TRAP              10 /* IVOR #15, trap and debug events */
#define IVR_CAUSE_TASK_ABBORTION_SPE_INSTRUCTION   11 /* IVOR #32, use of SPE instruction */

/* Define the offsets of fields in struct prc_userTaskDesc_t. */
#define SIZE_OF_TASK_DESC       4
#define O_TDESC_ti              0

/* Define the offsets of fields in struct prc_userTaskConfig_t. */
#define SIZE_OF_TASK_CONF       12
#define O_TCONF_pFct            0
#define O_TCONF_tiMax           4
#define O_TCONF_pid             8

/* Define the offsets of fields in struct prc_processDesc_t. */
#define SIZE_OF_PROCESS_DESC    (12+(IVR_NO_CAUSES_TASK_ABORTION)*4)
#define O_PDESC_USP             0
#define O_PDESC_ST              4
#define O_PDESC_CNTTOT          8
#define O_PDESC_CNTTARY         12

/* Define the field offsets and enumeration values in struct sc_systemCallDesc_t. */
#define SIZE_OF_SC_DESC         8
#define O_SCDESC_sr             0
#define O_SCDESC_confCls        4
#define E_SCDESC_basicHdlr      SC_HDLR_CONF_CLASS_BASIC 
#define E_SCDESC_simpleHdlr     SC_HDLR_CONF_CLASS_SIMPLE
#define E_SCDESC_fullHdlr       SC_HDLR_CONF_CLASS_FULL

/* Define the offsets of the stack frame of function ivr_runUserTask. Note the minimum
   offset of 8 due to the storage of stack pointer and link register.
     The pointer to this stack frame is globally stored and used from different code
   locations to implement exceptions and task termination. Therefore we define it globally. */
#define RUT_O_USP               (8+0)
#define RUT_O_SVSP              (8+4)
#define RUT_O_pPDESC            (8+8)
#define RUT_O_tiAvl             (8+12)
#define RUT_O_CPR               (8+16)
#define RUT_O_NVGPR             (8+20)  /* Non volatile GPR: r14 .. r31 = 18*4 = 72 Bytes */
#define RUT_SIZE_OF_SF_PAYLOAD  92      /* Size of user data in stack frame */

/** The index of the offered system call to terminate a user task. Note, this is not a
    configurable switch. Task termination needs to be system call zero. */
#define IVR_SYSCALL_SUSPEND_TERMINATE_TASK  0

/** SPR index of SPRG0. We use it for temporary storage of the SV stack pointer. It is
    possible to hold it in RAM, too, using the SPR is just to have more concise code for
    the frequently accessed variable. */
#define SPR_G0_SVSP 272

/** SPR index of SPRG1. It permanently holds the value of r13, the SDA base pointer. This
    value needs to be restored at any return from user mode and it is constant all the
    time. */
#define SPR_G1_SDA  273

/** SPR index of SPRG2. It permanently holds the value of r2, the SDA2 base pointer. This
    value needs to be restored at any return from user mode and it is constant all the
    time. */
#define SPR_G2_SDA2 274


#ifdef __STDC_VERSION__
/*
 * Global type definitions
 */

struct prc_userTaskConfig_t;


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** C signature for a assembler entry point, which ends a system call handler with user
    task termination and counted error. Must be used solely from within the implementation
    of a system call and only if the abortion is due to a clear fault in the calling user
    code. */
_Noreturn void ivr_systemCallBadArgument(void);

/** C signature for doing a system call. Note, the signature doesn't ... @TODOC */
uint32_t ivr_systemCall(uint32_t idxSysCall, ...);

/** C signature to execute a process init function. Must be called from OS context only.
    Basically identical to ivr_runUserTask() but disregards the process status. It will
    create and run the task even if the process did not start yet. */
int32_t ivr_runInitTask(const struct prc_userTaskConfig_t *pUserTaskConfig);

/** C signature to call a C function in a user process context. Must be called from OS
    context only.
      @return
    The created task may return a (positive) value. If it is aborted because of failures
    ivr_runUserTask() returns #IVR_CAUSE_TASK_ABBORTION_USER_ABORT.
      @param pUserTaskConfig
    Descriptor of the created task. Mainly function pointer and ID of process to start the
    task in.
      @param taskParam
    A value meaningless to ivr_runUserTask(), only propagated to the invoked task
    function. */
int32_t ivr_runUserTask( const struct prc_userTaskConfig_t *pUserTaskConfig
                       , uint32_t taskParam
                       );

/** C signature to terminate a user task. Can be called from any nested sub-routine in the
    user task.
      @param taskReturnValue
    If positive, the value is returned to the task creation function ivr_runUserTask(). If
    negative, ivr_runUserTask() receives #IVR_CAUSE_TASK_ABBORTION_USER_ABORT and an error
    is counted in the process. */
_Noreturn void ivr_terminateUserTask(int32_t taskReturnValue);

#endif  /* For C code compilation only */
#endif  /* IVR_IVORHANDLER_INCLUDED */
