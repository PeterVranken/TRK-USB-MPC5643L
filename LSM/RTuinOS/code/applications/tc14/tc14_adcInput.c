/**
 * @file tc14_adcInput.cpp
 *   Test case 14 of RTuinOS. (Find details of the e200z4 port for the TRK-USB-MPC5643L
 * board below.) A user interrupt is applied to pick the results of an analog input
 * channel, which is running in regular, hardware triggered Auto Trigger Mode.\n
 *   It could seem to be straight forward, to use the timing capabilities of an RTOS to
 * trigger the conversions of an ADC; a regular task would be used to do so. However,
 * signal processing of fluctuating input signals by means of regularly sampling the input
 * suffers from incorrect timing. Although the timing of a regular task is very precise in
 * mean, the actual points in time, when a task is invoked are not precisely equidistant.
 * The invocations may be delayed by an arbitrary, fluctuating tiny time span. This holds
 * true even for a task of high priority - although the so called jitter will be little
 * here. If the signal processing assumes regular sampling of the input but actually does
 * do this with small time shifts, it will see an error, which is approximately equal to
 * the first derivative of the input signal times the time shift. The latter is a random
 * quantity so the error also is a random quantity proportional to the derivative of the
 * input signal. In the frequency domain this mean that the expected error increases
 * linearly with the input frequency. Consequently, task triggered ADC conversions must be
 * used only for slowly changing input signals, it might e.g. be adequate for reading a
 * temperature input. All other applications need to trigger the conversions by a software
 * independent, accurate hardware signal. The software becomes a slave of this hardware
 * trigger. The jitter of the task now only doing the data evaluation doesn't matter at
 * all.\n
 *   This RTuinOS sample application uses timer/counter 0 in the unchanged Arduino standard
 * configuration to trigger the conversions of the ADC. The overflow interrupt is used for
 * this purpose yielding a conversion rate of about 977 Hz. A task of high priority is
 * awaken on each conversion-complete event and reads the conversion result. The read
 * values are down-sampled and passed to a much slower secondary task, which prints them on
 * the Arduino LCD shield (using the LiquidCrystal library).\n
 *   Proper down-sampling is a CPU time consuming operation, which is hard to implement on
 * a tiny eight Bit controller. Here we use the easiest possible to implement filter with
 * rectangular impulse response. It adds the last recent N input values and divides the
 * result by N. We exploit the fact, that we have 10 Bit ADC values but use a 16 Bit
 * arithmetics anyway: We can safely sum up up to 64 values without any danger of overflow.
 * The division by N=64 is not necessary at all; this constant value just changes the
 * scaling of the result (i.e. the scaling binary value to Volt), which has to be
 * considered for any output operation anyway. It doesn't matter to this "consider" which
 * scaling we actually have, it's just another constant to use.\n
 *   What do you need? What do you get?\n
 * To run this sample you need an Arduino Mega board with the LCD shield connected. Porting
 * this sample to one of the tiny AVRs will be difficult as it requires about 3kByte of RAM
 * and 22 kByte of ROM (in DEBUG configuration). Furthermore, all the 16 ADC inputs are
 * addressed, so functional code modifications would become necessary, too. The sample can
 * be run without the LCD shield as it prints a lot of information to the Arduino console
 * window also (in DEBUG configuration only). The function is as follows: The LCD shield
 * buttons left/right switch to the previous/next ADC input. The internal band gap voltage
 * reference can also be selected as input. The voltage measured at the selected input is
 * continuously displayed on the LCD. Another area of the display displays the current
 * time. (The clock can be adjusted with the buttons up/down.) The last display area shows
 * the current CPU load. All of these areas are continuously updated asynchronously to one
 * another by different tasks.\n
 *   This test case demonstrates the following things:\n
 * *) The use of a non multi-threading library in a multi-threading environment. The display
 * is purposely accessed by different tasks, which are asynchronous to one another. To do
 * so, the display has been associated with a mutex and each display writing task will
 * acquire the mutex first. All of this has been encapsulated in the class dpy_display_t and
 * all a task needs to do is calling a simple function printXXX. (Please find more detailed
 * considerations about the use of library LiquidCrystal in the RTuinOS manual.)\n
 * *) The the input voltage displaying task (taskDisplayVoltage) is regular but not by an
 * RTOS timer operation as usual but because it is associated with the ADC conversion
 * complete interrupt (which is purposely triggered by a regular hardware event). So this
 * part of the application is synchronous to an external event, whereas a concurrent task
 * (taskRTC) is an asynchronous regular task by means of RTuinOS timer operations. Both of
 * these tasks compete for the display without harmful side effects. (The regular timer
 * task implements a real time clock, see clk_clock.cpp.)\n
 * *) A user interface task scans the buttons, which are mounted on the LCD shield. It
 * decodes the buttons and dispatches the information to the different tasks, which are
 * controlled by the buttons. This part of the code demonstrates how to implement safe
 * inter-task interfaces, mainly built on broadcasted events and critical sections in
 * conjunction with volatile data objects. The interfaces are implemented in both styles,
 * by global, shared data or as functional interface. Priority considerations avoid having
 * superfluous access synchronization code. See code comments for more.\n
 * *) A totally asynchronous, irregular task also competes for the display. The idle task
 * estimates the CPU load and an associated display task of low priority prints the result
 * on the LCD.\n
 *
 *   TRK-USB-MPC5643L port:\n
 *
 * The port of the sample to the eval board TRK-USB-MPC5643L reuqires some changes because
 * of different hardware environments. There is no out-of-the-shelf LCD and no library
 * LiquidCrystal to control it. We have replaced all writes to the display by simple printf
 * output to the serial console. The logic of acquiring the display with a mutex to avoid
 * coincidental printing operations from different tasks became obsolete as printing lines
 * with printf is synchronized in the C library. Consequently, the Arduino module
 * dpy_display.cpp was not nigrated but deleted from the project.\n
 *   The buttons on Arduino are read via an anlogue voltage. This had shaped a functional
 * dependency between the analog input capturing of this sample and the user interface
 * withthe buttons. For the TRK-USB-MPC5643L this is obsolete, reading the button states is
 * independent from analog input processing. The user interface task has been re-written.\n
 *   The concept of the user interface is different. The TRK-USB-MPC5643L has only two
 * buttons. The two pairs of buttons from the Arduino LCD shield are emulated by a state:
 * The TRK-USB-MPC5643L is normally used to alter the selected analog input but if one of
 * the buttons is pressed for about 2s then the mode toggles and the buttons can be used to
 * adjust the time of the real time clock. The mode is shown by LED5: Green means selection
 * of ADC input channel, red means adjusting the RTC time.\n
 *   The analog driver of the TRK-USB-MPC5643L is configured of a sub-set of the available
 * channels only. This set included the internal temperature measurement TSENS0 and TSENS1,
 * the external temperature measurement with chip u4 and the internal band-gap reference
 * voltages of the two ADCs. For the MPC5643L, pad programming is independent of ADC
 * channel selection. In this sample, only a single analog channel is really connected to
 * an external voltage - this is the temparature voltage from u4 on channel 0. All other
 * channels will show either internal voltages sources or undetermined, floating values.
 *
 * @remark
 *   This test case is no demonstration of an optimal application design. Instead of
 * creating a clear, simple, stable, understandable, maintainable architecture, we tried to
 * put a number of RTOS elements in it to demonstrate and test the capabilities of RTuinOS.
 * Production code would probably look different.
 *
 * Copyright (C) 2013-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   setupAfterKernelInit
 *   loop
 * Local functions
 *   taskOnADCComplete
 *   taskRTC
 *   taskIdleFollower
 *   taskButton
 *   taskDisplayVoltage
 */

/*
 * Include files
 */

#include <stdio.h>
#include <assert.h>

#include "tac_mcuTestAndCalibrationData.h"
#include "rtos.h"
#include "f2d_float2Double.h"
#include "gsl_systemLoad.h"
#include "aev_applEvents.h"
#include "but_button.h"
#include "ihw_initMcuCoreHW.h"
#include "mai_main.h"
#include "clk_clock.h"
#include "adc_eTimerClockedAdc.h"
#include "adc_analogInput.h"


/*
 * Defines
 */

/** The index to the task objects as needed for requesting the overrun counter or the stack
    usage. */
enum { idxTaskOnADCComplete
     , idxTaskRTC
     , idxTaskIdleFollower
     , idxTaskButton
     , idxTaskDisplayVoltage
     , noTasks
     };

/** The number of interrupt levels, we use in this application is required for an
    estimation of the appropriate stack sizes.\n
      We have 2 interrupts for the serial interface, the RTOS system timer and the
    ADC's conversion complete interrupt. */
#define NO_IRQ_LEVELS_IN_USE    4

/** The stack usage by the application tasks itself; interrupts disregarded here. */
#define STACK_USAGE_IN_BYTE     256

/** The stack size. */
#define STACK_SIZE_IN_BYTE \
            (RTOS_REQUIRED_STACK_SIZE_IN_BYTE(STACK_USAGE_IN_BYTE, NO_IRQ_LEVELS_IN_USE))


/*
 * Local type definitions
 */


/*
 * Local prototypes
 */


/*
 * Data definitions
 */

static volatile uint32_t _noAdcResults = 0;
static _Alignas(uint64_t) uint8_t _stackTaskOnADCComplete[STACK_SIZE_IN_BYTE]
                                , _stackTaskRTC[STACK_SIZE_IN_BYTE]
                                , _stackTaskIdleFollower[STACK_SIZE_IN_BYTE]
                                , _stackTaskButton[STACK_SIZE_IN_BYTE]
                                , _stackTaskDisplayVoltage[STACK_SIZE_IN_BYTE];

/* Results of the idle task. */
volatile unsigned int _cpuLoad = 1000;


/*
 * Function implementation
 */

/**
 * This task is triggered one by one by the interrupts triggered by the ADC, when it
 * completes a conversion. The task reads the ADC result register and processes the
 * sequence of values. The processing result is input to a slower, reporting task.
 *   @param initialResumeCondition
 * The vector of events which made the task due the very first time.
 */
static void taskOnADCComplete(uint32_t initialResumeCondition ATTRIB_DBG_ONLY)
{
    assert(initialResumeCondition == EVT_ADC_CONVERSION_COMPLETE);

    do
    {
        /* Call the actual interrupt handler code. */
        adc_onConversionComplete();
    }
    while(rtos_waitForEvent( EVT_ADC_CONVERSION_COMPLETE | RTOS_EVT_DELAY_TIMER
                           , /* all */ false
                           , /* timeout */ 1
                           )
#ifdef DEBUG
          == EVT_ADC_CONVERSION_COMPLETE
#endif
         );

    /* The following assertion fires if the ADC interrupt isn't timely. The wait condition
       specifies a sharp timeout. True production code would be designed more failure
       tolerant and e.g. not specify a timeout at all. This code would cause a reset in
       case. */
    assert(false);

} /* End of taskOnADCComplete */




/**
 * A regular task of about 250 ms task time, which implements a real time clock.
 *   @param initialResumeCondition
 * The vector of events which made the task due the very first time.
 */

static void taskRTC(uint32_t initialResumeCondition ATTRIB_DBG_ONLY)
{
    assert(initialResumeCondition == RTOS_EVT_ABSOLUTE_TIMER);

    /* Regularly call the RTC implementation at its expected rate: The RTC module exports the
       expected task time by a define. */
    do
    {
        clk_taskRTC();
    }
    while(rtos_suspendTaskTillTime
                    (/* deltaTimeTillResume */ CLK_TASK_TIME_RTUINOS_STANDARD_TICKS)
         );
    assert(false);

} /* End of taskRTC */




/**
 * A task, which is triggered by the idle loop each time it has new results to display. The
 * idle task itself must not acquire any mutexes and consequently, it can't ever own the
 * display. This task however can.
 *   @param initialResumeCondition
 * The vector of events which made the task due the very first time.
 */

static void taskIdleFollower(uint32_t initialResumeCondition ATTRIB_DBG_ONLY)
{
    assert(initialResumeCondition == EVT_TRIGGER_IDLE_FOLLOWER_TASK);
    do
    {
        iprintf("CPU load: %u %%" RTOS_EOL, (_cpuLoad+5)/10);
    }
    while(rtos_waitForEvent(EVT_TRIGGER_IDLE_FOLLOWER_TASK, /* all */ false, 0));
    assert(false);

} /* End of taskIdleFollower */





/**
 * A task, which is triggered by the processing of the ADC conversion results: Whenever it
 * has a new voltage measurement of the analog button input this task is triggered to do
 * the further evaluation, i.e. identification of the pressed button, debouncing, state
 * machine and dispatching to the clients.
 *   @param initialResumeCondition
 * The vector of events which made the task due the very first time.
 */

static void taskButton(uint32_t initialResumeCondition ATTRIB_DBG_ONLY)
{
    assert(initialResumeCondition == EVT_TRIGGER_TASK_BUTTON);
    do
    {
        but_onCheckUserInput();
    }
    while(rtos_waitForEvent(EVT_TRIGGER_TASK_BUTTON, /* all */ false, 0));
    assert(false);

} /* End of taskButton */





/**
 * A task, which is triggered by the processing of the ADC conversion results: Whenever it
 * has a new input voltage measurement this task is triggered to display the result.
 *   @param initialResumeCondition
 * The vector of events which made the task due the very first time.
 */

static void taskDisplayVoltage(uint32_t initialResumeCondition ATTRIB_DBG_ONLY)
{
    assert(initialResumeCondition == EVT_TRIGGER_TASK_DISPLAY_VOLTAGE);
    
    /* The rate of the result values is about once every 133 ms, which makes the display
       quite nervous. And it would become even faster is the averaging constant
       ADC_NO_AVERAGED_SAMPLES would be lowered. Therefore we average here again to get are
       better readable, more stable display.
         The disadvantage: The state machine in module adc synchronizes switching the ADC
       input with the series of averaged samples. This is impossible here, which means that
       - in the instance of switching to another ADC input - the averaging series formed
       here typically consist of some samples from the former input and some from the new
       input. We do no longer see a sharp switch but a kind of cross fading. */
#define NO_AVERAGED_SAMPLES         5
#define SCALING_BIN_TO_V(binVal)    (ADC_SCALING_BIN_TO_V(binVal)/(float)NO_AVERAGED_SAMPLES)

    static uint32_t accumuatedAdcResult_ = 0;
    static uint8_t noMean_ = NO_AVERAGED_SAMPLES;
    do
    {
        /* This low priority task needs to apply a critical section to read the result of
           the ADC interrupt task of high priority. */
        ihw_suspendAllInterrupts();
        accumuatedAdcResult_ += adc_inputVoltage;
        ihw_resumeAllInterrupts();
        
        if(--noMean_ == 0)
        {
            iprintf( "Selected ADC input: %u mV" RTOS_EOL
                   , (unsigned int)(1000.0f*SCALING_BIN_TO_V(accumuatedAdcResult_)+0.5)
                   );

            /* Start next series on averaged samples. */
            noMean_ = NO_AVERAGED_SAMPLES;
            accumuatedAdcResult_ = 0;
        }
    }        
    while(rtos_waitForEvent(EVT_TRIGGER_TASK_DISPLAY_VOLTAGE, /* all */ false, 0));
    assert(false);
    
#undef NO_AVERAGED_SAMPLES
#undef SCALING_BIN_TO_V
} /* End of taskDisplayVoltage */





/**
 * The initalization of the RTOS tasks and general board initialization.
 */

void setup()
{
    /* Print standard greeting of RTuinOS applications. */
    iprintf(RTOS_EOL RTOS_RTUINOS_STARTUP_MSG RTOS_EOL);

    /* Configure the interrupt task of highest priority class. */
    assert(noTasks == RTOS_NO_TASKS);
    rtos_initializeTask( /* idxTask */          idxTaskOnADCComplete
                       , /* taskFunction */     taskOnADCComplete
                       , /* prioClass */        RTOS_NO_PRIO_CLASSES-1
                       , /* pStackArea */       &_stackTaskOnADCComplete[0]
                       , /* stackSize */        sizeof(_stackTaskOnADCComplete)
                       , /* startEventMask */   EVT_ADC_CONVERSION_COMPLETE
                       , /* startByAllEvents */ false
                       , /* startTimeout */     0
                       );

    /* Configure the real time clock task of lowest priority class. */
    rtos_initializeTask( /* idxTask */          idxTaskRTC
                       , /* taskFunction */     taskRTC
                       , /* prioClass */        0
                       , /* pStackArea */       &_stackTaskRTC[0]
                       , /* stackSize */        sizeof(_stackTaskRTC)
                       , /* startEventMask */   RTOS_EVT_ABSOLUTE_TIMER
                       , /* startByAllEvents */ false
                       , /* startTimeout */     CLK_TASK_TIME_RTUINOS_STANDARD_TICKS
                       );

    /* Configure the idle follower task of lowest priority class. */
    rtos_initializeTask( /* idxTask */          idxTaskIdleFollower
                       , /* taskFunction */     taskIdleFollower
                       , /* prioClass */        0
                       , /* pStackArea */       &_stackTaskIdleFollower[0]
                       , /* stackSize */        sizeof(_stackTaskIdleFollower)
                       , /* startEventMask */   EVT_TRIGGER_IDLE_FOLLOWER_TASK
                       , /* startByAllEvents */ false
                       , /* startTimeout */     0
                       );

    /* Configure the button evaluation task. Its priority is below the interrupt but - as
       it implements user interaction - above the priority of the display tasks. */
    rtos_initializeTask( /* idxTask */          idxTaskButton
                       , /* taskFunction */     taskButton
                       , /* prioClass */        1
                       , /* pStackArea */       &_stackTaskButton[0]
                       , /* stackSize */        sizeof(_stackTaskButton)
                       , /* startEventMask */   EVT_TRIGGER_TASK_BUTTON
                       , /* startByAllEvents */ false
                       , /* startTimeout */     0
                       );

    /* Configure the result display task. */
    rtos_initializeTask( /* idxTask */          idxTaskDisplayVoltage
                       , /* taskFunction */     taskDisplayVoltage
                       , /* prioClass */        0
                       , /* pStackArea */       &_stackTaskDisplayVoltage[0]
                       , /* stackSize */        sizeof(_stackTaskDisplayVoltage)
                       , /* startEventMask */   EVT_TRIGGER_TASK_DISPLAY_VOLTAGE
                       , /* startByAllEvents */ false
                       , /* startTimeout */     0
                       );
    
    /* Initialize other modules. */
    tac_initTestAndCalibrationDataAry();
    init_button();    

} /* End of setup */



/**
 * The second initailization hook is applied to install the ADC driver. This hook is called
 * after initialization of the RTuinOS kernel so that it can handle task switches. Being in
 * this state - and even if the system timer is not yet running - we can safely start the
 * ADC interrupts, which can make the ADC data evaluation task due and active.
 */
void setupAfterKernelInit()
{
    /* Initialize ADC driver. The driver configures a kernel interrupt and must therefore
       not been initialized in the original Arduino hook setup(). At the time of setup()
       the kernel is not yet ready to process kernel interrupts. */
    adc_initAfterPowerUp();
    
    /* Route analog input voltage to ADC. We use AN1 of ADC_0, port B8, PCR[24]. This
       connects the output of temperature chip u4 on the TRK-USB-MPC5643L to the ADC. */
    SIUL.PCR[24].B.APC = 1;
    
} /* End of setupAfterKernelInit */




/**
 * The application owned part of the idle task. This routine is repeatedly called whenever
 * there's some execution time left. It's interrupted by any other task when it becomes
 * due.
 *   @remark
 * Different to all other tasks, the idle task routine may and should return. (The task as
 * such doesn't terminate). This has been designed in accordance with the meaning of the
 * original Arduino loop function.
 */
void loop()
{
    /* Give an alive sign. */
    mai_blink(3);
    
#ifdef DEBUG
    iprintf(RTOS_EOL "RTuinOS is idle" RTOS_EOL);
#endif

    /* Share result of CPU load computation with the displaying idle follower task. No
       access synchronization is needed here for two reasons: Writing an unsigned int is
       atomic and we have a strict coupling in time between the idle task and the data
       reading task: They become active one after another. */
    _cpuLoad = gsl_getSystemLoad();

#ifdef DEBUG
    ihw_suspendAllInterrupts();
    uint16_t adcResult       = adc_inputVoltage;
    uint32_t noAdcResults = adc_noAdcResults;
    uint8_t hour = clk_noHour
          , min  = clk_noMin
          , sec  = clk_noSec;
    ihw_resumeAllInterrupts();

    iprintf("At %02u:%02u:%02u:" RTOS_EOL, hour, min, sec);
    printf( "ADC result %7lu at %7.2f s: %.4f V (input)" RTOS_EOL
          , noAdcResults
          , f2d(1e-3*millis())
          , f2d(ADC_SCALING_BIN_TO_V(adcResult))
          );
    printf( "CPU load: %.1f %%, chip temperature: %.1f/%.1f \260C" RTOS_EOL
          , f2d((float)_cpuLoad/10.0)
          , f2d(adc_getTsens0())
          , f2d(adc_getTsens1())
          );
    assert(rtos_getTaskOverrunCounter(/* idxTask */ idxTaskRTC, /* doReset */ false) == 0);
    
    uint8_t u;
    for(u=0; u<RTOS_NO_TASKS; ++u)
        iprintf("Unused stack area of task %u: %u Byte" RTOS_EOL, u, rtos_getStackReserve(u));
#else
    uint8_t u;
    for(u=0; u<RTOS_NO_TASKS; ++u)
    {
        unsigned int stackReserve = rtos_getStackReserve(u);
        if(stackReserve < 200)
        {
            iprintf( "CAUTION: Unused stack area of task %u is only %u Byte" RTOS_EOL
                   , u
                   , stackReserve
                   );
        }
    }
#endif

    /* Trigger the follower task, which is capable to safely display the results. */
    rtos_sendEvent(EVT_TRIGGER_IDLE_FOLLOWER_TASK);

} /* End of loop */




