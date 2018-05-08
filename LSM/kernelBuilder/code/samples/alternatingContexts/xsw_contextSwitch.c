/**
 * @file xsw_contextSwitch.c
 * Sample code for using the IVOR #4 and #8 handlers in int_interruptHandler.S to implement
 * a simple scheduler, which toggles between two execution contexts.\n
 *   The sample demonstrates that cooperative context switches can be implemented by a
 * system call.\n
 *   The sample demonstrates that context switches by asynchronous, External Interrupts are
 * not restricted to a single system timer interrupt. The sample uses two regular timers
 * with mutual prime cycle times on different interrupt priority levels, which both demand
 * a context switch.\n
 *   The sample demonstrates the use of system calls. It offers a primitive concept of
 * semaphores. Two system call permit to acquire and release a semaphore. A semaphore is
 * applied to control the ownership of the LED between the two execution contexts.\n
 *   A terminal program should be connected (115.2 kBd, 8 Bit, 1 stop, no parity); both
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
 * Local functions
 *   getTBL
 *   returnAfterMicroseconds
 *   blink
 *   subRoutineOf2ndCtx
 *   secondContext
 *   isrSystemTimerTick1
 *   enableIRQTimerTick1
 *   isrSystemTimerTick2
 *   enableIRQTimerTick2
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
#include "del_delay.h"
#include "ihw_initMcuCoreHW.h"
#include "int_defStackFrame.h"
#include "tcx_testContext.h"
#include "sc_systemCalls.h"
#include "ccx_createContext.h"
#include "xsw_contextSwitch.h"


/*
 * Defines
 */
 
/** Configuration of kernelBuilder: This sample doesn't make use of stack sharing but can
    be compiled in both configurations, with or without stack sharing support. */

/** The first context is the normal context and the stack is the main stack defined in the
    linker command file. The second context requires its own stack. The size needs to be at
    minimum N times the size of the ISR stack frame, where N is the number of interrupt
    priorities in use. In this sample we set N=6 (2 in serial and 3 here in this module,
    one as reserve for code extensions) and the stack frame size is S_I_StFr=168 Byte. So
    for safe interrupt handling we need a minimum of 840 Byte. This does not yet include
    the stack consumption of the implementation of the context.\n
      Note, the number of uint32 words in the stack needs to be even, otherwise the
    implementation of the 8 Byte alignment for the initial stack pointer value is wrong. */
#define STACK_SIZE_IN_BYTE (((6*S_I_StFr + 400)+7) & ~7)

/** The number of offered semaphore variables. */
#define NO_SEMAPHORES   10

/** The initial count of all semaphore varaibles. */
#define SEMAPHORE_INITIAL_COUNT 10


/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
 
/*
 * Data definitions
 */
 
/* Our minimalistic scheduler switches alternatingly bewteen two contexts. On suspend, their
   descriptors are saved in these data structures. */
static int_contextSaveDesc_t _contextSaveDesc1
                           , _contextSaveDesc2;

/** Our minimalistic scheduler switches alternatingly bewteen two contexts. This is the
    currently resumed context. */
static bool _context1IsActive = true;

/** Stack space for the second execution context. Note the alignment of 8 Byte, which is
    required to fulfill EABI constraints. */
static _Alignas(uint64_t) uint32_t _stack2ndCtxt[(STACK_SIZE_IN_BYTE+sizeof(uint32_t)-1)
                                                 / sizeof(uint32_t)
                                                ];

/** The array of offered semphore variables. */
static uint32_t _semaphoreAry[NO_SEMAPHORES] =
                                   {[0 ... NO_SEMAPHORES-1] = SEMAPHORE_INITIAL_COUNT};

/** Stress test with IRQ by PIT2: Invocations of ISR are counted here. */
volatile unsigned long xsw_cntIsrPit2 = 0;

/** Number of context switches so far.\n
      Note that we purposely not declare the variable as volatile. Inside the scheduler
    there are no race conditions, all interrupt and system call handlers are serialized. */
unsigned long xsw_noContextSwitches = 0;
 
/** Count bad interrupt servicing due to problem with the INTC priority ceiling protocol. If an ISR is preempted by another
    kernel interrupt, which does a context switch than the hardware will assert the
    preempted interrupt in the new context again. When resuming, the preempted one will
    find the hardware event already serviced - this condition is counted.\n
      The earlier revisions of kernelBuilder, which attempted to implement the PCP for
    kernel interrupts of different priority, show counts for PIT0, which is running at
    lower priority and which is preempted regardless of the priority ceiling in the INTC. */
static unsigned long _noErrPit0Tif = 0
                   , _noErrPit1Tif = 0;

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
 * Implementation of system call to switch from the one to the other execution context.
 * Using only this system call but not running the timer interrupts would yield a
 * non-preemptive, cooperative scheduler.
 *   @return
 * \a int_rcIsr_switchContext: This system call always demands a context switch.
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
 * This is the only user provided argument of the system call: A value that is returned to
 * the other, resumed context as result of its system call, that had made it suspended
 * before.\n
 *   Note, that the value is not delivered if the resumed context had suspended in a timer
 * interrupt (and not in this system call). In this case, the resumed context will continue
 * where it had been pre-empted by the timer interrupt.
 *   @remark
 * Never call this function directly; it is invoked from the common IVOR #8 handler.
 */
uint32_t cxs_sc_switchContext( int_cmdContextSwitch_t *pCmdContextSwitch
                             , uint32_t signalToResumedContext
                             )
{
    /* The result of the system call is stored in the passed data structure. */
    pCmdContextSwitch->signalToResumedContext = signalToResumedContext;

    ++ xsw_noContextSwitches;

    /* Toggle active and inactive contexts on each call. */
    if(_context1IsActive)
    {
        pCmdContextSwitch->pSuspendedContextSaveDesc = &_contextSaveDesc1;
        pCmdContextSwitch->pResumedContextSaveDesc = &_contextSaveDesc2;
    }
    else
    {
        pCmdContextSwitch->pSuspendedContextSaveDesc = &_contextSaveDesc2;
        pCmdContextSwitch->pResumedContextSaveDesc = &_contextSaveDesc1;
    }
    _context1IsActive = !_context1IsActive;

    /* This system call will always provoke a context switch. Simply return true. */
    return int_rcIsr_switchContext;

} /* End of cxs_sc_switchContext */



/**
 * This is the demo implementation of a typical synchonization call between different
 * competing contexts. A counting variable that behaves as a semaphore is tested and
 * decremented if and only if the value is found to be positive. The operation is the
 * counterpart to xsw_sc_increment().\n
 *   This simple function is synchronizing as it is implemented as a system call executed
 * in the race condition free scheduler context.
 *   @return
 * The function returns \a int_rcIsr_doNotSwitchContext if it returns to the calling context
 * and \a int_rcIsr_switchContext if it demands the switch to another context.\n
 *   This system call never switches to another context. It always returns \a
 * int_rcIsr_doNotSwitchContext.
 *   @param pCmdContextSwitch
 * Interface with the assembler code that implements the IVOR #8 handler.\n
 *   If the system call returns to a context, which had suspended in a system call (this
 * one or another one) then it can put the value to be returned to that context in \a
 * *pCmdContextSwitch.\n
 *   If the function returns \a true to request a context switch then it'll write
 * references to the descriptors of the suspended and resumed contexts into the same data
 * structure.\n
 *   This system call never requests a context switch and returns its result always to the
 * calling context. The result is (uint32_t)-1 if it could not decrement the semaphore
 * variable, i.e. if it already had value 0, and the new, decremented value otherwise.
 *   @param idxSem
 * The index of the queried semaphore variable.
 *   @remark
 * Never call this function directly; it is invoked from the common IVOR #8 handler only.
 */
uint32_t xsw_sc_testAndDecrement( int_cmdContextSwitch_t *pCmdContextSwitch
                                , unsigned int idxSem
                                )
{
    /* Check consistency between C data types and assembly code. */
    INT_STATIC_ASSERT_INTERFACE_CONSISTENCY_C2AS
    
    uint32_t ret;
    if(idxSem < NO_SEMAPHORES)
    {
        if(_semaphoreAry[idxSem] > 0)
            ret = -- _semaphoreAry[idxSem];
        else
            ret = (uint32_t)-1;
    }    
    else
        ret = (uint32_t)-1;

    /* The result of the system call is stored in the passed data structure. */
    pCmdContextSwitch->signalToResumedContext = ret;
    /* pCmdContextSwitch->pSuspendedContextSaveDesc doesn't care */
    /* pCmdContextSwitch->pResumedContextSaveDesc doesn't care */

    /* This system call will never provoke a context switch. */
    return int_rcIsr_doNotSwitchContext;

} /* End of xsw_sc_testAndDecrement */



/**
 * This is the demo implementation of a typical synchonization call between different
 * competing contexts. A counting variable that behaves as a semaphore is incremented. The
 * operation is the counterpart to xsw_sc_testAndDecrement().\n
 *   This simple function is synchonizing as it is implemented as a system call executed in
 * the race condition free scheduler context.
 *   @return
 * The function returns \a int_rcIsr_doNotSwitchContext if it returns to the calling context
 * and \a int_rcIsr_switchContext if it demands the switch to another context.\n
 *   This system call never switches to another context. It always returns \a
 * int_rcIsr_doNotSwitchContext.
 *   @param pCmdContextSwitch
 * Interface with the assembler code that implements the IVOR #8 handler.\n
 *   If the system call returns to a context, which had suspended in a system call (this
 * one or another one) then it can put the value to be returned to that context in \a
 * *pCmdContextSwitch.\n
 *   If the function returns \a true to request a context switch then it'll write
 * references to the descriptors of the suspended and resumed contexts into the same data
 * structure.\n
 *   This system call never requests a context switch and returns its result always to the
 * calling context. The result is the value of the semaphore variable after increment.
 *   @param idxSem
 * The index of the affected semaphore variable.
 *   @remark
 * Never call this function directly; it is invoked from the common IVOR #8 handler only.
 */
uint32_t xsw_sc_increment(int_cmdContextSwitch_t *pCmdContextSwitch, unsigned int idxSem)
{
    uint32_t ret;
    if(idxSem < NO_SEMAPHORES)
        ret = ++ _semaphoreAry[idxSem];
    else
        ret = (uint32_t)-1;

    /* The result of the system call is stored in the passed data structure. */
    pCmdContextSwitch->signalToResumedContext = ret;
    /* pCmdContextSwitch->pSuspendedContextSaveDesc doesn't care */
    /* pCmdContextSwitch->pResumedContextSaveDesc doesn't care */

    /* This system call will never provoke a context switch. */
    return int_rcIsr_doNotSwitchContext;

} /* End of xsw_sc_increment */


/* Context switches need to be possible at any code location - this includes from within a
   sub-routine. */
static float subRoutineOf2ndCtx(float a, float b)
{
    volatile float x = 2.0f * a
                 , y = 0.5f * b;

    static uint32_t signalCtx2To1_ = 11;
    const uint32_t signalCtx1To2 ATTRIB_UNUSED =
                    sc_switchContext(/* signal */ signalCtx2To1_);
    signalCtx2To1_ += 11;

    volatile float z = x * y;
    return z;

} /* End of subRoutineOf2ndCtx. */



/* The other context we want to switch to. Will later be another task in the RTOS. */
static _Noreturn uint32_t secondContext(uint32_t taskParam)
{
    uint32_t u = taskParam;

    unsigned int cntLoops = 500;
    for(;;)
    {
        if(++cntLoops >= 1009)
        {
            cntLoops = 0;

            iprintf("Be in new context! Initial \"task parameter\" + loops: %lu\r\n", u);

            /* Access to the LED is under control of semaphore 0. Don't blink if we don't own
               the LED. */
            static bool ownLED_ = false;
            if(!ownLED_)
            {
                /* Try to acquire the LED. */
                ownLED_ = sc_testAndDecrement(/*idxSem*/ 0) != (uint32_t)-1;
            }
            if(ownLED_)
            {
                blink(3);

                /* Release access to the LED. */
#ifdef DEBUG
                uint32_t newCount =
#endif
                sc_increment(/*idxSem*/ 0);
                assert(newCount == 1);
                ownLED_ = false;
            }
            else
            {
                /* Never loop to fast to read the printf statements */
                del_delayMicroseconds(/* tiCpuInUs */ 1000000);
            }
        }

        /* Switch back to where we came from. */
        static uint32_t signalCtx2To1_ = 0;
        uint32_t signalCtx1To2 ATTRIB_UNUSED = sc_switchContext(/* signal */ signalCtx2To1_);
        ++ signalCtx2To1_;
//        iprintf( "Back in context 2 from context switch, received signal %d\r\n"
//               , (int)signalCtx1To2
//               );

        /* Try a context switch from within a sub-routine and have some self-test code */
        static volatile float a_ = 1.0f
                            , b_ = 1.0f;
        static signed long au_ = 1
                         , bu_ = 1;
        float c = subRoutineOf2ndCtx(a_, b_);
        assert((signed long)(c + 0.5) == au_*bu_);
        a_ = a_ + 1.0f;
        b_ = b_ + 2.0f;
        au_ = au_ + 1;
        bu_ = bu_ + 2;
        if(bu_ > 4000)
        {
            /* Reset test numbers to avoid assertion because of loss of integer accuracy of
               floating point operations. */
            a_ = 1.0f;
            b_ = 1.0f;
            au_ = 1;
            bu_ = 1;
        }

        ++ u;
    }
} /* End of secondContext */




/**
 * Each call of this function cyclically increments the system time of the kernel by one.\n
 *   Incrementing the system timer is an important system event. The routine will always
 * include an inspection of all suspended tasks, whether they could become due again.\n
 *   The unit of the time is defined only by the it triggering source and doesn't matter at
 * all for the kernel. The time even don't need to be regular.
 *   @return
 * The function returns \a int_rcIsr_doNotSwitchContext if it returns to the calling context
 * and \a int_rcIsr_switchContext if it demands the switch to another context.
 *   @remark
 * The function needs to be called by an interrupt and can easily end with a context change,
 * i.e. the interrupt will return to another task as the one it had interrupted.
 *   @remark
 * The connected interrupt is defined by macro #RTOS_ISR_SYSTEM_TIMER_TICK. This interrupt
 * needs to be disabled/enabled by the implementation of \a enterCriticalSection and \a
 * leaveCriticalSection.
 *   @see bool onTimerTick(void)
 *   @see #rtos_enterCriticalSection
 */
static uint32_t isrSystemTimerTick1(int_cmdContextSwitch_t *pCmdContextSwitch)
{
    /* Acknowledge the timer interrupt in the causing HW device. */
    if(PIT.TFLG0.B.TIF != 0x1)
        ++ _noErrPit0Tif;
    PIT.TFLG0.B.TIF = 0x1;

    /* Switch task every few Milliseconds. */
    static unsigned cnt_ = 0;
    bool switchCtxt = ++cnt_ == 2;
    if(switchCtxt)
    {
        cnt_ = 0;

        ++ xsw_noContextSwitches;

        /* Command the desired context switch. */

        /* The return value set here may be lost; we don't really know if the currently
           suspended context had been suspended by system call or by timer ISR. In the
           latter case setting the value has no effect. */
        pCmdContextSwitch->signalToResumedContext = UINT32_MAX;

        /* Toggle active and inactive contexts on each call. */
        if(_context1IsActive)
        {
            pCmdContextSwitch->pSuspendedContextSaveDesc = &_contextSaveDesc1;
            pCmdContextSwitch->pResumedContextSaveDesc = &_contextSaveDesc2;
        }
        else
        {
            pCmdContextSwitch->pSuspendedContextSaveDesc = &_contextSaveDesc2;
            pCmdContextSwitch->pResumedContextSaveDesc = &_contextSaveDesc1;
        }
        _context1IsActive = !_context1IsActive;

        return int_rcIsr_switchContext;
    }
    else
        return int_rcIsr_doNotSwitchContext;

} /* End of 1st ISR to increment the system time by one tick. */



/**
 * Start the interrupt which clocks the system time. Timer 2 is used as interrupt source
 * with a period time of about 2 ms or a frequency of 490.1961 Hz respectively.\n
 *   This is the default implementation of the routine, which can be overloaded by the
 * application code if another interrupt or other interrupt settings should be used.
 */
static void enableIRQTimerTick1(void)
{
    /* Disable all PIT timers during configuration. */
    PIT.PITMCR.R = 0x2;

    /* Install the interrupt handler for cyclic timer PIT 0. It drives the OS scheduler for
       cyclic task activation. */
    ihw_installINTCInterruptHandler( (int_externalInterruptHandler_t)
                                                {.kernelIsr = &isrSystemTimerTick1}
                                   , /* vectorNum */ 59 /* Timer PIT 0 */
                                   , /* psrPriority */ 1
                                   , /* isPreemptable */ true
                                   , /* isOsInterrupt */ true
                                   );

    /* Peripheral clock has been initialized to 120 MHz. To get a 1ms interrupt tick we
       need to count till 120000.
         -1: See MCU reference manual, 36.5.1, p. 1157. */
    PIT.LDVAL0.R = 120000-1; /* Interrupt rate 1ms */

    /* Enable interrupts by this timer and start it. */
    PIT.TCTRL0.R = 0x3;

    /* Enable timer operation and let them be stopped on debugger entry. Note, this is a
       global setting for all four timers, even if we use and reserve only one for the
       RTOS. */
    PIT.PITMCR.R = 0x1;

} /* End of enableIRQTimerTick1 */




/**
 * Each call of this function produces a context switch. The action is nearly identical to
 * isrSystemTimerTick but the interrupt has another rate and another priority. It is meant
 * a test that all kernel relevant interrupts are serialized and do not harmfully interfere
 * with one another.\n
 *   This function is nearly a copy of isrSystemTimerTick1. It proves that unrelated ISRs
 * can independently take decisions for context switches. The tick rates of both interrupts
 * are chosen mutually prime such that all possible phase relations will occur.
 *   @return
 * The function returns \a int_rcIsr_doNotSwitchContext if it returns to the calling context
 * and \a int_rcIsr_switchContext if it demands the switch to another context.
 */
static uint32_t isrSystemTimerTick2(int_cmdContextSwitch_t *pCmdContextSwitch)
{
    assert(INTC.CPR_PRC0.R == 1);

    /* Acknowledge the timer interrupt in the causing HW device. */
    if(PIT.TFLG1.B.TIF != 0x1)
        ++ _noErrPit1Tif;
    PIT.TFLG1.B.TIF = 0x1;

    /* Switch task every few Milliseconds. */
    static unsigned cnt_ = 0;
    bool switchCtxt = ++cnt_ == 2;
    if(switchCtxt)
    {
        cnt_ = 0;

        ++ xsw_noContextSwitches;

        /* Command the desired context switch. */

        /* The return value set here may be lost; we don't really know if the currently
           suspended context had been suspended by system call or by timer ISR. In the
           latter case setting the value has no effect. */
        pCmdContextSwitch->signalToResumedContext = UINT32_MAX;

        /* Toggle active and inactive contexts on each call. */
        if(_context1IsActive)
        {
            pCmdContextSwitch->pSuspendedContextSaveDesc = &_contextSaveDesc1;
            pCmdContextSwitch->pResumedContextSaveDesc = &_contextSaveDesc2;
        }
        else
        {
            pCmdContextSwitch->pSuspendedContextSaveDesc = &_contextSaveDesc2;
            pCmdContextSwitch->pResumedContextSaveDesc = &_contextSaveDesc1;
        }
        _context1IsActive = !_context1IsActive;

        return int_rcIsr_switchContext;
    }
    else
        return int_rcIsr_doNotSwitchContext;

} /* End of 2nd ISR to increment the system time by one tick. */



/**
 * Start the interrupt which clocks the second context switching ISR.
 */
static void enableIRQTimerTick2(void)
{
    /* Disable all PIT timers during configuration. */
    PIT.PITMCR.R = 0x2;

    /* Install the interrupt handler for cyclic timer PIT 0. It drives the OS scheduler for
       cyclic task activation. */
    ihw_installINTCInterruptHandler( (int_externalInterruptHandler_t)
                                                {.kernelIsr = &isrSystemTimerTick2}
                                   , /* vectorNum */ 60 /* PITimer Channel 1 */
                                   , /* psrPriority */ 1
                                   , /* isPreemptable */ true
                                   , /* isOsInterrupt */ true
                                   );

    /* Peripheral clock has been initialized to 120 MHz. To get a 997us interrupt tick we
       need to count till 119640.
         -1: See MCU reference manual, 36.5.1, p. 1157. */
    PIT.LDVAL1.R = 119640-1; /* Interrupt rate slightly above 1ms */

    /* Enable interrupts by this timer and start it. */
    PIT.TCTRL1.R = 0x3;

    /* Enable timer operation and let them be stopped on debugger entry. Note, this is a
       global setting for all four timers, even if we use and reserve only one for the
       RTOS. */
    PIT.PITMCR.R = 0x1;

} /* End of enableIRQTimerTick2 */




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
 * Start the interrupt which clocks the system time. Timer 2 is used as interrupt source
 * with a period time of about 2 ms or a frequency of 490.1961 Hz respectively.\n
 *   This is the default implementation of the routine, which can be overloaded by the
 * application code if another interrupt or other interrupt settings should be used.
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







// The loop routine runs over and over again forever:
void xsw_loop()
{
    unsigned int u;

    fputs("Start\r\n", stdout);

    /* First context is running on entry and the stored values must not matter. */
    fputs("Prepare first context\r\n", stdout);
#if INT_USE_SHARED_STACKS == 1
    _contextSaveDesc1.ppStack = &_contextSaveDesc1.pStack;
    _contextSaveDesc1.pStackOnEntry = NULL;
#endif
    _contextSaveDesc1.pStack = NULL;
    _contextSaveDesc1.idxSysCall = -1;
    _contextSaveDesc1.fctEntryIntoContext = NULL; /* Actually not used */
    _contextSaveDesc1.privilegedMode = true;         /* Actually not used */
    _context1IsActive = true;

    /* As a compile time option, the second context defined here in this module can be
       replaced by a function implemented in assembly code, which does nothing apparently
       but which is designed to check the correct save/restore of the CPU context. Most of
       the registers are set to test patterns and double-checked later. 
         Prefill stack memory to make stack usage observable. */
    fputs("Prepare second context\r\n", stdout);
    memset(&_stack2ndCtxt[0], 0xa5, sizeof(_stack2ndCtxt));
    ccx_createContext( &_contextSaveDesc2
                     , /* stackPointer */ (uint8_t*)&_stack2ndCtxt[0] + sizeof(_stack2ndCtxt)
#if 1
                     , /* fctEntryIntoContext */ &secondContext
#else
                     , /* fctEntryIntoContext */ &tcx_testContext
#endif
                     , /* privilegedMode */ true
                     );

    iprintf("New context, initial stack pointer: %p\r\n", _contextSaveDesc2.pStack);
    u = sizeOfAry(_stack2ndCtxt);
    while(u > 0)
    {
        -- u;

        if(u%4 == 3)
        {
            del_delayMicroseconds(/* tiCpuInUs */ 10000);
            iprintf( "\r\n%2u .. %2u, %p .. %p: "
                   , u, u-3, &_stack2ndCtxt[u], &_stack2ndCtxt[u-3]
                   );
        }
        iprintf("%08lx ", _stack2ndCtxt[u]);
    }
    fputs("\r\n", stdout);

    fputs("Install timer interrupts\r\n", stdout);
    enableIRQTimerTick1();  /* Timer for context switches */
    enableIRQTimerTick2();  /* 2nd Timer for context switches */
    enableIRQPit2();        /* Simple IRQ of high priority for stress test */

    unsigned int cntLoops = 0;
    for(;;)
    {
        /* The Serial.print method is blocking. It returns when everything is finished, or
           when an error has been seen. Therefore switching the context immediately after a
           print is uncritical, regardless if the other context will do some print's also. */
        static uint32_t signalCtx2To1_ = 100;

        uint32_t signalCtx2To1 ATTRIB_UNUSED = sc_switchContext(/* signal */ signalCtx2To1_);
        signalCtx2To1_ += 5;
//        iprintf( "Back in context 1 from context switch, received signal %d\r\n"
//               , (int)signalCtx2To1
//               );

        /* Access to the LED is under control of semaphore 0. Don't blink if we don't own
           the LED. */
        if(++cntLoops >= 1009)
        {
            cntLoops = 0;

            static bool ownLED_ = false;
            if(!ownLED_)
            {
                /* Try to acquire the LED. */
                ownLED_ = sc_testAndDecrement(/*idxSem*/ 0) != (uint32_t)-1;
            }
            if(ownLED_)
            {
                blink(2);

                /* Release access to the LED. */
#ifdef DEBUG
                uint32_t newCount =
#endif
                sc_increment(/*idxSem*/ 0);
                assert(newCount == 1);
                ownLED_ = false;
            }
            else
            {
                /* Never loop to fast to read the printf statements */
                del_delayMicroseconds(/* tiCpuInUs */ 1000000);
            }

            unsigned long noCtxSw = (volatile unsigned long)xsw_noContextSwitches;
            static unsigned long noCtxSwPrintf_ = 0;
            iprintf( "No successful test loops: %lu, PIT2: %lu, no. context switches: %lu."
                     " (The execution of this printf has been interrupted by %lu"
                     " context switches.)\r\n"
                   , tcx_cntTestLoops
                   , xsw_cntIsrPit2
                   , xsw_noContextSwitches
                   , noCtxSwPrintf_
                   );
            noCtxSwPrintf_ = (volatile unsigned long)xsw_noContextSwitches - noCtxSw;
        }
    }
} /* End of xsw_loop */


