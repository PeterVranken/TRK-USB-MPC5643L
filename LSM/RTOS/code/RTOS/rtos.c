/**
 * @file rtos.c
 * This file implements a simple Real Time Operating System (RTOS) for the MPC5643L. Only a
 * few lines of code are required - this is because of the hardware capabilities of the
 * interrupt controller INTC, which has much of an RTOS kernel in hardware. The RTOS is
 * just a wrapper around these hardware capabilities. The reference manual of the INTC
 * partly reads like an excerpt from the OSEK/VDX specification, it effectivly implements
 * the basic task conformance classes BCC1 and BCC2 from the standard. Since we barely add
 * software support, our operating system is by principle restricted to these conformance
 * classes.\n
 *   Basic conformance class means that a task cannot suspend intentionally and ahead of
 * its normal termination. Once started, it needs to be entirely executed. Due to the
 * strict priority scheme it'll temporarily suspend only for the sake of tasks of higher
 * priority (but not voluntarily or on own desire). Another aspect of the same is that the
 * RTOS doesn't know events -- events are usually the way intentional suspension and later
 * resume of tasks is implemented.
 *
 * Copyright (C) 2017 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "MPC5643L.h"
#include "ihw_initMcuCoreHW.h"
#include "rtos.h"


/*
 * Defines
 */

/** The implementation of tasks as software interrupts limits the available number to
    #MAX_NO_TASKS. */
#define MAX_NO_TASKS    8


/*
 * Local type definitions
 */
 
/** The runtime information for an application task.
      Note, since we are hardware limited to eight tasks we use a statically allocated
    array of fixed size for all possible tasks. A resource optimized implementation could
    use an application defined macro to tailor the size of the array and it could put the
    task configuration data into ROM. */
typedef struct task_t
{
    /** The static configuration data for the task. */
    rtos_taskDesc_t taskDesc;
    
    /** The next due time. */
    unsigned int tiDue;
    
    /** We can't queue task activations. If a task is still buys when it becomes due again,
        a task activation is lost. This event is considered a task overrun and it is counted
        for diagnostic purpose. The counter is saturated and will halt at the
        implementation maximum.
          @remark This field is shared with the external client code of the module. There,
        it is read only. Only the scheduler code must update the field. */
    /// @todo We can. By not adjusting the due time. Some limitation code required to make this safe
    unsigned int noActivationLoss;

    /** The task termination code requires access to a bit in the INTC. This variable
        points to the appropriate register. */    
    vuint32_t *pINTC_SSCIR;
    
    /** The task termination code requires access to a bit in the INTC. This mask variable
        holds the bit to set in the appropriate register. */
    uint32_t maskBitClr;

} task_t;


/*
 * Local prototypes
 */
 
/* The prototypes of all possible task functions. */
#define TASK(taskId)    static void swInt##taskId(void);
TASK(0)
TASK(1)
TASK(2)
TASK(3)
TASK(4)
TASK(5)
TASK(6)
TASK(7)
#undef TASK
 
 
/*
 * Data definitions
 */
 
/** The list of all tasks. */
static task_t _taskAry[MAX_NO_TASKS];

/** The number of registered tasks. The range is 0..8. The tasks are implemented by
    software interrupts and these are limited by hardware to a number of eight. */
static unsigned int _noTasks = 0;

/** An array holds the function pointers to all possible task functions. */
static void (* const _swIntAry[8])(void) =
{ [0] = swInt0
, [1] = swInt1
, [2] = swInt2
, [3] = swInt3
, [4] = swInt4
, [5] = swInt5
, [6] = swInt6
, [7] = swInt7
};


/*
 * Function implementation
 */

/*
 * To hide the task-individual termination code from the user visible code, we implement a
 * task function for all possible tasks. Each one contains the branch into the user
 * specified code and the individual termination code.
 */
#define TASK(taskId)                                            \
static void swInt##taskId(void)                                 \
{                                                               \
    const task_t * const pTask = &_taskAry[taskId];             \
    (*pTask->taskDesc.taskFct)(pTask->taskDesc.contextData);    \
    *pTask->pINTC_SSCIR = pTask->maskBitClr;                    \
}
TASK(0)
TASK(1)
TASK(2)
TASK(3)
TASK(4)
TASK(5)
TASK(6)
TASK(7)
#undef TASK



/**
 * Registration of a (cyclically activated) application task.
 *   @return
 * All application tasks are identified by a positive integer. Normally this ID is
 * returned. The maximum number of tasks is limited to eight by hardware constraints. If
 * the task cannot be started due to this constraint or if the task descriptor contains
 * invalid data then the function returns (unsigned int)-1 and an assertion fires in DEBUG
 * compilation.
 *   @param
 * All calls of this function need to be done prior to the start of the kernel using
 * rtos_initKernel().
 */
unsigned int rtos_registerTask(const rtos_taskDesc_t *pTaskDesc)
{
    /* The number of tasks is contraint by hardware (INTC) */    
    if(_noTasks >= MAX_NO_TASKS)
    {
        assert(false);
        return (unsigned int)-1;
    }
    
    /* Task function not set. */
    if(pTaskDesc->taskFct == NULL)
    {
        assert(false);
        return (unsigned int)-1;
    }
    
    /* The INTC permits priorities only in the range 0..15 and we exclude 0 since such a
       task would never become active. */
    if(pTaskDesc->priority == 0  ||  pTaskDesc->priority > 15)
    {
        assert(false);
        return (unsigned int)-1;
    }
    
    /* The full 32 Bit range is avoided for time designations in order to have safe and
       unambiguous before and after decisions in a cyclic time model. */
    if(((pTaskDesc->tiCycleInMs | pTaskDesc->tiFirstActivationInMs) & 0xc0000000) != 0)
    {
        assert(false);
        return (unsigned int)-1;
    }
    
    /* Add the new task to the array. */             
    _taskAry[_noTasks].taskDesc = *pTaskDesc;

    /* Initialize the dynamic task data. */
    _taskAry[_noTasks].tiDue = pTaskDesc->tiFirstActivationInMs;
    _taskAry[_noTasks].noActivationLoss = 0;

    /** The task termination code requires access to a bit in the INTC. The next variable
        designate appropriate register and bit in register. */
    if(_noTasks <= 3)
    {
        _taskAry[_noTasks].pINTC_SSCIR = &INTC.SSCIR0_3.R;
        _taskAry[_noTasks].maskBitClr = 0x01000000ul >> (8*_noTasks);
    }
    else
    {
        _taskAry[_noTasks].pINTC_SSCIR = &INTC.SSCIR4_7.R;
        _taskAry[_noTasks].maskBitClr = 0x01000000ul >> (8*(_noTasks-4));
    }
    
    
    /* Register the task function at the INTC, who is actually doing the task activation
       and who will actually invoke the function. */
    assert(_noTasks <= 7);
    ihw_installINTCInterruptHandler( _swIntAry[_noTasks]
                                   , /* vectorNum */ _noTasks
                                   , /* psrPriority */ pTaskDesc->priority
                                   , /* isPreemptable */ true
                                   );
    
    return _noTasks++;

} /* End of rtos_registerTask */



/**
 * The OS timer handler. This routine is invoked once a Millisecond and triggers most of
 * the scheduler decisions. The application code is expected to run mainly in regular tasks
 * and these are activated by this routine when they become due. All the rest is done by
 * the interrupt controller INTC.
 */
static void osTimerTick()
{
    static unsigned long tiOs_ = 0;

    /* The scheduler is most simple; the only condition to make a task ready is the next
       periodic due time. The task activation is fully left to the hardware of the INTC and
       we don't have to bother with priority handling or task/context switching. */
    /// @todo reorganize loop. Read HW register once, evaluate and modify the four entries in the copy and write result back once.
    unsigned idxTask;
    task_t *pTask = &_taskAry[0];
    vuint32_t *pINTC_SSCIR = &INTC.SSCIR0_3.R;
    uint32_t mask = 0x03000000;
    for(idxTask=0; idxTask<_noTasks; ++idxTask, ++pTask)
    {
        if((signed int)(pTask->tiDue - tiOs_) <= 0)
        {
            /* Task is due. Read software interrupt bit. If it is still set then we have a
               task overrun otherwise activate task. */
            if((*pINTC_SSCIR & mask) == 0)
            {
                /* Put task into ready state (and leave the activation to the INTC). It is
                   important to avoid a read-modify-write operation, don't simply set a
                   single bit, the other three interrupts in the same register could be
                   harmed. */
                *pINTC_SSCIR = mask;
            }
            else
            {
                /* CLRi is still set, interrupt has not completed yet, task has not
                   terminated yet. */
                unsigned int noActivationLoss = ++ _taskAry[_noTasks].noActivationLoss;
                if(noActivationLoss > 0)
                    _taskAry[idxTask].noActivationLoss = noActivationLoss;
            }
            
            /* Adjust the due time. */
            pTask->tiDue += pTask->taskDesc.tiCycleInMs;
        }
        
        /* 4 software interrupts share a 32 Bit register. Move pointer and reset mask to
           first byte. */
        if(idxTask == 3)
        {
            ++ pINTC_SSCIR;
            assert(pINTC_SSCIR == &INTC.SSCIR4_7.R);
            mask = 0x03000000;
        }
        else
            mask >>= 8;

    } /* End for(All registered tasks) */
    
    ++ tiOs_;

    /* Acknowledge the interrupt in the causing HW device. */
    PIT.TFLG0.B.TIF = 0x1;

} /* End of osTimerTick */



/**
 * Initialization of the RTOS kernel. Has to be called before the External Interrupts are
 * enabled at the CPU (see ihw_resumeAllInterrupts()).
 *   @remark
 * The RTOS kernel uses a tick of 1ms. It applies the Periodic Interrupt Timer 0 for this
 * purpose. This timer is reserved to the RTOS and must not be used at all by some
 * application code.
 *   @param
 * All application tasks need to be registered before invoking this function, see
 * rtos_registerTask().
 */

void rtos_initKernel(void)
{
    /* Disable all PIT timers during configuration. */
    PIT.PITMCR.R = 0x2;

    /* Install the interrupt handler for cyclic timer PIT 0. It drives the OS scheduler for
       cyclic task activation. */
    ihw_installINTCInterruptHandler( osTimerTick
                                   , /* vectorNum */ 59
                                   , /* psrPriority */ 15
                                   , /* isPreemptable */ false
                                   );

    /* Peripheral clock has been initialized to 120 MHz. To get a 1ms interrupt tick we
       need to count till 120000.
         -1: See MCU reference manual, 36.5.1, p. 1157. */
    PIT.LDVAL0.R = 120000-1; /* Interrupt rate 1ms */

    /* Enable interrupts by this timer and start it. */
    PIT.TCTRL0.R  = 0x3;
    
    /* Enable timer operation and let them be stopped on debugger entry. Note, this is a
       global setting for all four timers, even if we use and reserve only one for the
       RTOS. */
    PIT.PITMCR.R = 0x1;

} /* End of rtos_initKernel */

