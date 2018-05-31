/**
 * @file sch_scheduler.c
 * This module shows, how a real time scheduler can be build on the kernelBuilder API. The
 * implementation is a demonstration of the capabilities and not meant a code sample for
 * good design of an RTOS kernel. Instead of shaping reusable structures the implementation
 * is intentionally as lean as possible; it uses hardcoded decisions rather than
 * configurable structures.\n
 *   Most elements of a true RTOS kernel implementation are found in this sample, too.\n
 *   The scheduler manages five tasks. Task A is an infinitely running task, which
 * implements a real time task: It becomes ready by regular timer interrupt, executes its
 * task action and suspends - until next timer interrupt, and so on.\n
 *   Task B and C implement real time tasks, too. However, they are implemented as single
 * shot tasks. They are created on the fly on timer interrupt and terminate after execution
 * of their specific task action. B has a higher priority as C and B will never suspend
 * voluntarily so that these two can share the stack.\n
 *   Task D and E demonstrate a cooperative producer/consumer model. D is constantly
 * producing an artifact and suspending. E is constantly waiting of a new artifact, reports
 * it and suspends. One of these two is always ready. Both have a low priority to avoid
 * starvation of tasks A..C. All computation time not consumed by A..C will be consumed by
 * D and E.\n
 *   D and E suspend and resume regularly but they have the lowest priority at the same
 * time and will surely be inactive as long as C is ready. Therefore, C can share the stack
 * with one of the two. The sample uses stack sharing for the group B, C and E.\n
 *   Single shot tasks, which do not suspend during their life time, can share the stack
 * with the idle task, too. Task set B, C, idle would be an alternative to B, C, E but this
 * is not implemented in the sample.
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
 *   int_fctOnContextEnd
 *   sch_sc_suspend
 *   sch_sc_activate
 *   sch_initAndStartScheduler
 * Local functions
 *   taskA
 *   taskB
 *   taskC
 *   taskD
 *   taskE
 *   activateTask
 *   isrSystemTimerTick
 *   startSystemTimer
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "MPC5643L.h"
#include "ihw_initMcuCoreHW.h"
#include "int_defStackFrame.h"
#include "int_interruptHandler.h"
#include "tsk_taskFunction.h"
#include "ccx_createContext.h"
#include "sc_systemCalls.h"
#include "sch_scheduler.h"


/*
 * Defines
 */
 
/** The number of interrupt levels, we use in this application is required for an
    estimation of the appropriate stack sizes.\n
      We have 2 interrupts for the serial interface and the RTOS system timer. */
#define NO_IRQ_LEVELS_IN_USE    3

/** The stack usage by the application tasks itself; interrupts disregarded here. */
#define STACK_USAGE_IN_BYTE     512

/** The stack size for a single task. */
#define STACK_SIZE_IN_BYTE \
            (REQUIRED_STACK_SIZE_IN_BYTE(STACK_USAGE_IN_BYTE, NO_IRQ_LEVELS_IN_USE))
            
/** The stack size for the task group B, C and E.
      3* : Three tasks share the same stack. The per task usage is the threefold, while the
   reserve for the interrupts has to be considered only once. This is effectively the only
   advantage of stack sharing. */
#define STACK_SIZE_IN_BYTE_B_C_E \
            (REQUIRED_STACK_SIZE_IN_BYTE(3*STACK_USAGE_IN_BYTE, NO_IRQ_LEVELS_IN_USE))

/** A macro to help estimating the appropriate stack size. The stack size in byte is
    derived from the macro arguments \a stackRequirementTaskInByte and \a
    noUsedIrqLevels.\n
      Furthermore, the computed value is rounded in order to consider the alignment
    constraints of a PowerPC stack.
      @param stackRequirementTaskInByte
    The number of bytes requires by the task code itself. This value needs to be estimated
    by the function designer.
      @param noUsedIrqLevels
    The number of interrupt levels in use. This needs to include all interrupts, from all
    I/O drivers and from the kernel. The macro considers the worst case stack space
    requirement for the stack frames for these interrupts and adds it to the task's own
    requirement. */
#define REQUIRED_STACK_SIZE_IN_BYTE(stackRequirementTaskInByte, noUsedIrqLevels) \
            ((((noUsedIrqLevels)*(S_I_StFr)+(S_SC_StFr)+(stackRequirementTaskInByte))+7)&~7)


/** Double check configuration: This sample makes use of stack sharing. */
#if INT_USE_SHARED_STACKS != 1
# error This sample uses stack sharing but stack sharing is not enabled by configuration
#endif


/*
 * Local type definitions
 */
 
/** The enumeration of all tasks managed by the scheduler. */
typedef enum idTask_t
{ idTaskA
, idTaskB
, idTaskC
, idTaskD
, idTaskE
, noTasks
, idTaskIdle = noTasks
, idTaskInvalid

} idTask_t;


/** The state of a task. */
typedef struct taskState_t
{
    /** The task can be ready to execute or not. */
    bool isReady;
    
    /** When the task became ready and is activated: Is it created on-the-fly or is it
        resumed from suspended state? */
    bool isNew;
    
    /** If a ready task is de-activated and this flag is set then the task terminates. */
    bool isTerminating;
    
    /** If a task is about to become ready: Is this possible or is it already ready, which
        would be a task overrun? Overrun events are counted. */ 
    unsigned int noOverruns;
    
} taskState_t;


/*
 * Local prototypes
 */
 
 
/*
 * Data definitions
 */
 
/** The descriptors of the context save areas of tasks.
      +1: Idle task is initially suspended (forever in this sample) and requires context
    save support. */
static int_contextSaveDesc_t _contextSaveDescAry[noTasks+1];
 
/** The stack for task A. */
static _Alignas(uint64_t) uint8_t _stackTaskA[STACK_SIZE_IN_BYTE];

/** The stack for task D. */
static _Alignas(uint64_t) uint8_t _stackTaskD[STACK_SIZE_IN_BYTE];

/** The stack for task group B, C, E. */
static _Alignas(uint64_t) uint8_t _stackTaskBCE[STACK_SIZE_IN_BYTE_B_C_E];

/** The active task. */
static idTask_t _idActiveTask = idTaskD;

/** The readiness of all tasks (including idle) and more state information. */
static taskState_t _taskStateAry[noTasks+1] =
{ [0 ... noTasks-1] =
    { .isReady = false
    , .isNew = false
    , .isTerminating = false
    , .noOverruns = 0
    },
  [idTaskIdle] =
    { .isReady = true
    , .isNew = false
    , .isTerminating = false
    , .noOverruns = 0
    },
};


/*
 * Function implementation
 */

/**
 * Task entry function of taskA.
 *   @param taskParam
 * Task function argument: The passed value is defined by the system call that creates the
 * task context.
 */
static _Noreturn uint32_t taskA(uint32_t taskParam ATTRIB_UNUSED)
{
    while(true)
    {
        tsk_taskA_reportTime();
        sc_suspend(/* signal */ 0);
    }
} /* End of taskA */



/**
 * Task entry function of taskB.
 *   @param taskParam
 * Task function argument: The passed value is defined by the system call that creates the
 * task context.
 */
static _Noreturn uint32_t taskB(uint32_t taskParam ATTRIB_UNUSED)
{
    static unsigned int noCycles_ = 0;
    ++ noCycles_;
    
    /* Call task function, which performs the action. */
    tsk_taskB(noCycles_);

    /* Task termination of single shot task. */
    sc_suspend(/* signal */ 0);
    
    /* We will never get here because of the suspend. */
    assert(false);

} /* End of taskB */



/**
 * Task entry function of taskC.
 *   @param taskParam
 * Task function argument: The passed value is defined by the system call that creates the
 * task context.
 */
static uint32_t taskC(uint32_t taskParam ATTRIB_UNUSED)
{
    static unsigned int noCycles_ = 0;
    ++ noCycles_;
    
    /* Call task function, which performs the action. */
    tsk_taskC(noCycles_);

    /* Task C terminates by return from the task function. This means that the control goes
       into the guard function _Noreturn void int_fctOnContextEnd(uint32_t p). The return
       value of the left task function is the parameter p of the guard function. */
    return noCycles_;
    
} /* End of taskC */



/**
 * Task entry function of task D. This tasks implements the producer. It invokes the
 * production function and suspends in favor of the consumer.
 *   @param taskParam
 * Task function argument: The passed value is defined by the system call that creates the
 * task context.
 */
static _Noreturn uint32_t taskD(uint32_t taskParam ATTRIB_UNUSED)
{
    unsigned int noCycles = 0;
    while(true)
    {
        /* Produce next artifact. Do this in a redundant way such that an unwanted
           preemption by the consumer would generate recognizable faults. */
        const tsk_artifact_t *pA = tsk_taskD_produce(noCycles);
        
        /* Signal new artifact to consumer. We get back here after it has been consumed. */
        uint32_t signalFromConsumer ATTRIB_DBG_ONLY = sc_suspend(/* signal */ (uint32_t)pA);
        assert(signalFromConsumer == 0);
        
        ++ noCycles;
    }
} /* End of taskD */


 
/**
 * Task entry function of task E. This tasks implements the consumer. It invokes the
 * consumer function and suspends in favor of the producer.
 *   @param taskParam
 * Task function argument: The passed value is defined by the system call that creates the
 * task context.
 */
static _Noreturn uint32_t taskE(uint32_t taskParam ATTRIB_UNUSED)
{
    /* Task E is the initially awoken task. It is entered prior to the producer and we need
       to demand a context switch before we evaluate the first artifact. */
    uint32_t signalToProducer = 0;
    while(true)
    {
        uint32_t signalFromProducer = sc_suspend(signalToProducer);
        signalToProducer = (uint32_t)tsk_taskE_consume
                                            ((const tsk_artifact_t *)signalFromProducer);
    }
} /* End of taskE */



 
/** 
 * Select to task to activate and return from interrupt specifying this task for further
 * execution.
 *   @return
 * The function returns \a int_rcIsr_doNotSwitchContext if it returns to the preempted context
 * and \a int_rcIsr_switchContext if it demands the switch to another context.
 *   @param pCmdContextSwitch
 * Interface with the assembler code that implements the IVOR #8 handler.\n
 *   If the system call returns to a context, which had suspended in a system call (this
 * one or another one) then it can put the value to be returned to that context in \a
 * *pCmdContextSwitch.\n
 *   If the function returns \a true to request a context switch then it'll write
 * references to the descriptors of the suspended and resumed contexts into the same data
 * structure.
 *   @param signal
 * If the resumed task had been suspend using this system call itself then it'll receive \a
 * signal as result from its system call. In fact, signal is used to transmit a value from
 * the suspending task to the other task it'll resume at the same time.\n
 *   If the resumed task had been suspended in an External Interrupt then \a signal will
 * have no effect.
 */
static uint32_t activateTask( int_cmdContextSwitch_t *pCmdContextSwitch
                            , uint32_t signal
                            )
{
    /* Look for the active task of highest priority. The behavior of our tasks D and E
       ensures that there's always such a task - otherwise we could resume the initial
       context of the C main function as idle task context (most RTOS implementation decide
       this way). */
    unsigned int idTaskToActivate = 0;
    while(!_taskStateAry[idTaskToActivate].isReady)
    {
        ++ idTaskToActivate;
        assert(idTaskToActivate < noTasks);
    }
    
    /* The return value is defined only if we resume to a context, which had earlier been
       suspend by a system call. However, it doesn't harm to set it always. */
    pCmdContextSwitch->signalToResumedContext = signal;
    
    /* Do we end the interrupt with context switch? */
    uint32_t rcIsr_cmd;
    if(idTaskToActivate != _idActiveTask)
    {
        /* Demand context switch to a resumed or a new context on return from interrupt. */
        rcIsr_cmd = int_rcIsr_switchContext;

        if(_taskStateAry[_idActiveTask].isTerminating)
        {
            assert(_idActiveTask == idTaskB  ||  _idActiveTask == idTaskC);
            
            /* The flag terminateSupendedTask is set when the single shot tasks B and C
               signaled their termination. The information is returned to the IVOR #8
               handler by return code. 
                 On leave of this function, we will evidently switch to another task. The
               context switch code will not save the current stack pointer but restore the
               value it had had at context creation. This enables the other task (or same
               task later in case of re-activation) to safely reuse the same stack. */
            rcIsr_cmd |= int_rcIsr_terminateLeftContext;
            
            /* The complete scheduler implementation is race condition free and we can
               acknowledge the termination request by a simple reset of the flag. */
            _taskStateAry[_idActiveTask].isTerminating = false;
        }
        
        /* Single shot tasks are different: There stack and context save descriptor are
           reinitialized just like the task itself. */
        if(_taskStateAry[idTaskToActivate].isNew)
        {
            assert((idTaskToActivate == idTaskB
                    && _contextSaveDescAry[idTaskToActivate].fctEntryIntoContext
                       == taskB
                    ||  idTaskToActivate == idTaskC
                        && _contextSaveDescAry[idTaskToActivate].fctEntryIntoContext
                        == taskC
                   ) &&  _contextSaveDescAry[idTaskToActivate].privilegedMode
                  );

            /* The aimed context is newly created on return from interrupt. */
            rcIsr_cmd |= int_rcIsr_createEnteredContext;
            
            /* The complete scheduler implementation is race condition free and we can
               acknowledge the task creation request by a simple reset of the flag. */
            _taskStateAry[idTaskToActivate].isNew = false;
        }

        pCmdContextSwitch->pSuspendedContextSaveDesc = &_contextSaveDescAry[_idActiveTask];
        pCmdContextSwitch->pResumedContextSaveDesc = &_contextSaveDescAry[idTaskToActivate];
        
        /* The scheduler needs to keep track, who is currently active. */
        _idActiveTask = idTaskToActivate;
    }
    else
    {    
        /* No context switch on return from interrupt. The save area descriptors don't
           care. (Note, the return value does - we could return to the same context, which
           had invoked a system call.) */
        rcIsr_cmd = int_rcIsr_doNotSwitchContext;
    }
    
    return rcIsr_cmd;
    
} /* End of activateTask */



/**
 * Make a task ready. This is possible only if it is not yet ready (overrun event).
 *   @param idTask
 * The task to become ready.
 */
static void makeTaskReady(idTask_t idTask)
{
    assert(idTask < noTasks);
    if(!_taskStateAry[idTask].isReady)
    {
        _taskStateAry[idTask].isReady = true;
        
        /* The single-shot tasks need to be created on the fly when being activated the
           first time. */
        if(idTask == idTaskB  ||  idTask == idTaskC)
            _taskStateAry[idTask].isNew = true;
    }
    else
        ++ _taskStateAry[idTask].noOverruns;
        
} /* End of makeTaskReady */



/**
 * Each call of this function cyclically increments the system time of the kernel by one.
 * The interrupt handler decides whether a task becomes ready in this tick and ends with
 * activate the one, which has the highest priority of all currently ready tasks.
 *   @return
 * The function returns \a int_rcIsr_doNotSwitchContext if it returns to the preempted context
 * and \a int_rcIsr_switchContext if it demands the switch to another context.
 *   @param pCmdContextSwitch
 * Interface with the assembler code that implements the IVOR #4 handler.\n
 *   If the system call returns to a context, which had suspended in a system call (this
 * one or another one) then it can put the value to be returned to that context in \a
 * *pCmdContextSwitch.\n
 *   If the function request a context switch then it'll write references to the
 * descriptors of the suspended and resumed contexts into the same data structure.
 */
static uint32_t isrSystemTimerTick(int_cmdContextSwitch_t *pCmdContextSwitch)
{
    /* Acknowledge the timer interrupt in the causing HW device. */
    assert(PIT.TFLG3.B.TIF == 0x1);
    PIT.TFLG3.B.TIF = 0x1;

    
    /* Check for all RTOS tasks if they become ready in this tick. */
    static unsigned int sysTime_ = 0
                      , nextActivationTaskA_ = 0
                      , nextActivationTaskB_ = 3
                      , nextActivationTaskC_ = 7;
    if(sysTime_ >= nextActivationTaskA_)
    {
        makeTaskReady(idTaskA);
        nextActivationTaskA_ += 10;
    }
    if(sysTime_ >= nextActivationTaskB_)
    {
        makeTaskReady(idTaskB);
        nextActivationTaskB_ += 2;
    }
    if(sysTime_ >= nextActivationTaskC_)
    {
        makeTaskReady(idTaskC);
        nextActivationTaskC_ += 7;
    }
    
    /* Increment system time. */
    ++ sysTime_;
    
    /* Return from interrupt with selection of active task. In this simple sample there's
       nothing reasonable to signal to a resumed task. */
    return activateTask(pCmdContextSwitch, /* signal */ 0);

} /* End of isrSystemTimerTick */



/**
 * This is the common guard function of the context entry functions: When a function, which
 * had been been specified as context entry function is left with return then program flow
 * goes into this guard function.
 *   @param retValOfContext
 * The guard function receives the return value of the left context entry function as
 * parameter.
 *   @remark
 * Note, the guard function has no calling parent function. Any attempt to return from
 * this function will surely lead to a crash. The normal use case is to have a system call
 * implemented here, which notifies the situation about the terminating context to the
 * scheduler. On return, the system call implementation will surely not use the option \a
 * int_rcIsr_doNotSwitchContext and control will never return back to the guard. 
 */
_Noreturn void int_fctOnContextEnd(uint32_t retValOfContext)
{
    /* In this example, the only task function, which makes use of the guard is taskC. It
       sends the number of calls to the guard. */
    assert(_idActiveTask == idTaskC);
    if(retValOfContext % 1000 == 0)
    {
        iprintf( "int_fctOnContextEnd: The %luth termination of taskC is notified\r\n"
               , retValOfContext
               );
    }

    /* Notify task termination to the scheduler. In our sample, this mechanism is used only
       for task C, which is a single shot task. However, it could also be used for any
       permanently existing, spinning tasks. In which case the scheduler could pool and later
       reuse the terminated task for whatever other purposes (or simply delete it if
       dynamic memory allocation is available). */
    sc_suspend(/* signal */ 0);
    
    /* We will never get here because of the suspend. (This is not fulfilled by principle
       but an essential requirement for the scheduler implementation.) */
    assert(false);

} /* End of int_fctOnContextEnd */



/**
 * The system call for task suspension. The state of the calling task is changed from
 * active to suspended. The remaining tasks are looked for the ready one of highest priority
 * and this one is activated.
 *   @return
 * The function returns \a int_rcIsr_doNotSwitchContext if it returns to the preempted context
 * and \a int_rcIsr_switchContext if it demands the switch to another context. The demand
 * to switch the task can be combined with the requests to terminate the suspended task
 * and/or to newly created the new task.\n
 *   This system call always switches to another context.
 *   @param pCmdContextSwitch
 * Interface with the assembler code that implements the IVOR #8 handler.\n
 *   If the system call returns to a context, which had suspended in a system call (this
 * one or another one) then it can put the value to be returned to that context in \a
 * *pCmdContextSwitch.\n
 *   If the function returns \a int_rcIsr_switchContext to request a context switch then
 * it'll write references to the descriptors of the suspended and resumed contexts into the
 * same data structure.
 *   @param signal
 * If the resumed task had been suspend using this system call itself or if it is newly
 * created then it'll receive \a signal as result from its system call. In fact, signal is
 * used to transmit a value from the suspending task to the other task it'll resume at the
 * same time.\n
 *   If the resumed task had been suspended in an External Interrupt then \a signal will
 * have no effect.
 *   @remark
 * Never call this function directly; it is invoked from the common IVOR #8 handler only.
 */
uint32_t sch_sc_suspend( int_cmdContextSwitch_t *pCmdContextSwitch
                       , uint32_t signal
                       )
{
    /* This demo doesn't define generic kernel operations but hardcodes the demonstrated
       kernel actions. We use a switch case to implement the individual behavior for each
       task. */
    assert(_idActiveTask < noTasks);
    _taskStateAry[_idActiveTask].isReady = false;
    switch(_idActiveTask)
    {
    case idTaskA:
        /* Task A is a real time task, which is implemented by regular timer awake and
           voluntary suspend after execution of its action. Nothing more to do. */
        break;

    case idTaskC:
    case idTaskB:
        /* The single shot tasks B and C use this call to signal that they have terminated. */
        _taskStateAry[_idActiveTask].isTerminating = true;
        break;

        /* Task pair D and E suspend in favor of each other.
             Note, sending a signal from E to D or vice versa as implemented here is not a
           general coding pattern. In a normal, generic scheduler implementation, a task A
           cannot easily signal the argument of a system call directly to another task
           directly by return from the system call handler. The value is returned to the
           activated task, and which one that is is usually not under control of the
           sending task. Here, in our particular sample it is possible: D and E always
           mutually resume one another and therefore the signal is delivered always to the
           other one. */
    case idTaskD:
        _taskStateAry[idTaskE].isReady = true;
        break;
    case idTaskE:
        _taskStateAry[idTaskD].isReady = true;
        break;
        
    default:
        assert(false /* Invalid task ID */);
    }
    
    /* Return from interrupt with selection of active task. */
    return activateTask(pCmdContextSwitch, signal);

} /* End of sch_sc_suspend */



/**
 * The system call for task activation. The state of the referenced task is changed ready
 * (regardless whether it already was ready). Then the tasks are looked for the ready one
 * of highest priority and this one is activated.
 *   @return
 * The function returns \a int_rcIsr_doNotSwitchContext if it returns to the preempted context
 * and \a int_rcIsr_switchContext if it demands the switch to another context.
 *   @param pCmdContextSwitch
 * Interface with the assembler code that implements the IVOR #8 handler.\n
 *   If the system call returns to a context, which had suspended in a system call (this
 * one or another one) then it can put the value to be returned to that context in \a
 * *pCmdContextSwitch.\n
 *   If the function returns \a true to request a context switch then it'll write
 * references to the descriptors of the suspended and resumed contexts into the same data
 * structure.
 *   @param idTask
 * The ID of the task to activate.
 *   @param signal
 * If the resumed task had been suspend using this system call itself then it'll receive \a
 * signal as result from its system call. In fact, signal is used to transmit a value from
 * the suspending task to the other task it'll resume at the same time.\n
 *   If the resumed task had been suspended in an External Interrupt then \a signal will
 * have no effect.\n
 *   If the resumed task is resumed the very first time after creation then \a signal has
 * the meaning of the function argument of the task entry function.
 *   @remark
 * The name of this system call has been adopted from existing RTOS although it is
 * unapproriate. The referenced task is not activated, it is made ready. It is activated
 * only in the special situation that it now is the very task of highest priority.
 *   @remark
 * Never call this function directly; it is invoked from the common IVOR #8 handler only.
 */
uint32_t sch_sc_activate( int_cmdContextSwitch_t *pCmdContextSwitch
                        , uint32_t idTask
                        , uint32_t signal
                        )
{
    makeTaskReady(idTask);

    /* Return from interrupt with selection of active task. */
    return activateTask(pCmdContextSwitch, signal);

} /* End of sch_sc_activate */



/**
 * Initialize and start the timer interrupt, which clocks our simple RTOS.
 */
static void startSystemTimer(void)
{
    /* Disable all PIT timers during configuration. */
    PIT.PITMCR.R = 0x2;

    /* Install the interrupt handler for cyclic timer PIT 3. It drives the OS scheduler for
       cyclic task activation. We choose PIT 3 since it has a significant lower priority
       than the other three. (This matters because all kernel interrupts need to share the
       same INTC priority level 1.) */ 
    ihw_installINTCInterruptHandler
                        ( (int_externalInterruptHandler_t){.kernelIsr = isrSystemTimerTick}
                        , /* vectorNum */ 127 /* Timer PIT 3 */
                        , /* psrPriority */ 1
                        , /* isPreemptable */ true
                        , /* isOsInterrupt */ true
                        );

    /* Peripheral clock has been initialized to 120 MHz. To get a 1ms interrupt tick we
       need to count till 120000.
         -1: See MCU reference manual, 36.5.1, p. 1157. */
    PIT.LDVAL3.R = 120000-1;

    /* Enable interrupts by this timer and start it. */
    PIT.TCTRL3.R = 0x3;

    /* Enable timer operation and let them be stopped on debugger entry. Note, this is a
       global setting for all four timers, even if we use and reserve only one for the
       RTOS. */
    PIT.PITMCR.R = 0x1;

} /* End of startSystemTimer */



/**
 * Module initialization and start of scheduler. This function doesn't return to the
 * caller.
 */
_Noreturn void sch_initAndStartScheduler()
{
    /* Preset all stack contents in order to make stack usage observable. */
    memset(&_stackTaskA[0], 0x5a, sizeof(_stackTaskA));
    memset(&_stackTaskD[0], 0x5a, sizeof(_stackTaskD));
    memset(&_stackTaskBCE[0], 0x5a, sizeof(_stackTaskBCE));
    
    /* Initialize the active task. To satisfy our simple task selection logic in
       activateTask() we need to ensure that the value is not identical to the ID of the
       task, which is resumed first. */
    _idActiveTask = idTaskIdle;

    /* Create all task as new execution contexts. We distinguish between self-contained
       tasks with own stack and the single-shot tasks B and C, which share the stack. The
       former are created in suspended state and can be resumed by the scheduler just like
       that. The latter need an on-the-fly start by the scheduler. */
    unsigned int idTask;
    
    /* The context creation functions require the address of the top of the stack area of
       the context as input (i.e. the start value of the stack pointer; our stacks grow
       downwards). This macro helps providing the required address. */
    #define SP(stack)   ((void*)((const uint8_t*)(stack)+sizeof(stack)))

    idTask = idTaskA;
    ccx_createContext( /* pNewContextSaveDesc */  &_contextSaveDescAry[idTask]
                     , /* stackPointer */ SP(_stackTaskA)
                     , /* fctEntryIntoContext */ taskA
                     , /* privilegedMode */ true
                     );

    /* Task E is created next so that sharing its stacl becomes most easy to implement: Its
       context save descriptor is initialized and can be referenced for cloning by the
       sharing single-shot tasks. */
    idTask = idTaskE;
    ccx_createContext( /* pNewContextSaveDesc */  &_contextSaveDescAry[idTask]
                     , /* stackPointer */ SP(_stackTaskBCE)
                     , /* fctEntryIntoContext */ taskE
                     , /* privilegedMode */ true
                     );
    
    /* Task B, started on the fly: No entry function is specified here. */
    idTask = idTaskB;
    ccx_createContextShareStack( /* pNewContextSaveDesc */  &_contextSaveDescAry[idTask]
                               , /* pPeerContextSaveDesc */ &_contextSaveDescAry[idTaskE]
                               , /* fctEntryIntoContext */ taskB
                               , /* privilegedMode */ true
                               );

    /* Task C, started on the fly, shares the stack with B. No entry function is specified
       here. */
    idTask = idTaskC;
    ccx_createContextShareStack( /* pNewContextSaveDesc */  &_contextSaveDescAry[idTask]
                               , /* pPeerContextSaveDesc */ &_contextSaveDescAry[idTaskB]
                               , /* fctEntryIntoContext */ taskC
                               , /* privilegedMode */ true
                               );

    /* Task D: Note the -1: Since B and C share the stack we have one element less than
       tasks in the array of stacks.
         Note, task D is the only one, which make no use of printf. Serial communication
       has not been migrated to the use of system calls yet and can't be used in user mode.
       Task D is the only task, we can run in user mode. Let's try it. */
    idTask = idTaskD;
    ccx_createContext( /* pNewContextSaveDesc */  &_contextSaveDescAry[idTask]
                     , /* stackPointer */ SP(_stackTaskD)
                     , /* fctEntryIntoContext */ taskD
                     , /* privilegedMode */ false
                     );

    #undef SP

    /* The context save descriptor of the idle task needs to be initialized, too. It is
       used once at the beginning, when we leave the idle context. */
    _contextSaveDescAry[idTaskIdle].ppStack = &_contextSaveDescAry[idTaskIdle].pStack;
    
    /* Initialize the interrupt, which triggers the activation of the conventional RTOS
       tasks A, B and C. */
    startSystemTimer();
    
    /* Activate the consumer task, which has been created in suspended state. The pair of
       taskD and taskE is now spinning. */
    sc_activate(/* idTask */ idTaskE, /* signal */ 0);
    
    /* Since one of our tasks D and E is always ready we will never get or return here. */
    assert(false);    
    
} /* End of sch_initAndStartScheduler */



