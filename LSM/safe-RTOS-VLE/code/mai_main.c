/**
 * @file mai_main.c
 *   The main entry point of the C code. The assembler implemented startup code has been
 * executed and brought the MCU in a preliminary working state, such that the C compiler
 * constructs can safely work (e.g. stack pointer is initialized, memory access through MMU
 * is enabled). After that it branches here, into the C entry point main().\n
 *  The first operation of the main function is the call of the remaining hardware
 * initialization ihw_initMcuCoreHW() that is still needed to bring the MCU in basic stable
 * working state. The main difference to the preliminary working state of the assembler
 * startup code is the selection of appropriate clock rates. Furthermore, the interrupt
 * controller is configured. This part of the hardware configuration is widely application
 * independent. The only reason, why this code has not been called from the assembler code
 * prior to entry into main() is code transparency. It would mean to have a lot of C
 * code without an obvious point, where it is used.\n
 *   In this most basic sample the main function implements the standard Hello World
 * program of the Embedded Software World, the blinking LED.\n
 *   The main function configures the application dependent hardware, which is a cyclic
 * timer (Programmable Interrupt Timer 0, PIT 0) with cycle time 1ms. An interrupt handler
 * for this timer is registered at the Interrupt Controller (INTC). A second interrupt
 * handler is registered for software interrupt 3. Last but not least the LED outputs and
 * button inputs of the TRK-USB-MPC5643L are initialized.\n
 *   The code enters an infinite loop and counts the cycles. Any 500000 cycles it triggers
 * the software interrupt.\n
 *   Both interrupt handlers control one of the LEDs. LED 4 is toggled every 500ms by the
 * cyclic timer interrupt. We get a blink frequency of 1 Hz.\n 
 *   The software interrupt toggles LED 5 every other time it is raised. This leads to a
 * blinking of irrelated frequency.\n
 *   The buttons are unfortunately connected to GPIO inputs, which are not interrupt
 * enabled. We use the timer interrupt handler to poll the status. On button press of
 * Switch 3 the colors of the LEDs are toggled.
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
 *   main
 * Local functions
 *   testFloatingPointConfiguration
 *   interruptSW3Handler
 *   SW3UserTask
 *   subRoutineWithTaskTermination
 *   interruptPIT0Handler
 *   userTask1ms
 *   interruptPIT1Handler
 *   PIT1UserTask
 *   endOfInterruptPIT1Handler
 *   isrPit2
 *   isrPit3
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <assert.h>

#include "MPC5643L.h"
#include "ihw_initMcuCoreHW.h"
#include "prc_process.h"
#include "lbd_ledAndButtonDriver.h"
#include "mpu_systemMemoryProtectionUnit.h"
#include "gsl_systemLoad.h"
#include "rtos.h"
#include "sio_serialIO.h"
#include "mai_main.h"
#include "mai_main_defSysCalls.h"


/*
 * Defines
 */
 
 
/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
 
/*
 * Data definitions
 */
 
/** The average CPU load produced by all tasks and interrupts in tens of percent. */
unsigned int mai_cpuLoad SECTION(.sdata.P1) = 1000;

volatile unsigned long mai_cntIdle = 0    /** Counter of cycles of infinite main loop. */
                     , mai_cntIntPIT2 = 0 /** Counter of calls of PIT 2 interrupts */
                     , mai_cntIntPIT3 = 0;/** Counter of calls of PIT 3 interrupts */

volatile SECTION(.sbss.P1) unsigned long mai_cntIntPIT1 = 0;/** Count PIT 1 interrupts */
volatile SECTION(.sbss.P2) unsigned long mai_cntIntSW3 = 0  /** Count software interrupts 3 */
                                       , mai_cntIntPIT0 = 0;/** Count PIT 0 interrupts */

/** The color currently used by the interrupt handlers are controlled through selection of
    a pin. The selection is made by global variable. Here for D5. */
static volatile SECTION(.sdata.P2) lbd_led_t _ledSW3Handler  = lbd_led_D5_grn;
 
/** The color currently used by the interrupt handlers are controlled through selection of
    a pin. The selection is made by global variable. Here for D4. */
static volatile SECTION(.data.P2) lbd_led_t _ledPIT0Handler = lbd_led_D4_red;


/*
 * Function implementation
 */

/**
 * Some floating point operations in order to test the compiler configuration.
 */
static void testFloatingPointConfiguration(void)
{
    volatile float x, y = 99.0f, z;
    x = y / 3;
    x = y / 3.0f;
    x = y / 3.0;
    
    z = y / x;
    z = y * x;
    z = y + x;
    z = y - x;
    z = y + 56ul;
    
    x = 3.1415f / 4.0f;
    y = sin(x);
    y = sinf(x);
    y = cos(x);
    y = cosf(x);
    
    x = 1.0;
    y = exp(x);
    y = expf(x);
    y = log(x);
    y = logf(x);
    y = pow10(x);
    y = pow10f(x);
    
    x = 0.0f;
    y = z / x;
    y = log(x);
    y = logf(x); /* This code fails in a user process: A write into some data of C lib
                    implementation lib_a-impure.o cause an MPU exception at first address
                    of section .data.impure_data */
    x = -1.0;
    y = sqrt(x);
    y = sqrtf(x);
    
    volatile double a, b = 99.0f, c;
    a = x + z;
    a = b / 3;
    a = b / 3.0f;
    a = b / 3.0;
    
    c = b / a;
    c = b * a;
    c = b + a;
    c = b - a;
    c = b + 56ul;
    
    a = 3.1415f / 4.0f;
    b = sin(a);
    b = sinf(a);
    b = cos(a);
    b = cosf(a);
    
    a = 1.0;
    b = exp(a);
    b = expf(a);
    b = log(a);
    b = logf(a);
    b = pow10(a);
    b = pow10f(a);
    
    a = 0.0f;
    b = c / a;
    b = log(a);
    b = logf(a);
    a = -1.0;
    b = sqrt(a);
    b = sqrtf(a);
    
    /* Give us a chance to see the last result in the debugger prior to leaving scope. */
    b = 0.0;

} /* testFloatingPointConfiguration */



/**
 * A dummy function, which doesn't do much but then it tries end the task execution
 * prematurely.
 */
static void subRoutineWithTaskTermination(float n, bool printMsg)
{
    const unsigned maxCount = (unsigned)(1000.0*n + 0.5);
    
    unsigned int u;
    for(u=0; u<maxCount; ++u)
        ;
    
    if(printMsg)
    {
        #define MSG "subRoutineWithTaskTermination: Hi!\r\n"
        sio_writeSerial(MSG, /* noBytes */ sizeof(MSG)-1);
        #undef MSG
    }
    
    /* See https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html */
    asm volatile ( /* AssemblerTemplate */
                   "se_li %%r3, 0\n\t"
                   "se_stw  %%r3, 0(%%r3) /* MPU bus error */\n\t"
                   "se_sc\n\t"
                 : /* OutputOperands */
                 : /* InputOperands */
                 : /* Clobbers */ "r3", "memory"
                 );
} /* End of subRoutineWithTaskTermination */



/**
 * Interrupt handler that serves the SW interrupt 3.
 */
static void interruptSW3Handler(void)
{
    /* Acknowledge our SW interrupt 3 (test) in the causing HW device. */
    INTC.SSCIR3.R = 1<<0;
    
} /* End of interruptSW3Handler */



/**
 * First sample implementation of a system call of conformance class "full". Such a system
 * call can be implemented in C and it is run while interrupt processing is active. It'll
 * be preempted by ISRs and user tasks of higher priority. Suitable for longer running
 * services.\n
 *   Here we use the concept to implement an LED I/O driver.
 *   @return
 * The value of the argument \a isOn is returned.
 *   @param zero
 * System call functions of this conformance class get 0 as first argument.
 * Actually, this is rather a side effect of the implementation than desired behavior.
 * May be changed in the future.
 *   @param led
 * The LED to address to.
 *   @param isOn
 * Switch the selected LED either on or off.
 */
uint32_t mai_scFlHdlr_setLEDAndWait(uint32_t zero ATTRIB_UNUSED, lbd_led_t led, bool isOn)
{
    /* @todo A safe, "trusted" implementation needs to double check the selected LED in
       order to avoid undesired access to I/O ports other than the four true LED ports. */
    lbd_OS_setLED(led, isOn);
    
    /* This system call can be preempted by IRQs with higher priority. To even provoke this
       we stay for a while in this routine. */
    volatile unsigned int u;
    for(u=0; u<1000; ++u)
        ;

    return isOn;
    
} /* End of mai_scFlHdlr_setLEDAndWait */



/**
 * User notification part of interrupt service. After servicing the interrupt in the
 * primary ISR, this function is executed in another process context. The process is user
 * owned and the code executed in problem state.\n
 *   Here for the SW3 interrupt. Process is PID=1.
 */
static void SW3UserTask(void)
{
    ++ mai_cntIntSW3;

    static SECTION(.sbss.P2) int cntIsOn_;
    if(++cntIsOn_ >= 1)
        cntIsOn_ = -1;

    rtos_systemCall(MAI_SYSCALL_SET_LED_AND_WAIT, _ledSW3Handler, cntIsOn_ >= 0);
    
    /* Test use of process owned stack. */
//    testFloatingPointConfiguration(); fails to run in user process, MPU, static data */

    /* The user task can be preempted by IRQs, with higher priority, including our PIT0. To
       even provoke this we stay for a while in this routine. */
    volatile unsigned int u;
    for(u=0; u<1000; ++u)
        ;

    #define MSG "SW3UserTask: Hello World\r\n"
    sio_writeSerial(MSG, /* noBytes */ sizeof(MSG)-1);
    #undef MSG

    /* Task is terminated prematurely from subroutine, i.e. with not unwinded stack. */
    if((mai_cntIntSW3 & 2) == 0)
        subRoutineWithTaskTermination(0.1, /* printMsg */ true);
    else if((mai_cntIntSW3 & 4) == 0)
    {
        /* Try an IVOR 6 exception using privileged instruction. */
        asm volatile ( /* AssemblerTemplate */
                       "wrteei 0\n\t"
                     : /* OutputOperands */
                     : /* InputOperands */
                     : /* Clobbers */
                     );
    }
    else
    {
        /* Test: We hurt the memory separation rules by writing to a variable of the other
           process. This should terminate the task. */
        
        /* This read should be still valid. */
        const unsigned long myCopy = *(volatile unsigned long*)&mai_cntIntPIT1;
        
        /* This write should be impossible. */
        mai_cntIntPIT1 = 0; 
    }

    /* If we get here then the user Task is terminated by return. */    
    
} /* End of SW3UserTask */



#if 0
/**
 * Interrupt handler that serves the interrupt of Programmable Interrupt Timer 0.
 */
static void interruptPIT0Handler(void)
{
    /* Read the current button status to possibly toggle the LED colors. */
    static bool lastStateButton = false;
    if(lbd_getButton(lbd_bt_button_SW3))
    {
        if(!lastStateButton)
        {
            /* Button down event: Toggle colors */
            static unsigned int cntButtonPress_ = 0;
            
            lbd_setLED(_ledSW3Handler, /* isOn */ false);
            lbd_setLED(_ledPIT0Handler, /* isOn */ false);
            _ledSW3Handler  = (cntButtonPress_ & 0x1) != 0? lbd_led_D5_red: lbd_led_D5_grn;
            _ledPIT0Handler = (cntButtonPress_ & 0x2) != 0? lbd_led_D4_red: lbd_led_D4_grn;
            
            lastStateButton = true;
            ++ cntButtonPress_;
        }
    }
    else
        lastStateButton = false;
            
    /* In order to see at the LEDs whether all tasks are running we let this task observe,
       whether the other ones advance and inhibit LED blinking otherwise. */
    static unsigned long mai_cntIntPIT1_lastVal = 0
                       , mai_cntIntPIT2_lastVal = 0
                       , mai_cntIntPIT3_lastVal = 0;
    static int cntIsOn_ = 0;
    if(mai_cntIntPIT1_lastVal != mai_cntIntPIT1
       &&  mai_cntIntPIT2_lastVal != mai_cntIntPIT2
       &&  mai_cntIntPIT3_lastVal != mai_cntIntPIT3
      )
    {
        mai_cntIntPIT1_lastVal = mai_cntIntPIT1;
        if(++cntIsOn_ >= 500)
            cntIsOn_ = -500;
    }
    lbd_setLED(_ledPIT0Handler, /* isOn */ cntIsOn_ >= 0);
    
} /* End of interruptPIT0Handler */
#endif



/**
 * User notification part of interrupt service. After servicing the interrupt in the
 * primary ISR, this function is executed in another process context. The process is user
 * owned and the code executed in problem state.\n
 *   Here for the PIT0 interrupt. Process is PID=2.
 */
static void userTask1ms(void)
{
    ++ mai_cntIntPIT0;

    /* The user task can be preempted by IRQs of higher priority. To even provoke this we
       stay for a while in this routine. */
    volatile unsigned int u;
    for(u=0; u<1000; ++u)
        ;

    /* In order to see at the LEDs whether all tasks are running we let this task observe,
       whether the other ones advance and inhibit LED blinking otherwise. */
    static unsigned long mai_cntIntPIT1_lastVal_ SECTION(.sdata.P2.mai_cntIntPIT1_lastVal_) = 0
                       , mai_cntIntPIT2_lastVal_ SECTION(.sdata.P2.mai_cntIntPIT2_lastVal_) = 0
                       , mai_cntIntPIT3_lastVal_ SECTION(.sdata.P2.mai_cntIntPIT3_lastVal_) = 0;
    static int cntIsOn_ SECTION(.sdata.P2.cntIsOn_) = 0;
    if(mai_cntIntPIT1_lastVal_ != mai_cntIntPIT1
       &&  mai_cntIntPIT2_lastVal_ != mai_cntIntPIT2
       &&  mai_cntIntPIT3_lastVal_ != mai_cntIntPIT3
      )
    {
        mai_cntIntPIT1_lastVal_ = mai_cntIntPIT1;
        mai_cntIntPIT2_lastVal_ = mai_cntIntPIT2;
        mai_cntIntPIT3_lastVal_ = mai_cntIntPIT3;
        if(++cntIsOn_ >= 500)
            cntIsOn_ = -500;
    }
    
    uint32_t priorityOld = rtos_suspendAllInterruptsByPriority(/* suspendUpToThisPrio */ 7);
    lbd_setLED(_ledPIT0Handler, /* isOn */ cntIsOn_ >= 0);
    rtos_resumeAllInterruptsByPriority(/* resumeDownToThisPriority */ priorityOld);

#if 0
    /* Test of function abort for a sub-routine. */
    subRoutineWithTaskTermination(3.7f);
    
    /* We should never get here. */
    assert(false);
#endif    
} /* End of userTask1ms */




/**
 * Interrupt handler that serves the interrupt of Programmable Interrupt Timer 1.
 */
static void interruptPIT1Handler(void)
{
    /* Currently not used. */
    assert(false);    
    
} /* End of interruptPIT1Handler */



/**
 * User notification part of interrupt service. After servicing the interrupt in the
 * primary ISR, this function is executed in another process context. The process is user
 * owned and the code executed in problem state.\n
 *   Here for the PIT1 interrupt. Process is PID=1.
 */
static void PIT1UserTask(void)
{
    ++ mai_cntIntPIT1;

    /* The user task can be preempted by IRQs of higher priority. To even provoke this we
       stay for a while in this routine. */
    volatile unsigned int u;
    for(u=0; u<1000; ++u)
        ;
    
    #define P1_STATIC(var)  static SECTION(.data.P1.var) var
    unsigned int P1_STATIC(noSoOften) = 0;
    if(++noSoOften >= 2000)
    {
        iprintf("PIT1UserTask: CPU load: %u%%\r\n", mai_cpuLoad/10);
        noSoOften = 0;
    }
    #undef P1_STATIC

    /* Task is terminated prematurely from subroutine, i.e. with not unwinded stack. */
    if((mai_cntIntPIT1 & 1) == 0)
        subRoutineWithTaskTermination(0.2, /* printMsg */ (mai_cntIntPIT1 & 0x1ffu) == 0);

    /* If we get here then the user Task is terminated by return. */    

} /* End of PIT1UserTask */




/**
 * Secondary interrupt handler that acknowledges the interrupt at the I/O device after
 * having executed the user code notification callback. Here for Programmable Interrupt
 * Timer 1.
 */
static void endOfInterruptPIT1Handler(void)
{
    /* Acknowledge the interrupt in the causing HW device. */
    PIT.TFLG1.B.TIF = 0x1;
    
} /* End of endOfInterruptPIT1Handler */



/**
 * A regularly triggered interrupt handler for the timer PIT2. The interrupt does nothing
 * but counting a variable. It is triggered at high frequency and asynchronously to the
 * kernel's clock tick to prove the system stability and properness of the context switches.
 */
static void isrPit2()
{
    ++ mai_cntIntPIT2;
    
    /* Acknowledge the interrupt in the causing HW device. */
    PIT.TFLG2.B.TIF = 0x1;

} /* End of isrPit2 */



/**
 * A regularly triggered interrupt handler for the timer PIT3. The interrupt does nothing
 * but counting a variable. It is triggered at high frequency and asynchronously to the
 * kernel's clock tick to prove the system stability and properness of the context switches.
 */
static void isrPit3()
{
    ++ mai_cntIntPIT3;
    
    /* Acknowledge the interrupt in the causing HW device. */
    PIT.TFLG3.B.TIF = 0x1;

} /* End of isrPit3 */



/**
 * Entry point into C code. The C main function is entered without arguments and despite of
 * its return code definition it must never be left. (Returning from main would enter an
 * infinite loop in the calling assembler startup code.)
 */
void main()
{
    /* Init core HW of MCU so that it can be safely operated. */
    ihw_initMcuCoreHW();
    prc_initINTCInterruptController();

    /* Install the interrupt handler for SW interrupt 3 (for test only) */
    assert(((uintptr_t)interruptSW3Handler & 0x80000000) == 0);
    prc_interruptServiceDesc_t interruptSW3ServiceDesc = 
                                    { .isr = PRC_ISD_OS_HANDLER( interruptSW3Handler
                                                               , /* isPreemptable */ true
                                                               )
                                    , .userTask = SW3UserTask
                                    , .tiTaskMax = 60000
                                    , .taskTerminationCondition = 0
                                    , .endOfIrq = NULL
                                    , .PID = 2
                                    };
    prc_installINTCInterruptHandler( &interruptSW3ServiceDesc
                                   , /* vectorNum */ 3
                                   , /* psrPriority */ 1
                                   );
                                   
    /* Disable timers during configuration. */
    PIT.PITMCR.R = 0x2;

    /* Install the interrupt handler for cyclic timer PIT 1 (for test only) */
    assert(((uintptr_t)interruptPIT1Handler & 0x80000000) == 0);
    prc_interruptServiceDesc_t interruptPIT1ServiceDesc =
                                    { .isr = PRC_ISD_OS_HANDLER( NULL
                                                               , /* isPreemptable */ false
                                                               )
                                    , .userTask = PIT1UserTask
                                    , .tiTaskMax = 25000
                                    , .taskTerminationCondition = 0
                                    , .endOfIrq = 
                                        (void (*)(void))
                                        ((uintptr_t)endOfInterruptPIT1Handler
                                         | 0x80000000ul
                                        )
                                    , .PID = 1
                                    };
    prc_installINTCInterruptHandler( &interruptPIT1ServiceDesc
                                   , /* vectorNum */ 60
                                   , /* psrPriority */ 2
                                   );

    /* Install the interrupt handlers for cyclic timers PIT 2 and 3. They do actually
       nothing than pre-empting the others at high frequency and at asynchronous rates.
       They are just meant for testing: They produce load and context switches at arbitrary
       code locations. */
    const prc_interruptServiceDesc_t interruptPIT2ServiceDesc =
                                     { .isr = PRC_ISD_OS_HANDLER( isrPit2
                                                                , /* isPreemptable */ true
                                                                )
                                       , .userTask = NULL
                                       , .tiTaskMax = 0
                                       , .taskTerminationCondition = 0
                                       , .endOfIrq = NULL
                                       , .PID = 0 /* doesn't care if userTask == NULL */
                                     };
    prc_installINTCInterruptHandler( &interruptPIT2ServiceDesc
                                   , /* vectorNum */ 61
                                   , /* psrPriority */ 6
                                   );
    const prc_interruptServiceDesc_t interruptPIT3ServiceDesc = 
                                     { .isr = PRC_ISD_OS_HANDLER( isrPit3
                                                                , /* isPreemptable */ false
                                                                )
                                       , .userTask = NULL
                                       , .tiTaskMax = 0
                                       , .taskTerminationCondition = 0
                                       , .endOfIrq = NULL
                                       , .PID = 0 /* doesn't care if userTask == NULL */
                                     };
    prc_installINTCInterruptHandler( &interruptPIT3ServiceDesc
                                   , /* vectorNum */ 127
                                   , /* psrPriority */ 15
                                   );
                                   
    /* Initialize the button and LED driver for the eval board. */
    lbd_initLEDAndButtonDriver();
    
    /* Peripheral clock has been initialized to 120 MHz. To get a 1ms interrupt tick we
       need to count till 120000. The timer cycle times are chosen mutually prime in order
       to lead to most varying, always different phase relations between all affected ISR
       calls and context switches.
         -1: See MCU reference manual, 36.5.1, p. 1157. */
    PIT.LDVAL1.R = 120011-1;/* A prime number close to the other time. */
    PIT.LDVAL2.R = 4001-1;  /* Interrupt rate approx. 30kHz */
    PIT.LDVAL3.R = 3989-1;  /* Interrupt rate approx. 30kHz */
    
    /* Enable interrupts by this timer and start it. */
    PIT.TCTRL1.R  = 0x3; 
    PIT.TCTRL2.R  = 0x3; 
    PIT.TCTRL3.R  = 0x3; 

    /* Enable timer operation and let them be stopped on debugger entry. */
    PIT.PITMCR.R = 0x1;

    /* Run the RTOS kernel with a cyclic user task. */
    const rtos_taskDesc_t task1msDesc = 
        { .PID = 2
          , .taskFct = userTask1ms
          , .tiCycleInMs = 1
          , .tiTaskMaxInUS = 500000
          , .tiFirstActivationInMs = 17
          , .priority = 3
        };
    const unsigned int idTask1ms = rtos_registerTask(&task1msDesc);
    assert(idTask1ms == 0);

    /* Arm the memory protection unit. */
    mpu_initMPU();

    /* Initialize the serial output channel as prerequisite of using printf. */
    sio_initSerialInterface(/* baudRate */ 115200);

    /* Start the scheduler of the RTOS. */
    rtos_initKernel();
    
    /* The external interrupts are enabled after configuring I/O devices and registering
       the interrupt handlers. */
    ihw_resumeAllInterrupts();
    
    /* Call test of floating point configuration. (Only useful with a connected debugger.) */
    testFloatingPointConfiguration();
    
    while(true)
    {
        ++ mai_cntIdle;
        if((mai_cntIdle % 500000) == 0)
        {
            /* Request SW interrupt 3 (test) */
            INTC.SSCIR3.R = 1<<1;
        }
        
        /* Note, measurement of CPU load does not return for about 1.5s. The SW IRQ is not
           triggered for a while. That means that the CPU load caused by the SW IRQ is not
           counted in our measurement. Due to its load rate this is negligible. */           
        if((mai_cntIdle % 10000000) == 0)
        {
            mai_cpuLoad = gsl_getSystemLoad();
//            iprintf("CPU load: %u%%\r\n", (mai_cpuLoad+5)/10);
        }
    }
} /* End of main */
