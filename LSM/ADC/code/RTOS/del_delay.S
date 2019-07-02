/**
 * @file del_delay.S
 * The CPU load measurement requires a defined delay time in terms of CPU execution clock
 * ticks (as opposed to elapsing world time under control of an independent clock).
 *   @remark This
 * code had initially been developed as inline assembly in the C module, where it is
 * required but this approach failed: The execution time of the statements, which are
 * applied to realize the delay, depends on the load address of the code. This is due to
 * the 64 Bit instruction address bus; the delay code can either be in a single double word
 * or be spread across two such words. The load address can't be controlled from C inline
 * assembly.
 *
 * Copyright (C) 2017-2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   del_delayMicroseconds
 * Local functions
 */

/*
 * Include files
 */


/*
 * Defines
 */

/** Is the instruction cache switched on? This is decided in file sup_startup.S */
#define IS_ENABLE_ICACHE            1

/** Is the branch prediction switched on? This is decided in file sup_startup.S */
#define IS_ENABLE_BRANCH_PREDICTION 1


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
 *   @func del_delayMicroseconds
 * This function does nothing but consumes an exactely determined amount of CPU execution
 * time before it returns. "Consumption of CPU time" means that not the elapsed world time
 * counts; all interrupts etc. which interrupt the execution of this function won't be
 * accounted. The blocking time of this function until return therefore is greater or
 * equal to \a tiCpuInUs Microseconds.
 *   @param r3
 * r3, 32 Bit, "tiCpuInUs": The CPU executes further useless code this number of
 * Microseconds.
 *   @remark The implementation is done in assembler in order to keep the execution time
 * independent of the compiler's optimization settings; particularly the compile
 * configuration DEBUG/PRODUCTION. Furthermore, the execution time needs to be kept clear
 * of load address caused variances, which can be controlled only in an assembly module.
 *   @remark
 * This function can be called from user and supervisor mode.
 */
    .section .text.del_delayMicroseconds, "ax", @progbits
    .global del_delayMicroseconds, del_loopDelayMicroseconds
    
    /* The alignment of the function code is the most important element of the
       implementation. The loop code must be inside a 64 Bit word because of the 64 Bit
       address bus for fetching instructions. Otherwise the execution time rises and the
       timing calibration fails.
         The alignment is ensured by the .align in combination with the nop instructions
       and can be double checked in the map file; therefore, we've made
       del_loopDelayMicroseconds a global symbol. */
    .balign     8
    
del_delayMicroseconds:

#ifdef __VLE__
# if !IS_ENABLE_ICACHE && !IS_ENABLE_BRANCH_PREDICTION
#  define NO_CYCLES_PER_US   20     /* I-cache off, no branch prediction. */
# elif IS_ENABLE_ICACHE && IS_ENABLE_BRANCH_PREDICTION
# define NO_CYCLES_PER_US   60  /* I-cache and branch prediction on. */
# else
#  error Module del_delay.S needs calibration for chosen confguration
# endif

    e_mulli r3, r3, NO_CYCLES_PER_US    /* Rescale us to CPU clock ticks. */
    
    se_cmpli r3, 1                  /* Two loops delay less to compensate for */
    e_ble   exitDelayMicroseconds   /* overhead, like function call and return. */
    e_addi  r3, r3, -2
    .balignw    8, 0x4400   /* Ensure alignment of loop code on 8 Byte address using fill
                               pattern "se_nop" */
del_loopDelayMicroseconds:
    e_add2i. r3, -1
    se_bne  del_loopDelayMicroseconds

exitDelayMicroseconds:
    se_blr

# undef NO_CYCLES_PER_US
#else /* Book E instruction set */

# if !IS_ENABLE_ICACHE && !IS_ENABLE_BRANCH_PREDICTION
#  define NO_CYCLES_PER_US_NUM   40 /* I-cache off, no branch prediction. */
#  define NO_CYCLES_PER_US_DEN   3   
# define NO_CYCLES_OVERHEAD     3
# elif IS_ENABLE_ICACHE && IS_ENABLE_BRANCH_PREDICTION
#  define NO_CYCLES_PER_US_NUM   40 /* I-cache and branch prediction on. */
#  define NO_CYCLES_PER_US_DEN   1
#  define NO_CYCLES_OVERHEAD     9
# else
#  error Module del_delay.S needs calibration for chosen confguration
# endif

    li      r4, NO_CYCLES_PER_US_DEN    /* Rescale us to CPU clock ticks. */
    mulli   r3, r3, NO_CYCLES_PER_US_NUM   
    divwu   r3, r3, r4
    
    cmpli   cr7, r3, (NO_CYCLES_OVERHEAD-1) /* A few loops delay less to compensate for */
    ble     cr7, exitDelayMicroseconds      /* overhead, like function call and return. */
    subi    r3, r3, NO_CYCLES_OVERHEAD
    
/// @todo Prove correctness of next two lines and uncomment
//    .balignl    8, 0x60000000  /* Ensure alignment of loop code on 8 Byte address using fill
//                                  pattern "ori r0, r0, 0" */
del_loopDelayMicroseconds:
    subi    r3, r3, 1
    cmpwi   cr7, r3, 0
    bne+    cr7, del_loopDelayMicroseconds

exitDelayMicroseconds:
    blr

# undef NO_CYCLES_PER_US_NUM   
# undef NO_CYCLES_PER_US_DEN   
# undef NO_CYCLES_OVERHEAD
#endif

/* End of del_delayMicroseconds */
