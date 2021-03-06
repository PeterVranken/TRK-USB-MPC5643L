#ifndef RTOS_IVORHANDLER_INCLUDED
#define RTOS_IVORHANDLER_INCLUDED
/**
 * @file rtos_ivorHandler.h
 * Part of definition of global interface of module rtos_ivorHandler.S.\n
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
  /* Place #include statements here, which are only read by C source files. */
#endif

#include "rtos_systemCall.h"


/*
 * Defines
 */

/** The number of different causes of non graceful task terminations. */
#define RTOS_NO_CAUSES_TASK_ABORTION                13

/* The enumeration of casues of task termination. */
#define RTOS_CAUSE_TASK_ABBORTION_PROCESS_ABORT     0  /* Process abort from user/scheduler */
#define RTOS_CAUSE_TASK_ABBORTION_MACHINE_CHECK     1  /* IVOR #1, Machine check, mostly memory
                                                          protection */
#define RTOS_CAUSE_TASK_ABBORTION_DEADLINE          2  /* Task exceeded deadline */
#define RTOS_CAUSE_TASK_ABBORTION_DI_STORAGE        3  /* IVOR #2/#3, MMU storage error */
#define RTOS_CAUSE_TASK_ABBORTION_SYS_CALL_BAD_ARG  4  /* Task referred to invalid system
                                                          call */
#define RTOS_CAUSE_TASK_ABBORTION_ALIGNMENT         5  /* IVOR #5, Alignment */
#define RTOS_CAUSE_TASK_ABBORTION_PROGRAM_INTERRUPT 6  /* IVOR #6, mostly illegal
                                                          instruction */
#define RTOS_CAUSE_TASK_ABBORTION_FPU_UNAVAIL       7  /* IVOR #7, Book E FPU instructions */
#define RTOS_CAUSE_TASK_ABBORTION_TBL_DATA          8  /* IVOR #13, TBL data access mismatch */
#define RTOS_CAUSE_TASK_ABBORTION_TBL_INSTRUCTION   9  /* IVOR #14, TBL instr access
                                                          mismatch */
#define RTOS_CAUSE_TASK_ABBORTION_SPE_INSTRUCTION   10 /* IVOR #32, use of SPE instruction */
#define RTOS_CAUSE_TASK_ABBORTION_USER_ABORT        11 /* User code returned error code */
#define RTOS_CAUSE_TASK_ABBORTION_RESERVED          12 /* Still unused error code */

/* Helper macro: Compute the size of a stack frame if the size of the contained user data
   is known. The macro considers the additional space for stack pointer and link register
   storage and the EABI constraint of a stack frame size being a multiple of eight.
     @param sizeOfPayload Size of user data in stack frame. Stack pointer and link register
   are not considered here. */
#define RTOS_SIZE_OF_SF(sizeOfPayload)      ((((sizeOfPayload)+15)/8)*8)

/* Define the offsets of fields in struct rtos_userTaskDesc_t. */
#define SIZE_OF_TASK_DESC       4
#define O_TDESC_ti              0

/* Define the offsets of fields in struct rtos_taskDesc_t. */
#define SIZE_OF_TASK_CONF       12
#define O_TCONF_pFct            0
#define O_TCONF_tiMax           4
#define O_TCONF_pid             8

/* Define the offsets of fields in struct processDesc_t. */
#define SIZE_OF_PROCESS_DESC    (12+(RTOS_NO_CAUSES_TASK_ABORTION)*4)
#define O_PDESC_USP             0
#define O_PDESC_ST              4
#define O_PDESC_CNTTOT          8
#define O_PDESC_CNTTARY         12

/* Define the field offsets and enumeration values in struct systemCallDesc_t. */
#define SIZE_OF_SC_DESC         8
#define O_SCDESC_sr             0
#define O_SCDESC_confCls        4
#define E_SCDESC_basicHdlr      RTOS_HDLR_CONF_CLASS_BASIC
#define E_SCDESC_simpleHdlr     RTOS_HDLR_CONF_CLASS_SIMPLE
#define E_SCDESC_fullHdlr       RTOS_HDLR_CONF_CLASS_FULL

/* Define the offsets of the stack frame of function rtos_osRunUserTask. Note the minimum
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

/* Define the offsets of the stack frame of the launch code for system  handlers of full
   conformance class. The definitions have been made public as they are shared between the
   implementations of the IVO #8 handler, system calls, and of the PCP, priority ceiling
   protocol. */
#define IV8_O_USP               (8+0)
#define IV8_O_PID               (8+4)
#define IV8_O_SRRi              (8+8)   /* Two 32 Bit registers = 8 Byte */
#define IV8_O_LR                (IV8_SIZE_OF_SF+4)
#define IV8_SIZE_OF_SF_PAYLOAD  16      /* Size of user data in stack frame */
#define IV8_SIZE_OF_SF          RTOS_SIZE_OF_SF(IV8_SIZE_OF_SF_PAYLOAD)

/** The index of the offered system call to terminate a user task. Note, this is not a
    configurable switch. Task termination needs to be system call zero. */
#define RTOS_SYSCALL_SUSPEND_TERMINATE_TASK  0

/** The index of the system call that implements the C assert macro. Note, the choice is
    actually made in the implementation files of the assert function. We need to duplicate
    the definition here as the assembly code can't read the original C code definition. To
    ensure consistency, the C code implementation file reads this header and compares this
    duplicate with its original definition. */
#define RTOS_SYSCALL_ASSERT_FUNC    6

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

/** This is an expression, which needs to be used in C unit rtos_process.c as condition for
    a static assertion. It double-checks the interface between C and assembly code and
    shapes a kind of minimalistic type-safety. See
    #RTOS_CONSTRAINTS_INTERFACE_C_AS_PROCESS, #RTOS_STATIC_CONSTRAINTS_INTERFACE_C_AS_PCP
    and #RTOS_STATIC_CONSTRAINTS_INTERFACE_C_AS_SYS_CALL, too. */
#define RTOS_STATIC_CONSTRAINTS_INTERFACE_C_AS_PROCESS (                                    \
            sizeof(rtos_taskDesc_t) == SIZE_OF_TASK_CONF                                    \
            &&  offsetof(rtos_taskDesc_t, addrTaskFct) == O_TCONF_pFct                      \
            &&  offsetof(rtos_taskDesc_t, addrTaskFct) == 0                                 \
            &&  sizeoffield(rtos_taskDesc_t, addrTaskFct) == 4                              \
            &&  sizeoffield(rtos_taskDesc_t, addrTaskFct) == sizeof(uintptr_t)              \
            &&  offsetof(rtos_taskDesc_t, tiTaskMax) == O_TCONF_tiMax                       \
            &&  sizeoffield(rtos_taskDesc_t, tiTaskMax) == 4                                \
            &&  offsetof(rtos_taskDesc_t, PID) == O_TCONF_pid                               \
            &&  sizeoffield(rtos_taskDesc_t, PID) == 1                                      \
            &&  sizeof(processDesc_t) == SIZE_OF_PROCESS_DESC                               \
            &&  offsetof(processDesc_t, userSP) == O_PDESC_USP                              \
            &&  O_PDESC_USP == 0                                                            \
            &&  offsetof(processDesc_t, state) == O_PDESC_ST                                \
            &&  sizeoffield(processDesc_t, state) == 1                                      \
            &&  offsetof(processDesc_t, cntTotalTaskFailure) == O_PDESC_CNTTOT              \
            &&  sizeoffield(processDesc_t, cntTotalTaskFailure) == 4                        \
            &&  offsetof(processDesc_t, cntTaskFailureAry) == O_PDESC_CNTTARY               \
            &&  sizeoffield(processDesc_t, cntTaskFailureAry)                               \
                == RTOS_NO_CAUSES_TASK_ABORTION*4                                           \
        )

/** This is an expression, which needs to be used in C unit rtos_process.c as condition for
    a runtime assertion. It double-checks the interface between C and assembly code and
    shapes a kind of minimalistic type-safety. See
    #RTOS_STATIC_CONSTRAINTS_INTERFACE_C_AS_PROCESS, too. */
#define RTOS_CONSTRAINTS_INTERFACE_C_AS_PROCESS (                                           \
            true                                                                            \
        )

/** This is an expression, which needs to be used in C unit rtos_systemCall.c as
    condition for a static assertion. It double-checks the interface between C and assembly
    implementation of the system call service and shapes a kind of minimalistic
    type-safety. See #RTOS_STATIC_CONSTRAINTS_INTERFACE_C_AS_PROCESS and
    #RTOS_STATIC_CONSTRAINTS_INTERFACE_C_AS_PCP, too. */
#define RTOS_STATIC_CONSTRAINTS_INTERFACE_C_AS_SYS_CALL (                                   \
            sizeof(systemCallDesc_t) == SIZE_OF_SC_DESC                                     \
            &&  offsetof(systemCallDesc_t, addressOfFct) == O_SCDESC_sr                     \
            &&  sizeoffield(systemCallDesc_t, addressOfFct) == 4                            \
            &&  offsetof(systemCallDesc_t, conformanceClass) == O_SCDESC_confCls            \
            &&  sizeoffield(systemCallDesc_t, conformanceClass) == 4                        \
        )

/** This is an expression, which needs to be used in C unit rtos_systemCall.c as condition
    for a runtime assertion. It double-checks the interface between C and assembly code and
    shapes a kind of minimalistic type-safety. See
    #RTOS_STATIC_CONSTRAINTS_INTERFACE_C_AS_SYS_CALL, too. */
#define RTOS_CONSTRAINTS_INTERFACE_C_AS_SYS_CALL (                                          \
            true                                                                            \
        )


/*
 * Global type definitions
 */

struct rtos_taskDesc_t;


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** C signature for an assembler entry point, which ends a system call handler with user
    task termination and counted error. Must be used solely from within the implementation
    of a system call and only if the abortion is due to a clear fault in the calling user
    code. */
_Noreturn void rtos_osSystemCallBadArgument(void);

/**
 * C signature for doing a system call.\n
 *   @return
 * The return value depends on the system call.
 *   @param idxSysCall
 * The index of the system call. See manual,
 * https://github.com/PeterVranken/TRK-USB-MPC5643L/blob/master/LSM/safe-RTOS-VLE/doc/manual/manual.pdf,
 * section System calls of RTOS, Table 1, p. 17, for a list of system calls offered by the
 * RTOS kernel. More system calls will be offered by your operating system, see your
 * device driver documentation.
 *   @param ...
 * The remaining arguments are passed register based to the system call implementation.
 * "Register based" means that the number of arguments is restricted to 7 values of 8..32
 * Bit each or accordingly lesser arguments if 64 Bit arguments are in use that require
 * two registers each.
 *   @remark
 * It depends on the particular system call, which context this function may be called
 * from. Nearly all system calls will be restricted to be called from user code. Only a few,
 * like the assert function, are accessible from operating system code, too. Please, refer
 * to the documentation of the particular system call.
 *   @remark
 * Note, the function is implemented in assmbler code and the doesn't implement all of what
 * the C signature promises. No arguments are passed on the stack, which limits the number
 * of possible system call arguments to 7 times 32 Bit.
 */
uint32_t rtos_systemCall(uint32_t idxSysCall, ...);

/** C signature to execute a process init function. Must be called from OS context only.
    Basically identical to rtos_osRunUserTask() but disregards the process status. It will
    create and run the task even if the process did not start yet.
      @remark The assembly code is such that it would pass a further argument as task param
    to the init function. Just by uncommenting the second argument it would work. We have
    the function argument commented as there's currently no use case for it and it saves us
    from loading a dummy value into register GPR4 at the calling code location. */
int32_t rtos_osRunInitTask(const struct rtos_taskDesc_t *pUserTaskConfig);

/** C signature to call a C function in a user process context. Must be called from OS
    context only.
      @return
    The created task may return a (positive) value. If it is aborted because of failures
    rtos_osRunUserTask() returns #RTOS_CAUSE_TASK_ABBORTION_USER_ABORT.
      @param pUserTaskConfig
    Descriptor of the created task. Mainly function pointer and ID of process to start the
    task in.
      @param taskParam
    A value meaningless to rtos_osRunUserTask(), only propagated to the invoked task
    function. */
int32_t rtos_osRunUserTask(const struct rtos_taskDesc_t *pUserTaskConfig, uint32_t taskParam);

/** C signature to terminate a user task. Can be called from any nested sub-routine in the
    user task.
      @param taskReturnValue
    If positive, the value is returned to the task creation function rtos_osRunUserTask(). If
    negative, rtos_osRunUserTask() receives #RTOS_CAUSE_TASK_ABBORTION_USER_ABORT and an error
    is counted in the process. */
_Noreturn void rtos_terminateUserTask(int32_t taskReturnValue);

#endif  /* For C code compilation only */
#endif  /* RTOS_IVORHANDLER_INCLUDED */
