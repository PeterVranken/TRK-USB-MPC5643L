/**
 * @file xsw_contextSwitch.c
 * Sample code for using the IVOR #4 and #8 handlers in int_interruptHandler.S to implement
 * a simple scheduler, which toggles in a cyclic manner between N execution contexts.\n
 *   The sample demonstrates only cooperative context switches, implemented by a system
 * call.\n
 *   The sample demonstrates the use of system calls.\n
 *   A terminal program should be connected (115.2 kBd, 8 Bit, 1 stop, no parity); the
 * contexts print a status message every about 1000 cycles of operation.
 *
 * Copyright (C) 2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   cxs_sc_switchContext
 *   xsw_startContextSwitching
 * Local functions
 *   getTBL
 *   returnAfterMicroseconds
 *   blink
 *   executionContext
 *   isrPit2
 *   enableIRQPit2
 */

/*
 * Include files
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "typ_types.h"
#include "lbd_ledAndButtonDriver.h"
#include "ihw_initMcuCoreHW.h"
#include "int_defStackFrame.h"
#include "sc_systemCalls.h"
#include "ccx_createContext.h"
#include "xsw_contextSwitch.h"


/*
 * Defines
 */

/** The number of execution contexts in this sample. */
#define NO_CONTEXTS 10

/** The first context is the normal context and the stack is the main stack defined in the
    linker command file. All other contexts require their own stack. The size needs to be at
    minimum N times the size of the ISR stack frame, where N is the number of interrupt
    priorities in use. In this sample we set N=3 (2 in serial, none here in this module,
    one as reserve for code extensions) and the stack frame size is S_I_StFr=168 Byte. So
    for safe interrupt handling we need a minimum of 504 Byte. This does not yet include
    the stack consumption of the implementation of the contexts.\n
      Note, the number of uint32 words in the stack needs to be even, otherwise the
    implementation of the 8 Byte alignment for the initial stack pointer value is wrong. */
#define STACK_SIZE_IN_BYTE (((3*S_I_StFr + 400)+7) & ~7)



/*
 * Local type definitions
 */


/*
 * Local prototypes
 */


/*
 * Data definitions
 */

/** Our minimalistic scheduler switches cyclically bewteen #NO_CONTEXTS contexts. On
    suspend, their descriptors are saved in the according field of this array. */
static int_contextSaveDesc_t _contextSaveDescAry[NO_CONTEXTS];

/** Our minimalistic scheduler switches cyclically bewteen #NO_CONTEXTS contexts. This is
    the currently resumed context. */
static unsigned int _idxActiveContext = 0;

/** Stack space for the additionally created execution contexts. (The first one inherits
    the standard stack from the startup code.) Note the alignment of 8 Byte, which is
    required to fulfill EABI constraints. */
static _Alignas(uint64_t) uint32_t _stackAry[NO_CONTEXTS-1]
                                            [(STACK_SIZE_IN_BYTE+sizeof(uint32_t)-1)
                                             / sizeof(uint32_t)
                                            ];

/** Stress test with IRQ by PIT2: Invocations of ISR are counted here. */
volatile unsigned long xsw_cntIsrPit2 = 0;

/** Number of context switches so far.\n
      Note that we purposely not declare the variable as volatile. Inside the scheduler
    there are no race conditions, all interrupt and system call handlers are serialized. */
unsigned long xsw_noContextSwitches = 0;


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
 * Delay code execution for a number of Microseconds of world time.
 *   @param tiInUs
 * The number of Microseconds to stay in the function.
 */
static void returnAfterMicroseconds(unsigned long tiInUs)
{
    assert(tiInUs > 0);
    const uint32_t tiReturn = (uint32_t)((float)tiInUs / 0.00833333333333f /* us/tick */)
                              + getTBL();
    while((int32_t)(tiReturn - getTBL()) > 0)
        ;

} /* End of returnAfterMicroseconds */



/**
 * Trivial routine that flashes the LED a number of times to give simple feedback. The
 * routine is blocking. The timing is independent of the system load, it is coupled to a real
 * time clock.
 *   @param noFlashes
 * The number of times the LED is lit.
 */
static void blink(uint16_t noFlashes)
{
#define TI_FLASH_MS 200

    while(noFlashes-- > 0)
    {
        lbd_setLED(lbd_led_D4_red, /* isOn */ true); /* Turn the LED on. */
        returnAfterMicroseconds(/* tiInUs */ TI_FLASH_MS*1000);
        lbd_setLED(lbd_led_D4_red, /* isOn */ false); /* Turn the LED off. */
        returnAfterMicroseconds(/* tiInUs */ TI_FLASH_MS*1000);
    }

    /* Wait for a second after the last flash - this command could easily be invoked
       immediately again and the bursts need to be separated. */
    returnAfterMicroseconds(/* tiInUs */ (1000-TI_FLASH_MS)*1000);

#undef TI_FLASH_MS
}



/**
 * Implementation of system call to switch from the one to another execution context by
 * index. Using only this system call but not running a timer interrupt to do context
 * switching yields a non-preemptive, cooperative scheduler.
 *   @return
 * \a true: This system call normally demands a context switch. However, if \a
 * idxOfResumedContext should denote the currently running context then it returns \a false
 * and not context switch happens.
 *   @param pCmdContextSwitch
 * Interface with the assembly code that implements the IVOR #8 handler.\n
 *   If the system call returns to a context, which had suspended in a system call (this
 * one or another one) then it can put the value to be returned to that context in \a
 * *pCmdContextSwitch.\n
 *   If the function returns \a true to request a context switch then it'll write
 * references to the descriptors of the suspended and resumed contexts into the same data
 * structure.\n
 *   This system always requests a context switch and returns \a signalToResumedContext to
 * the resumed context.
 *   @param signalToResumedContext
 * User provided argument of the system call: The index of the context to resume.
 *   @param signalToResumedContext
 * User provided argument of the system call: A value that is returned to the other,
 * resumed context as result of its system call, that had made it suspended before.\n
 *   Note, that the value would not be delivered if the resumed context should have been
 * suspended in an asynchronous, External Interrupt (and not in this system call). In this
 * case, the resumed context would be continued where it had been pre-empted by the
 * interrupt.
 *   @remark
 * Never call this function directly; it is invoked from the common IVOR #8 handler.
 */
bool cxs_sc_switchContext( int_cmdContextSwitch_t *pCmdContextSwitch
                         , unsigned int idxOfResumedContext
                         , uint32_t signalToResumedContext
                         )
{
    /* The result of the system call is stored in the passed data structure. */
    pCmdContextSwitch->retValSysCall = signalToResumedContext;

    assert(idxOfResumedContext < NO_CONTEXTS);
    if(idxOfResumedContext != _idxActiveContext)
    {
        ++ xsw_noContextSwitches;
        pCmdContextSwitch->pSuspendedContextSaveDesc = &_contextSaveDescAry[_idxActiveContext];
        pCmdContextSwitch->pResumedContextSaveDesc = &_contextSaveDescAry[idxOfResumedContext];
        _idxActiveContext = idxOfResumedContext;
        return true;
    }
    else
    {
        /* Requested context to resume is already running. */
        return false;
    }
} /* End of cxs_sc_switchContext */



/**
 * This function implements the behavior of the concurrent execution contexts. We use a
 * single function, all context behaves identical in this simple demo.
 *   @param idxThis
 * The passed system call argument initialData is used to tell the function from which
 * context it has been called.
 */
static _Noreturn uint32_t executionContext(uint32_t idxThis)
{
    /* The first thing to do is to create the next context. It would be more straight
       forward to this in the main function in a loop, which creates all of them and prior
       to running them but here we want to make use of the option to immediately start a
       new context.
         Creation of contexts is done as long as not yet all are created.
         Creation is not done for context 0: It is not a new one but the continuation of
       the standard context from the startup code. */
    assert(_idxActiveContext == idxThis);
    if(idxThis+1 < NO_CONTEXTS)
    {
        /* We are not the last one in the chain, create the next one and run it immediately
           (which will as first operation create the next one, etc.) */
        iprintf( "Startup cycle: Context %lu creates and starts context %lu\r\n"
               , idxThis
               , idxThis+1
               );
        _idxActiveContext = idxThis+1;
        
        /* Prefill stack memory to make stack usage observeable. */
        memset(&_stackAry[idxThis][0], 0xa5, STACK_SIZE_IN_BYTE);
        
        /* Create context and branch into it. We return from this (system) function call
           only after a complete cycle of chained context switches. */
        const ccx_contextDesc_t newContextDesc =
        {
            .executionEntryPoint = &executionContext,
            .stackPointer = (uint8_t*)&_stackAry[idxThis][0] + STACK_SIZE_IN_BYTE,
            .privilegedMode = true,
        };
        sc_createNewContext( &newContextDesc
                           , /* onExitGuard */          (void (*)(uint32_t))0x00000010
                           , /* runImmediately */       true
                           , /* initialData */          idxThis+1
                           , /* pNewContextSaveDesc */  &_contextSaveDescAry[idxThis+1]
                           , /* pThisContextSaveDesc */ &_contextSaveDescAry[idxThis]
                           );
    }

    /* After the context creation cycle, all contexts enter an infinite loop. The let the
       LED flash, print a status message and give control to the next context.
         Note, the last created context, which will not call sc_createNewContext in the if
       before, is expected to be the first one to get here. */
    const unsigned int idxNextContext = (idxThis+1) % NO_CONTEXTS;
    uint32_t idxResumedBy = (uint32_t)-1;
    unsigned int cntCycles = 0;
    for(;;)
    {
        /* To increase the number of context switches in this test we report our progress
           only occasionally. */
        const bool giveFeedback = ++cntCycles >= 1000;
        if(giveFeedback)
        {
            cntCycles = 0;
            iprintf( "This is context %lu, resumed by %lu. Context switches: %lu, PIT2:"
                     " %lu\r\n"
                   , idxThis
                   , idxResumedBy
                   , xsw_noContextSwitches
                   , xsw_cntIsrPit2
                   );
            blink(idxThis+1);

            iprintf("Context %lu suspends and resumes %u\r\n", idxThis, idxNextContext);
        }
        
        /* Switch to the next context in the chain. We return from this (system) function
           call only after a complete cycle of chained context switches. */
        idxResumedBy = sc_switchContext( /* idxOfResumedContext */ idxNextContext
                                       , /* signalToResumedContext */ idxThis
                                       );
    }
} /* executionContext */



/**
 * Most simple ISR just for stress testing. Runs at high priority and high frequency.
 */
static void isrPit2(void)
{
    ++ xsw_cntIsrPit2;

    /* Acknowledge the timer interrupt in the causing HW device. */
    PIT.TFLG2.R = 0x1;

} /* End of isrPit2 */



/**
 * Start the interrupt PIT2, which produces stress.
 */
static void enableIRQPit2(void)
{
    /* Disable all PIT timers during configuration. */
    PIT.PITMCR.R = 0x2;

    /* Install the interrupt handler for cyclic timer PIT 0. It drives the OS scheduler for
       cyclic task activation. */
    ihw_installINTCInterruptHandler( (int_externalInterruptHandler_t){.simpleIsr = &isrPit2}
                                   , /* vectorNum */ 61 /* PITimer Channel 2 */
                                   , /* psrPriority */ 10
                                   , /* isPreemptable */ true
                                   , /* isOsInterrupt */ false
                                   );

    /* Peripheral clock has been initialized to 120 MHz. To get a 0.1ms interrupt tick we
       need to count till 12000.
         11987: Prime number close to 12k to get irregular pattern with other interrupts.
         -1: See MCU reference manual, 36.5.1, p. 1157. */
    PIT.LDVAL2.R = 11987-1; /* Interrupt rate 100us */

    /* Enable interrupts by this timer and start it. */
    PIT.TCTRL2.R = 0x3;

    /* Enable timer operation and let them be stopped on debugger entry. Note, this is a
       global setting for all four timers, even if we use and reserve only one for the
       RTOS. */
    PIT.PITMCR.R = 0x1;

} /* End of enableIRQPit2 */


/**
 * Main function of sample scheduler: Create and start contexts and never return.
 */
void _Noreturn xsw_startContextSwitching(void)
{
    enableIRQPit2();
    
    /* Enter first (this) execution context. It'll create and start all others. */
    _idxActiveContext = 0;
    executionContext(/* idxThis */ _idxActiveContext);
    
    /* We never return from the started contexts. */
    assert(false);

} /* xsw_startContextSwitching */