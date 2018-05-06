/**
 * @file tc02_oneTask.c
 *   Test case 02 of RTuinOS. One task is defined, which runs alternatingly with the idle
 * task.\n
 *   Observations [from the former Arduino documentation]:\n
 *   The object Serial is used to write progress and status information to console, under
 * which the current CPU load. It is remarkably high, which is not the consumption of the
 * software implemented here but the effect of the data streaming over the RS 232
 * connection. We have selected a Baud rate of only 9600 bps and all print commands block
 * until the characters to print are processed. At the beginning it is very fast as the
 * characters immediately fit into the send buffer. Once it has been filled the print
 * function is in mean as fast as the stream, 9600 characters a second. Even though the
 * rest is just waiting somewhere inside print it's lost CPU processing time for RTuinOS. A
 * hypothetical redesign of the library for serial communication for RTuinOS would
 * obviously use a suspend command to free this time for use by other tasks. This way, the
 * mean CPU load would become independent of the chosen Baud rate.\n
 *   Please consider to change the Baud rate to 115200 bps in setup() to prove that the CPU
 * load strongly goes down.\n
 *   This PowerPC implementation: The serial output is configured to use 115200 bps. It is
 * implemented using DMA from a buffer and writing into the buffer is never blocking - the
 * decision is to discard data, which does not fit but not to block until it fits.
 * Consequently, you will not observe a dependency of the CPU load on the chosen Baud rate.
 * The load is very little and will mostly be caused by the quite expensive floating point
 * printf applied in this sample.
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
 *
 * Module interface
 *   setup
 *   loop
 * Local functions
 *   task01_class00
 */

/*
 * Include files
 */

#include <stdio.h>
#include <assert.h>

#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
#include "f2d_float2Double.h"
#include "del_delay.h"
#include "rtos.h"
#include "mai_main.h"
#include "gsl_systemLoad.h"


/*
 * Defines
 */
 
/** The stack size needs to be at minimum (N+1) times the size of the ISR stack frame,
    where N is the number of interrupt priorities in use (+1 for the system call
    interrupt). In this sample we set N=4 (2 in serial and one for the RTuinOS system
    timer, one as reserve) and the stack frame size is S_I_StFr=168 Byte.\n
      This does not yet include the stack consumption of the implementation of the task
    itself.\n
      Note, the number of uint32 words in the stack needs to be even, otherwise the
    implementation of the 8 Byte alignment for the initial stack pointer value is wrong
    (checked by assertion). */
#define NO_IRQ_LEVELS           4
#define STACK_USAGE_IN_BYTE     400

/** The stack size in byte is derived from #STACK_USAGE_IN_BYTE and #NO_IRQ_LEVELS.
    Alignment constraints are considered in the computation. */
#define STACK_SIZE_TASK00_IN_BYTE   ((((1+(NO_IRQ_LEVELS))*S_I_StFr + STACK_USAGE_IN_BYTE)+7) & ~7)


/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
 
/*
 * Data definitions
 */
 
static _Alignas(uint64_t) uint8_t _taskStack[STACK_SIZE_TASK00_IN_BYTE];
static unsigned int _cpuLoad = 1000;

 
/*
 * Function implementation
 */

/**
 * Helper function: Read high resolution timer register of CPU. The register wraps around
 * after about 35s. The return value can be used to measure time spans up to this length.
 *   @return
 * Get the current register value. The value is incremented every 1/120MHz = (8+1/3)ns
 * regardless of the CPU activity.
 */
static inline uint32_t getTBL()
{
    uint32_t TBL;
    asm volatile ( /* AssemblerTemplate */
                   "mfspr %0, 268\n\r" /* SPR 268 = TBL, 269 = TBU */
                 : /* OutputOperands */ "=r" (TBL)
                 : /* InputOperands */
                 : /* Clobbers */
                 );
    return TBL;

} /* End of getTBL */




/**
 * The only task in this test case (besides idle).
 *   @param initCondition
 * The task gets the vector of events, which made it initially due.
 *   @remark
 * A task function must never return; this would cause a reset.
 */ 
static _Noreturn void task01_class00(uint32_t initCondition)
{
#define TICS_CYCLE  250

    uint32_t ti = getTBL()
           , tiCycle
           , u;
    
    iprintf("task01_class00: Activated by 0x%lx\r\n", initCondition);

    for(u=0; u<3; ++u)
        mai_blink(2);
    
    for(;;)
    {
        iprintf("task01_class00: rtos_delay...\n\r");
        u = rtos_delay(110);
        iprintf("task01_class00: Released with 0x%lx\r\n", u);
        
        iprintf("task01_class00: Suspending...\r\n");
        u = rtos_suspendTaskTillTime(/* deltaTimeTillRelease */ TICS_CYCLE);
        tiCycle = getTBL();
        iprintf("task01_class00: Released with 0x%lx\r\n", u);
        
        /* The system timer tick has a frequency of 1/RTOS_TICK Hz.
             Caution: The compiler fails to recognize the constant floating point
           expression if there's no explicit, superfluous pair of parenthesis around it.
           With parenthesis it compiles just one product, without it uses several products
           and divisions. */
        printf("Cycle time: %.1f%%\r\n"
              , f2d((tiCycle-ti) * ((1.0/120e6) * 100.0 / ((TICS_CYCLE)*RTOS_TICK)))
              );
        printf("CPU load: %.1f%%\r\n", f2d(_cpuLoad/10.0));
        
        ti = tiCycle;
    }
#undef TICS_CYCLE
} /* End of task01_class00 */





/**
 * The initalization of the RTOS tasks and general board initialization.
 */ 
void setup(void)
{
    /* Print standard greeting of RTuinOS applications. */
    iprintf(RTOS_EOL RTOS_RTUINOS_STARTUP_MSG RTOS_EOL);

    /* Configure task 1 of priority class 0 */
    rtos_initializeTask( /* idxTask */          0
                       , /* taskFunction */     task01_class00
                       , /* prioClass */        0
                       , /* pStackArea */       &_taskStack[0]
                       , /* stackSize */        sizeof(_taskStack)
                       , /* startEventMask */   RTOS_EVT_DELAY_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     5
                       );
} /* End of setup */




/**
 * The application owned part of the idle task. This routine is repeatedly called whenever
 * there's some execution time left. It's interrupted by any other task when it becomes
 * due.
 *   @remark
 * Different to all other tasks, the idle task routine may and should return. (The task as
 * such doesn't terminate). This has been designed in accordance with the meaning of the
 * original Arduino loop function.
 */ 

void loop(void)
{
    mai_blink(3);
    
    /* Share current CPU load measurement with task code, which owns Serial and which can
       thus display it. */
    _cpuLoad = gsl_getSystemLoad();
    
} /* End of loop */
