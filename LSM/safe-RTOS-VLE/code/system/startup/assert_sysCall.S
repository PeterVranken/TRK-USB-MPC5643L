/**
 * @file assert_sysCall.c
 * Assert function: Implementation as basic system call handler. The basic type is required
 * in order to make the system call work well in all contexts, the OS contexts in
 * supervisor mode or the normal user tasks.
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
 * Local functions
 */

#ifndef __VLE__
# error This file is intended for compilation for VLE instruction set only
#endif

/* The entire contents of this file are not required in PRODUCTION compilation. */
#ifdef DEBUG

/*
 * Include files
 */


/*
 * Defines
 */

/** The code that implements the function called from the C assert macro, if its
    condition is \a false, can be compiled in different ways. It can halt the SW execution
    immediately or only count the occurances of \a false conditions and continue. The
    decision has a major impact on the testability of failure handling.\n
      One out of #ASSERT_FAILURE_BEHAVIOR_HALT_SW,
    #ASSERT_FAILURE_BEHAVIOR_CONTINUE_SW_AND_STORE_FIRST or
    #ASSERT_FAILURE_BEHAVIOR_CONTINUE_SW_AND_STORE_LAST is assigned to this define. See
    these other macros for more details. */
#define ASSERT_FAILURE_BEHAVIOR     ASSERT_FAILURE_BEHAVIOR_CONTINUE_SW_AND_STORE_FIRST

/** Traditional behavior of assert: The SW execution is halted at the first occurrence of an
    assert macro with \a false condition.\n
      This behavior can be chosen when software is functionally tested, which is expected to
    be error free.\n
      This behavior must not be used if the software's failure handling is tested, e.g. by
    purposely injected errors. If this behavior is chosen then the fault handling system of
    the RTOS is by principle non-trustable. There's a significant chance that the software
    halts because of unintended entry of a straying task into the assert system call.\n
      See #ASSERT_FAILURE_BEHAVIOR, too. */
#define ASSERT_FAILURE_BEHAVIOR_HALT_SW                             1

/// @todo We can't continue after an assert macro: The function is considered _Noreturn by
// the compiler. We need to re-define the behavior, e.g. store_and_wait. wait != halt: We
// spin in an endless loop but only in problem state and with MSR[EE]=1 so that scheduler
// and deadline monitoring are still operational. The failing task won't proceed (just wait
// for its deadline failure abort) and the chance to catch this in a debugger is quite
// high.
//   Change impact on code is little but we have to rename and re-doc the macros.
//   todoc: In typical situations, the macro exposes the traditional behavior: If placed in
// SV code, if placed in supervisory task (has highest prio)

/** "Safe" behavior of assert: The occurrence of assert macros with \a false condition is
    counted. The assert information is stored only for the very first occurrence of an
    assert macro with \a false condition. The SW execution is halted if the assert macro is
    placed in operating system code but continued behind the assert macro if it is placed
    in user task code.\n
      Note: This behavior can be used with SW compilations, which are to be used for
    testing of the software's failure handling capabilities. */
#define ASSERT_FAILURE_BEHAVIOR_CONTINUE_SW_AND_STORE_FIRST         2

/** "Safe" behavior of assert: The occurrence of assert macros with \a false condition is
    counted. The assert information is stored for the last recent occurrence of an assert
    macro with \a false condition. The SW execution is halted if the assert macro is placed
    in operating system code but continued behind the assert macro if it is placed in user
    task code.\n
      Note: This behavior can be used with SW compilations, which are to be used for
    testing of the software's failure handling capabilities. */
#define ASSERT_FAILURE_BEHAVIOR_CONTINUE_SW_AND_STORE_LAST          3


/** SPR index of process ID. */
#define SPR_PID0    48


#if !(ASSERT_FAILURE_BEHAVIOR == ASSERT_FAILURE_BEHAVIOR_CONTINUE_SW_AND_STORE_FIRST \
      ||  ASSERT_FAILURE_BEHAVIOR == ASSERT_FAILURE_BEHAVIOR_CONTINUE_SW_AND_STORE_LAST \
      ||  ASSERT_FAILURE_BEHAVIOR == ASSERT_FAILURE_BEHAVIOR_HALT_SW \
     )
# error Invalid behavior chosen for assert function
#endif


/*
 * External function declarations
 */


/*
 * Data declarations
 */

    .extern     assert_assertionFired, assert_fileName, assert_funcName
    .extern     assert_line, assert_expression

/*
 * Function implementation
 */

/**
 *   @func assert_scBscHdlr_assert_func
 * Implementation of the assert function, which is called by the C assert macro if the
 * condition fails. The implementation here is made as basic system call handler. This
 * permits offering the service to user and operating system code.\n
 *   For kernel code it's soon decided how to proceed: A violated invariant condition will
 * halt the SW execution as traditionally the case for the assert macro.\n
 *   For user code we could halt the SW but this impacts the SW behavior such that DEBUG
 * compiled SW is no longer testable with respect to fault handling. It's unavoidable that
 * failing, straying code could enter the system call by accident. It will halt the SW
 * execution (and report wrong fault information through the stored assert information,
 * filename, line number, etc.) rather then running through the fault handling procedures
 * established in the operating system. Fault handling would become untestable by
 * principle.\n
 *   On the other hand is halting the SW still very useful for all early, "normal" testing,
 * when the basic SW functionality is put to operation and initially tested. A logical
 * fault halts execution and the stored assert information points directly to the violated
 * invariant.\n
 *   We offer two behaviors of the assert function. User code, which enters the assert
 * function may halt the code execution or it just stores the assert information, counts
 * the occurance and returns (to the failing user task). The choice needs to be made at
 * compile time, see macro #ASSERT_FAILURE_BEHAVIOR.
 *   @param fileName
 * On entry, r4 holds a pointer to the constant string with the name of source, which the
 * failing assertion is located in.
 *   @param line
 * On entry, r5 holds a pointer to the constant string with the line number, which the
 * failing assertion is located at.
 *   @param funcName
 * On entry, r6 holds a pointer to the constant string with the name of C function, which
 * the failing assertion is located in.
 *   @param expression
 * On entry, r7 holds a pointer to the constant string with the failing expression in the
 * assertion.
 *   @remark
 * This is the implementation of a system call handler. The code must not be invoked
 * directly. The system call handler is compliant with the basic conformance class. It is
 * executed with EE=0 and must not rely on sp, r2 and r13
 */
    .section    .text.ivor
    .globl      assert_scBscHdlr_assert_func
    .align      2

assert_scBscHdlr_assert_func:
    /* Globaly store fault location related information, file name, line number, etc.,
       provided by the C assert macro. The information is passed in as function arguments
       in r4, r5, etc. */
    e_lis       r3, assert_noOccurances@ha
    e_lwz       r0, assert_noOccurances@l(r3)
    se_addi     r0, 1
    se_cmpli    r0, 1
    se_blt      ass_saturateOcc     /* Don't store increment if we wrap around */
    e_stw       r0, assert_noOccurances@l(r3)
ass_saturateOcc:

#if ASSERT_FAILURE_BEHAVIOR == ASSERT_FAILURE_BEHAVIOR_CONTINUE_SW_AND_STORE_FIRST
    se_bne      ass_noUpdErrInfo
#endif
    e_lis       r3, assert_fileName@ha
    e_stw       r4, assert_fileName@l(r3)
    e_lis       r3, assert_line@ha
    e_stw       r5, assert_line@l(r3)
    e_lis       r3, assert_funcName@ha
    e_stw       r6, assert_funcName@l(r3)
    e_lis       r3, assert_expression@ha
    e_stw       r7, assert_expression@l(r3)
ass_noUpdErrInfo:
    mfspr       r0, SPR_PID0    /* PID is not changed prior to entry in basic handler */
    e_lis       r3, assert_PID@ha
    e_lbz       r4, assert_PID@l(r3)    /* r4: PID stored so far, can initially be 0xff */
    se_extsb    r4                      /* r4: PID so far as int32_t, range [-1 .. 4] */
    se_cmp      r0, r4                  /* Signed compare with PID from this assertion */
    se_ble      ass_noUpdPID            /* Don't store if not greater */
    e_stb       r0, assert_PID@l(r3)    /* Store new greatest seen PID. */
ass_noUpdPID:

    /* For testability of failure handling of SW, we must not halt a user task. We inspect
       the machine status of the calling context if testability is requested. */
#if ASSERT_FAILURE_BEHAVIOR == ASSERT_FAILURE_BEHAVIOR_CONTINUE_SW_AND_STORE_FIRST \
    ||  ASSERT_FAILURE_BEHAVIOR == ASSERT_FAILURE_BEHAVIOR_CONTINUE_SW_AND_STORE_LAST

    mfsrr1      r0                  /* Load MSR of system calling process */
    se_btsti    r0, 17              /* Test bit "problem state" */
    se_bne      ass_contUserTask    /* if(Problem state == user) return; */

#endif

    /* MSR[EE] = 0 is always granted on entry into a basic system call handler and all
       which remains is entering an infinite loop. */
    .extern     assert_infiniteLoopAfterCaughtError /* Just to have it in the map file. */
assert_infiniteLoopAfterCaughtError:
    se_b        assert_infiniteLoopAfterCaughtError

    /* Return to the user task, which had made the system call. */
ass_contUserTask:
    se_li       r3, 0
    se_rfi

/* End of assert_scBscHdlr_assert_func */


#endif /* DEBUG */
