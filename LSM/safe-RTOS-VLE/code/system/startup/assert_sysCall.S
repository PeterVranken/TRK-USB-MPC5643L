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
 *
 *   @param 
 * On entry, r4 holds 
 *   @remark
 * This is the implementation of a system call handler. The code must not be invoked
 * directly. The system call handler is compliant with the basic conformance class. It is
 * executed with EE=0 and must not rely on sp, r2 and r13
 */
    .section    .text.ivor
    .extern     ivr_systemCallBadArgument
    .globl      assert_scBscHdlr_assert_func
    .align      2

assert_scBscHdlr_assert_func:
    /* Globaly store fault location related information, file name, line number, etc.,
       provided by the C assert macro. The information is passed in as function arguments
       in r4, r5, etc. */
    e_lis       r3, assert_fileName@ha
    e_stw       r4, assert_fileName@l(r3)
    e_lis       r3, assert_line@ha
    e_stw       r5, assert_line@l(r3)
    e_lis       r3, assert_funcName@ha
    e_stw       r6, assert_funcName@l(r3)
    e_lis       r3, assert_expression@ha
    e_stw       r7, assert_expression@l(r3)
    se_li       r0, 1   /* Indicate the problem by setting bool assert_assertionFired=true */
    e_lis       r3, assert_assertionFired@ha
    e_stb       r0, assert_assertionFired@l(r3)

    /* MSR[EE] = 0 is always granted on entry into a basic system call handler and all
       which remains to do is entering an infinite loop. */
    se_b        .
    
/* End of assert_scBscHdlr_assert_func */


#endif /* DEBUG */