/**
 * @file tsk_taskFunction.c
 * The implementation of the task functions, i.e. the functions, which implement the action
 * of a task. The required scheduler logic to invoke this function whenever appropriate, is
 * not found here but implemented in sch_scheduler.c.
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
 *   tsk_taskA_reportTime
 *   tsk_taskB
 *   tsk_taskC
 *   tsk_taskD_produce
 *   tsk_taskE_consume
 * Local functions
 *   RTC
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "lbd_ledAndButtonDriver.h"
#include "del_delay.h"
#include "sc_systemCalls.h"
#include "tsk_taskFunction.h"


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
 
 
/*
 * Function implementation
 */


/**
 * Simple real time clock. The elapsed time is printed to the console. Is used by task A to
 * print the time elapsed. The time information is based on counting the function
 * invocation and it'll only be correct if the rate of calling this function is once a 10
 * Milliseconds.
 */
static void RTC(void)
{
    static unsigned int noCalls_ = 0;
    
    /* Display a real time clock. */
    static unsigned int noMillis_ = 0
                      , noSecs_ = 0
                      , noMins_ = 0
                      , noHours_ = 0;
    noMillis_ += 10;
    if(noMillis_ >= 1000)
    {
        noMillis_ = 0;
        ++ noSecs_;
        if(noSecs_ >= 60)
        {
            noSecs_ = 0;
            ++ noMins_;
            if(noMins_ >= 60)
            {
                noMins_ = 0;
                ++ noHours_;
                if(noHours_ >= 24)
                    noHours_ = 0;
            }
        }
    }

    static signed int noCallsNextReport_ = 0;
    if(((signed int)noCalls_ - noCallsNextReport_) >= 0)
    {
        iprintf( "RTC (task A): %02u:%02u:%02u.%03u\r\n"
               , noHours_, noMins_, noSecs_, noMillis_
               );
        noCallsNextReport_ += 67;
    }
    
    ++ noCalls_;

} /* End of RTC */



/**
 * Task A: Is permanently spinning at 100Hz. We can exploit the regular invocation time
 * grid to display some time information.
 */
void tsk_taskA_reportTime()
{
    /* Update and print the time. */
    RTC();
    
    /* To force preemptions, we produce some CPU load inside this task. */
    del_delayMicroseconds(/* tiCpuInUs */ 1500);

} /* End of tsk_taskA_reportTime. */



/**
 * Task B: Single shot task, triggered every 2ms. Reports counter value.\n
 *   Note, this task shares the stack with task C.
 *   @param idxCycle
 * An arbitrary parameter, e.g. a linear incremented counter of invocations.
 */
void tsk_taskB(unsigned int idxCycle)
{
    static signed int noCyclesNextReport_ = 0;
    if(((signed int)idxCycle - noCyclesNextReport_) >= 0)
    {
        iprintf("tsk_taskB: %u activations\r\n", idxCycle);
        
        static bool isOn_ = true;
        lbd_setLED(lbd_led_D5_red, isOn_);
        isOn_ = !isOn_;
        
        noCyclesNextReport_ += 1000;
    }
    
    /* To force preemptions, we produce some CPU load inside this task. */
    del_delayMicroseconds(/* tiCpuInUs */ 600);

} /* End of tsk_taskB. */



/**
 * Task B: Single shot task, triggered every 7ms. Reports counter value.\n
 *   Note, this task shares the stack with task B.
 *   @param idxCycle
 * An arbitrary parameter, e.g. a linear incremented counter of invocations.
 */
void tsk_taskC(unsigned int idxCycle)
{
    static signed int noCyclesNextReport_ = 0;
    if(((signed int)idxCycle - noCyclesNextReport_) >= 0)
    {
        iprintf("tsk_taskC: %u activations\r\n", idxCycle);
        noCyclesNextReport_ += 250;
    }
    
    /* To force preemptions, we produce some CPU load inside this task. */
    del_delayMicroseconds(/* tiCpuInUs */ 2200);

} /* End of tsk_taskC. */



/**
 * Task D, producer: Produce next artifact. Do this in a redundant way such that an
 * unwanted preemption by the consumer would generate recognizable faults.
 *   @return
 * The produced artifact is returned by reference.
 *   @param idxCycle
 * An arbitrary parameter, e.g. a linear incremented counter of invocations.
 */
const tsk_artifact_t *tsk_taskD_produce(unsigned int idxCycle)
{
    /* Caution, this task is run in user mode and the serial output has not been updated to
       run in user mode. printf and else must not be used in this task; a privileged
       instruction exception would result. */
       
    static tsk_artifact_t a_ = {.x = 0, .y = 0};
    
    a_.noCycles = idxCycle;
    a_.x += 2;
    ++ a_.y;
    -- a_.x;

    return &a_;

} /* End of tsk_taskD_produce. */



/**
 * Task E, consumer: Validate the received artifact. Double-check the redundant data of the
 * object. Regularly report progress and status to stdout.
 *   @return
 * The number of recognized faults is returned.
 *   @param pA
 * The artifact to validate by reference.
 */
signed int tsk_taskE_consume(const tsk_artifact_t *pA)
{
    assert(pA != NULL);
    signed int delta = (signed int)(pA->x - pA->y);
    static signed int noCyclesNextReport_ = 0;
    if(((signed int)pA->noCycles - noCyclesNextReport_) >= 0)
    {
        iprintf("tsk_taskE_consume: %u cycles, delta is %i\r\n", pA->noCycles, delta);
        
        static bool isOn_ = true;
        lbd_setLED(lbd_led_D4_grn, isOn_);
        isOn_ = !isOn_;
        
        noCyclesNextReport_ += 100000;
    }
    return delta;

} /* End of tsk_taskE_consume. */
