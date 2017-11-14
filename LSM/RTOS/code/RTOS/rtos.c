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
 *   rtos_registerTask
 *   rtos_activateTask
 *   rtos_getNoActivationLoss
 *   rtos_initKernel
 * Local functions
 *   osTimerTick
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
#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "rtos.h"


/*
 * Defines
 */

/** The implementation of tasks as software interrupts limits the available number to
    #MAX_NO_TASKS. */
#define MAX_NO_TASKS    8

/** This code can be used to acknowledge the interrupt bit of the software interrupt of
    given index idxSwIsr. */
#define ACKN_ISR(idxSwIsr)                                                                  \
            (void)((idxSwIsr)<=3? (INTC.SSCIR0_3.R = (0x01000000ul >> (8*(idxSwIsr))))      \
                                : (INTC.SSCIR4_7.R = (0x01000000ul >> (8*((idxSwIsr)-4))))  \
                  )


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
    /// @todo We can queue task activations. By not adjusting the due time. Some limitation code required to make this safe
    unsigned int noActivationLoss;

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
#define TASK(taskId)                                \
static void swInt##taskId(void)                     \
{                                                   \
    const task_t * const pTask = &_taskAry[taskId]; \
    (*pTask->taskDesc.taskFct)();                   \
    ACKN_ISR(/*idxSwIsr*/ taskId);                  \
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
 * The OS timer handler. This routine is invoked once a Millisecond and triggers most of
 * the scheduler decisions. The application code is expected to run mainly in regular tasks
 * and these are activated by this routine when they become due. All the rest is done by
 * the interrupt controller INTC.
 */
static void osTimerTick()
{
    /* Note, the scheduler function is run at highest priority, which means that no task or
       ISR can preempt this code. No mutual exclusion code is required. */

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
        if(pTask->taskDesc.tiCycleInMs > 0)
        {
            if((signed int)(pTask->tiDue - tiOs_) <= 0)
            {
                /* Task is due. Read software interrupt bit. If it is still set then we
                   have a task overrun otherwise we activate task by requesting the
                   software interrupt. */
                if((*pINTC_SSCIR & mask) == 0)
                {
                    /* Put task into ready state (and leave the activation to the INTC). It
                       is important to avoid a read-modify-write operation, don't simply
                       set a single bit, the other three interrupts in the same register
                       could be harmed. */
                    *pINTC_SSCIR = mask;
                }
                else
                {
                    /* CLRi is still set, interrupt has not completed yet, task has not
                       terminated yet.
                         This code requires a critical section. The loss counter can be
                       written concurrently from a task invoking rtos_activateTask(). */
                    const unsigned int noActivationLoss = _taskAry[idxTask].noActivationLoss
                                                          + 1;
                    if(noActivationLoss > 0)
                        _taskAry[idxTask].noActivationLoss = noActivationLoss;
                }

                /* Adjust the due time. */
                pTask->tiDue += pTask->taskDesc.tiCycleInMs;
                
            } /* End if(Task is due?) */
        }        
        else
        {
            /* Non regular tasks: nothing to be done. These tasks are started only by
               explicit software call of rtos_activateTask. */

        } /* End if(Timer or application software activated task?) */

        /* 4 software interrupts share a 32 Bit register; update the mask for the next one.
           After four loops move pointer and reset mask to first byte. */
        if(idxTask != 3)
            mask >>= 8;
        else
        {
            ++ pINTC_SSCIR;
            assert(pINTC_SSCIR == &INTC.SSCIR4_7.R);
            mask = 0x03000000;
        }
    } /* End for(All registered tasks) */
    
    ++ tiOs_;

    /* Acknowledge the interrupt in the causing HW device. */
    PIT.TFLG0.B.TIF = 0x1;

} /* End of osTimerTick */



/**
 * Registration of a (cyclically activated) application task.
 *   @return
 * All application tasks are identified by a positive integer. Normally this ID is
 * returned. The maximum number of tasks is limited to eight by hardware constraints. If
 * the task cannot be started due to this constraint or if the task descriptor contains
 * invalid data then the function returns (unsigned int)-1 and an assertion fires in DEBUG
 * compilation.
 *   @param pTaskDesc
 * All calls of this function need to be done prior to the start of the kernel using
 * rtos_initKernel().
 *   @param tiFirstActivationInMs
 * The first activation of the task in ms after start of kernel. The permitted range
 * is 0..2^30-1.\n
 *   Note, this setting is useless if a cycle time zero in \a pTaskDesc->tiCycleInMs
 * specifies a non regular task. \a tiFirstActivationInMs needs to be zero in this case,
 * too.
 */
unsigned int rtos_registerTask( const rtos_taskDesc_t *pTaskDesc
                              , unsigned int tiFirstActivationInMs
                              )
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
    
    /* Check settings for non regularly activated tasks. */
    if(pTaskDesc->tiCycleInMs == 0)
    {
        /* Avoid a useless and misleading setting. */
        if(tiFirstActivationInMs != 0)
        {
            assert(false);
            return (unsigned int)-1;
        }
    }
    
    /* The full 32 Bit range is avoided for time designations in order to have safe and
       unambiguous before and after decisions in a cyclic time model. */
    else if(((pTaskDesc->tiCycleInMs | tiFirstActivationInMs) & 0xc0000000) != 0)
    {
        assert(false);
        return (unsigned int)-1;
    }
    
    /* @todo There's likely no issue with re-registering tasks at run-time in order to
       change the task function, the cycle time or the priority. (Exception: Task function
       and priority can't be altered from the task, which is going to be re-registered -
       this is a contraint of ihw_installINTCInterruptHandler.) Consider a critical section
       here, to enable this feature or provide dedicated API functions to alter one or all
       of the three parameters. */

    /* Add the new task to the array. */             
    _taskAry[_noTasks].taskDesc = *pTaskDesc;

    /* Initialize the dynamic task data. */
    _taskAry[_noTasks].tiDue = tiFirstActivationInMs;
    _taskAry[_noTasks].noActivationLoss = 0;

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
 * Activate a task. A task, which had been registered with cycle time zero is normally not
 * executed. This function can be called from any other task or ISR to make it run once,
 * i.e. its task function is exceuted once as result of the activation call.\n
 *   Note, the system respects the priority of the activated task. If a task of priority
 * higher than the activating task is activated then the activating task is immediately
 * preempted to the advantage of the activated task. Otherwise the activated task is
 * chained and executed after the activating task.
 *   @return
 * There is no acivation queueing. If the activated task is already activated (i.e. it is
 * in ready or running state) then no (further) activation is possible. The function
 * returns \a false and the activation loss counter of the task is incremented. (See
 * rtos_getNoActivationLoss().)
 *   @param
 * The ID of the task to activate as it had been got by the registering call for that task.
 * (See rtos_registerTask().)
 *   @remark
 * The function is indented to start a non cyclic task by application software trigger but
 * can be applied to cyclic tasks, too. In which case the task function of the cyclic task
 * would be invoked once addtionally. Note, that a task activation loss is not unliekly in
 * this case; the cyclic task may currently be busy.
 *   @remark
 * It is not forbidden but useless to let a task activate itself. This will have no effect
 * besides incrementing the activation loss counter for that task.
 */
bool rtos_activateTask(unsigned int taskId)
{
    assert(taskId < _noTasks);
    
    /* The tasks are related tothe eight available software interrupts. Each SI is
       controlled by two bits in one out of two status registers of the INTC.
         Note, there is a regular Byte-wise arrangment of the bit pairs and an access like
       *((uint8_t*)baseAddress+idxTask) seems to be possible. However, the reference manual
       doesn't mention the option of a single Byte access while it does do this for several
       other I/O devices having a similar register structure. Therefore, we don't use a
       direct Byte access but split the task index into register address and bit pair
       position in order to apply a normal 32 Bit access. */
    vuint32_t *pINTC_SSCIR;
    uint32_t mask = 0x03000000;
    if(taskId <= 3)
    {
        pINTC_SSCIR = &INTC.SSCIR0_3.R;
        mask >>= 8*taskId;
    }
    else
    {
        pINTC_SSCIR = &INTC.SSCIR4_7.R;
        mask >>= 8*(taskId-4);
    }
    
    /* Read software interrupt bit of the task. If it is still set then we have a task
       overrun otherwise we activate the task by requesting the related software
       interrupt. */
    if((*pINTC_SSCIR & mask) == 0)
    {
        /* Put task into ready state (and leave the activation to the INTC). It
           is important to avoid a read-modify-write operation, don't simply
           set a single bit, the other three interrupts in the same register
           could be harmed. */
        *pINTC_SSCIR = mask;
        
        /* Operation successful. */
        return true;
    }
    else
    {
        /* CLRi is still set, interrupt has not completed yet, task has not terminated yet.
             This code requires a critical section. The loss counter can be written
           concurrently from the task scheduler in case of cyclic tasks. */
        const uint32_t msr = ihw_enterCriticalSection();
        {
            const unsigned int noActivationLoss = _taskAry[taskId].noActivationLoss + 1;
            if(noActivationLoss > 0)
                _taskAry[taskId].noActivationLoss = noActivationLoss;
        }
        ihw_leaveCriticalSection(msr);
        
        /* Activation failed. */
        return false;
    }
} /* End of rtos_activateTask */




//unsigned int rtos_getNoActivationLoss(void)
//{
//} /* End of rtos_getNoActivationLoss */




/**
 * Initialization of the RTOS kernel. Can be called before or after the External Interrupts
 * are enabled at the CPU (see ihw_resumeAllInterrupts()).
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
    PIT.TCTRL0.R = 0x3;
    
    /* Enable timer operation and let them be stopped on debugger entry. Note, this is a
       global setting for all four timers, even if we use and reserve only one for the
       RTOS. */
    PIT.PITMCR.R = 0x1;

} /* End of rtos_initKernel */



