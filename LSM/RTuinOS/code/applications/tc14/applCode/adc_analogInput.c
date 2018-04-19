/**
 * @file adc_analogInput.cpp
 *   The ADC task code: Process the analog input.
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
 */
/* Module interface
 *   adc_initAfterPowerUp
 *   adc_nextInput
 *   adc_onConversionComplete
 * Local functions
 *   onEndOfConversion
 *   selectAdcInput
 */

/*
 * Include files
 */

#include <stdio.h>

#include "typ_types.h"
#include "rtos.h"
#include "rtos_systemCalls.h"
#include "aev_applEvents.h"
#include "adc_eTimerClockedAdc.h"
#include "adc_analogInput.h"


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
 
/** Global counter of all ADC conversion results starting with system reset. The frequency
    should be about 960 Hz. */
volatile uint32_t adc_noAdcResults = 0;

/** The voltage measured at the user selected analog input, see \a _userSelectedInput. */
volatile uint16_t adc_inputVoltage = 0;

/** The user selected ADC input as a linear number between 0 and number of channels
    configured in adc_eTimerClockedAdc.h.
      @remark This variable has a special character. It is manipulated via function \a
    adc_nextInput solely by another task, the user interface task. The task, which is
    implemented in this module will never touch this variable. */
static uint8_t _userSelectedInputLin = adc_adc0_idxChn10;

/** The ADC channel, which is currently selected for observation. Can either be user
    selected or a selected by some program logic. */
static unsigned int _idxDisplayedAdcChn = adc_adc0_idxChn10;

/*
 * Function implementation
 */


/** 
 * End of conversion callback: This function is called from the ADC driver every time a new
 * ADC sample result is available. The function is executed in the context of the ADC
 * interrupt. The driver is configured to use a kernel interrupt, which gives us the chance
 * to send an event from the callback. A task is configured to be activated by this event.
 * It'll run synchronously with the ADC conversion cycle and can process the result data
 * coherently.
 *   @return
 * A kernel interrupt returns \a true if it initiates a task switch and \a false if it
 * should continue the preempted context as an ordinary interrupt generally does.
 *   @param pCmdContextSwitch
 * If the kernel interrupts wants to initiate a task switch then it'll enter the
 * information where to place the context save information of the suspended task and where
 * to find the context save information of the resumed task into * \a pCmdContextSwitch.
 * Moreover, and if the resumed context is a system call, it can provide the return value of
 * that system call.
 */
static bool onEndOfConversion(int_cmdContextSwitch_t *pCmdContextSwitch)
{
    /* Sending an event and taking the decision, whether this event makes another task
       active, is a service, which is offered by the kernel implementation. We simply call
       the kernel function.
         Note, the normal application API for sending an event is implemented as software
       interrupt (system call) but here we are already inside an interrupt. A system call
       from here would crash the system. Instead, we call the kernel function, which is
       called by the system call implementation, too. */
    return rtos_sc_sendEvent(pCmdContextSwitch, /* eventVec */ EVT_ADC_CONVERSION_COMPLETE);

} /* End of onEndOfConversion */



/**
 * Reprogram the ADC so that the next conversion will use another input.
 *   @param input
 * The input to select as displayed ADC channel.
 *   @remark
 * This function is a relict from the original Arduino implementation. Here, it only
 * selects the ADC channel to display. The actual channel configuration is made statically
 * at compile time and all configured channels are always sampled.
 */ 

static void selectAdcInput(uint8_t input)
{
    _idxDisplayedAdcChn = input;
    
} /* End of selectAdcInput */




/**
 * Configure the ADC but do not release the interrupt on ADC conversion complete yet. Most
 * important is the hardware triggered start of the conversions, see chosen settings for
 * ADATE and ADTS.\n
 *   The initialization is called at system startup time, before the RTuinOS kernel is
 * started and multitasking takes place. Therefore it's crucial to not enable the actual
 * interrupts yet. This must be done as part of the start of the kernel, when the system is
 * ready to accept and handle the interrupts. Please refer to the RTuinOS manual for more.
 */

void adc_initAfterPowerUp()
{
    /* Initialize the ADC driver. Note, the driver is not specific to this sample
       application but a reusable building block from the TRK-USB-MPC5643L project
       (https://github.com/PeterVranken/TRK-USB-MPC5643L). */
    adc_initDriver(onEndOfConversion);

    /* Start ADC conversion cycles. */
    adc_startConversions();

} /* End of adc_initAfterPowerUp */





/**
 * The input for the measured and displayed voltage is changed by one, upwards or downwards.
 *   @param up
 * True to go from input n to n+1, false step back.
 *   @remark
 * The intended use case of this function is that it is called by another task, the user
 * interface task. The implemented data access synchronization requires that the other
 * task has the same or a lower priority than this task, the ADC interrupt task.
 */

void adc_nextInput(bool up)
{
    /* Select the new input by increment/decrement. 
         Remark: Although implemented here in the ADC module the variable
       _userSelectedInputLin is completely owned by the user interface task ("owned" with
       respect to concurrency and access rights). No access synchronization code is needed
       although we have a read/modify/write operation. */
    uint8_t tmpMux = 0;
    if(up)
    {
        tmpMux = ++ _userSelectedInputLin;
        if(tmpMux >= adc_adc0_noActiveChns+adc_adc1_noActiveChns)
            tmpMux = 0;
    }
    else
    {
        /* <0 is detected as overrun, it's an unsigned variable. */
        tmpMux = -- _userSelectedInputLin;
        if(tmpMux >= adc_adc0_noActiveChns+adc_adc1_noActiveChns)
            tmpMux = adc_adc0_noActiveChns+adc_adc1_noActiveChns-1;
    }

    /* Now write to the target variable in an atomic operation. */
    _userSelectedInputLin = tmpMux;

    /* Display selection of new ADC input. */
    iprintf("New ADC input selection: %u" RTOS_EOL, _userSelectedInputLin);
    
} /* End of adc_nextInput */




/**
 * The main function of the ADC task: It is the handler for the conversion complete
 * interrupt. It reads the new input sample from the ADC registers and processes it.
 * Processing means to do some averaging as a kind of simple down sampling and notify the
 * sub-sequent, slower running clients of the data.\n
 *   There are two kinds of data and two related clients: The analog input 0, which the LCD
 * shield's buttons are connected to, is read regularly and the input values are passed to
 * the button evaluation task, which implements the user interface state machine.\n
 *   A user selected ADC input is measured and converted to Volt. The client of this
 * information is a simple display task.
 */
void adc_onConversionComplete()
{
    /* Averaging: Each series accumulates up to 64 samples. */
    static uint32_t accumuatedAdcResult_ = 0;
    static uint8_t noMean_ = ADC_NO_AVERAGED_SAMPLES;
        
    /* Accumulate all samples of the running series. Add the new ADC conversion result. */
    accumuatedAdcResult_ += (uint32_t)adc_getChannelRawValue(_idxDisplayedAdcChn);
    
    /* Notify the new result to the button evaluation task.\n
         Note, controlling the user interface task from the ADC task was jsutified in the
       original Arduino sample and is only adopted in order to alter the sample code as
       little as possible. In Arduino, the buttons of the user interface are decoded from
       an measured analog voltage and so we used to first read and filter the voltage and
       then trigger the button decoder. The TRK-USB-MPC5643L is more conventional; the
       buttons are connected to GPIO inputs. For debouncing and time dependent actions and
       because of system responsiveness it is required to look at these inputs regularly
       every about 20ms. */
    static uint8_t cntUserInterface = 0;
    if(++cntUserInterface == 19)
    {
        rtos_sendEvent(EVT_TRIGGER_TASK_BUTTON);
        cntUserInterface = 0;
    }
    
    /* Accumulate up to 64 values to do averaging and anti-aliasing for slower reporting
       task. */
    if(--noMean_ == 0)
    {
        /* Averaging: Return to the 16 Bit range (on cost of resolution). */
        accumuatedAdcResult_ /= ADC_NO_AVERAGED_SAMPLES;
        
        /* A new down-sampled result is available for our client, the display task. Since
           the client has a lower priority as this task we don't need a critical section to
           update the client's input. */
        adc_inputVoltage = (uint16_t)accumuatedAdcResult_;
        rtos_sendEvent(EVT_TRIGGER_TASK_DISPLAY_VOLTAGE);

        /* New ADC input is the button voltage. We do this as early as possible in
           the processing here, to have it safely completed before the next conversion
           start interrupt fires. */
        selectAdcInput(_userSelectedInputLin);
        
        /* Start next series of averaged samples. */
        noMean_ = ADC_NO_AVERAGED_SAMPLES;
        accumuatedAdcResult_ = 0;
    }
        
    /* Count the read cycles. The frequency should be about 960 Hz. */
    ++ adc_noAdcResults;

} /* End of adc_onConversionComplete */




