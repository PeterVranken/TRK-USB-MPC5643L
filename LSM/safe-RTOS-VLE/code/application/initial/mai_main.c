/**
 * @file mai_main.c
 *   C entry function. The core completes the HW initialization (clocks run at full speed,
 * drivers for MPU and devices are initialized).\n
 *   The safe-RTOS is configured to run a few tasks, both OS and user mode tasks, which
 * drive the user LEDs. The user tasks injects a very few failures in order to demonstrate
 * the error catching capabilities of the RTOS.\n
 *   Only if all the LEDs are blinking everything is alright.\n
 *   Progress information is permanently written into the serial output channel. A terminal
 * on the development host needs to use these settings: 115000 Bd, 8 Bit data word, no
 * parity, 1 stop bit.
 *
 * Copyright (C) 2017-2020 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   taskInitProcess
 *   task1ms
 *   taskOs1ms
 *   startSecondaryCore
 */

/*
 * Include files
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
#include "lbd_ledAndButtonDriver.h"
#include "sio_serialIO.h"
#include "rtos.h"
#include "del_delay.h"
#include "gsl_systemLoad.h"
//#include "mai_main.h"


/*
 * Defines
 */

/** The demo can be compiled with a ground load. Most tasks produce some CPU load if this
    switch is set to 1. */
#define TASKS_PRODUCE_GROUND_LOAD   1


/*
 * Local type definitions
 */

/** The enumeration of all events, tasks and priorities, to have them as symbols in the
    source code. Most relevant are the event IDs. Actually, these IDs are provided by the
    RTOS at runtime, when creating the event. However, it is guaranteed that the IDs, which
    are dealt out by rtos_osCreateEvent() form the series 0, 1, 2, .... So we don't need
    to have a dynamic storage of the IDs; we define them as constants and double-check by
    assertion that we got the correct, expected IDs from rtos_osCreateEvent(). Note, this
    requires that the order of creating the events follows the order here in the
    enumeration.\n
      Here, we have the IDs of the created events. They occupy the index range starting
    from zero. */
enum
{
    /** Regular timer event. */
    idEv1ms = 0,

    /** The number of tasks to register. */
    noRegisteredEvents
};


/** The RTOS uses constant priorities for its events, which are defined here.\n
      Note, the priority is a property of an event rather than of a task. A task implicitly
    inherits the priority of the event it is associated with. */
enum
{
    prioTaskIdle = 0,            /* Prio 0 is implicit, cannot be chosen explicitly */
    prioEv1ms = 1,
};


/** In safe-RTOS a task belongs to a process, characterized by the PID, 1..4. The
    relationship is defined here.\n
      Note, a process needs to be configured in the linker script (actually: assignment of
    stack space) before it can be used. */
enum
{
    pidOs = 0,              /* kernel always and implicitly has PID 0 */
    pidTask1ms = 1,
    pidTaskOs1ms = pidOs,   /* A kernel or operating system task, e.g. to implement a
                               polling I/O driver. */
    pidTaskIdle = pidOs     /* PID 0 is implicit, idle belongs to the kernel */
};




/*
 * Local prototypes
 */


/*
 * Data definitions
 */

/** Counter of cycles of infinite main loop. */
volatile unsigned long SBSS_OS(mai_cntTaskIdle) = 0;

/** Counter of cyclic 1ms user task. */
volatile unsigned long long SBSS_P1(mai_cntTask1ms) = 0;  

/** Counter of cyclic 1ms OS task. */
volatile unsigned long long SBSS_OS(mai_cntTaskOs1ms) = 0;

/** The average CPU load produced by all tasks and interrupts in tens of percent. */
unsigned int DATA_OS(mai_cpuLoad) = 1000;



/*
 * Function implementation
 */

/**
 * Initialization task of process \a PID.
 *   @return
 * The function returns the Boolean descision, whether the initialization was alright and
 * the system can start up. "Not alright" is expressed by a negative number, which hinders
 * the RTOS to startup.
 *   @param PID
 * The ID of the process, the task function is executed in.
 *   @remark
 * In this sample, we demonstrate that different processes' tasks can share the same task
 * function implementation. This is meant a demonstration of the technical feasibility but
 * not of good practice; the implementation needs to use shared memory, which may break a
 * safety constraint, and it needs to consider the different privileges of the processes.
 */
static int32_t taskInitProcess(uint32_t PID)
{
    static unsigned int cnt_ SECTION(.data.Shared.cnt_) = 0;
    ++ cnt_;

//    /* Only process 1 has access to the C lib (more precise: to those functions of the C
//       lib, which write to lib owned data objects) and can write a status message. */
//    if(PID == 1)
//        iprintf("taskInitPID%lu(): %u\r\n", PID, cnt_);

    return cnt_ == PID? 0: -1;

} /* End of taskInitProcess */



/**
 * Task function, cyclically activated every Millisecond. The LED D4 is switched on and off.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process.
 *   @param PID
 * A user task function gets the process ID as first argument.
 *   @param taskParam
 * A variable task parameter. Here just used for testing.
 */
static int32_t task1ms(uint32_t PID ATTRIB_UNUSED, uintptr_t taskParam ATTRIB_DBG_ONLY)
{
    assert(taskParam == 0);

    /* Make spinning of the task observable in the debugger. */
    ++ mai_cntTask1ms;

#if TASKS_PRODUCE_GROUND_LOAD == 1
    /* Produce a bit of CPU load. This call simulates some true application software. */
    del_delayMicroseconds(/* fullLoadThisNoMicroseconds */ 50 /* approx. 5% load */);
#endif

    static int SBSS_P1(cntIsOn_) = 0;
    if(++cntIsOn_ >= 500)
    {
        cntIsOn_ = -500;
        
        printf("This is call %llu of task1ms\r\n", mai_cntTask1ms);
    }
    lbd_setLED(lbd_led_D4_red, /* isOn */ cntIsOn_ >= 0);

    /* Inject some errors. */
    if((mai_cntTask1ms & 0x3ff) == 0)
    {
        static volatile unsigned int DATA_P2(foreignData) ATTRIB_UNUSED;
        foreignData = (unsigned int)mai_cntTask1ms;
    }
//    if((mai_cntTask1ms & 0x7ff) == 1)
//    {
//        static volatile uint8_t DATA_P1(coreId) ATTRIB_UNUSED;
//        coreId = sem_osGetCoreID();
//    }

    return 0;

} /* End of task1ms */



/**
 * OS task function, cyclically activated every Millisecond. Used to clock the step
 * functions of our I/O drivers.\n
 *   This task is run in supervisor mode and it has no protection. The implementation
 * belongs into the sphere of trusted code.
 *   @param taskParam
 * A variable task parameter. Here just used for testing.
 */
static void taskOs1ms(uintptr_t taskParam ATTRIB_DBG_ONLY)
{
    assert(taskParam == 0);

    /* Make spinning of the task observable in the debugger. */
    ++ mai_cntTaskOs1ms;

} /* End of taskOs1ms */



/**
 * C entry function main. Is used for and only for the Z7_0 core.
 *   @param noArgs
 * Number of arguments in \a argAry. Is actually always equal to zero.
 *   @param argAry
 * Array of string arguments to the function. Is actually always equal to NULL.
 */
int main(int noArgs ATTRIB_DBG_ONLY, const char *argAry[] ATTRIB_DBG_ONLY)
{
    /* The arguments of the main function are quite useless. Just check correctness. */
    assert(noArgs == 0  && argAry == NULL);


    /* The first operation of the main function is the call of ihw_initMcuCoreHW(). The
       assembler implemented startup code has brought the MCU in a preliminary working
       state, such that the C compiler constructs can safely work (e.g. stack pointer is
       initialized, memory access through MMU is enabled).
         ihw_initMcuCoreHW() does the remaining hardware initialization, that is still
       needed to bring the MCU in a basic stable working state. The main difference to the
       preliminary working state of the assembler startup code is the selection of
       appropriate clock rates.
         This part of the hardware configuration is widely application independent. The
       only reason, why this code has not been called directly from the assembler code
       prior to entry into main() is code transparency. It would mean to have a lot of C
       code without an obvious point, where it is called. */
    ihw_initMcuCoreHW();
    
    /* The core is now running in the desired state. I/O driver initialization follows to
       the extend required by this simple sample. */

    /* The interrupt controller is configured. This is the first driver initialization
       call: Many of the others will register their individual ISRs and this must not be
       done prior to initialization of the interrupt controller. */
    rtos_osInitINTCInterruptController();

    /* Initialize the button and LED driver for the eval board. */
    lbd_osInitLEDAndButtonDriver( /* onButtonChangeCallback */ NULL
                                , /* pidOnButtonChangeCallback */ 0
                                );

    /* Initialize the serial output channel as prerequisite of using printf. */
    sio_osInitSerialInterface(/* baudRate */ 115200);

    /* Register the process initialization tasks. */
    bool initOk = true;
    if(rtos_osRegisterInitTask(taskInitProcess, /* PID */ 1, /* tiTaskMaxInUS */ 1000)
       != rtos_err_noError
      )
    {
        initOk = false;
    }

    /* Create the events that trigger application tasks at the RTOS. Note, we do not really
       respect the ID, which is assigned to the event by the RTOS API rtos_osCreateEvent().
       The returned value is redundant. This technique requires that we create the events
       in the right order and this requires in practice a double-check by assertion - later
       maintenance errors are unavoidable otherwise. */
    unsigned int idEvent;
    if(rtos_osCreateEvent( &idEvent
                         , /* tiCycleInMs */              1
                         , /* tiFirstActivationInMs */    10
                         , /* priority */                 prioEv1ms
                         , /* minPIDToTriggerThisEvent */ RTOS_EVENT_NOT_USER_TRIGGERABLE
                         , /* taskParam */                0
                         )
       == rtos_err_noError
      )
    {
        assert(idEvent == idEv1ms);

        if(rtos_osRegisterOSTask(idEv1ms, taskOs1ms) != rtos_err_noError)
            initOk = false;

        if(rtos_osRegisterUserTask( idEv1ms
                                  , task1ms
                                  , pidTask1ms
                                  , /* tiTaskMaxInUs */ 0
                                  )
           != rtos_err_noError
          )
        {
            initOk = false;
        }
    }
    else
        initOk = false;

    /* The last check ensures that we didn't forget to register a task. */
    assert(idEvent == noRegisteredEvents-1);

    /* Initialize the RTOS kernel. The global interrupt processing is resumed if it
       succeeds. The step involves a configuration check. We must not startup the SW if the
       check fails. */
    if(!initOk ||  rtos_osInitKernel() != rtos_err_noError)
        while(true)
            ;

    /* The code down here becomes the idle task of the RTOS. We enter an infinite loop,
       where some background can be placed. */
    while(true)
    {
        /* Compute the average CPU load. Note, this operation lasts about 1s and has a
           significant impact on the cycling speed of this infinite loop. Furthermore, it
           measures only the load produced by the tasks and system interrupts but not that
           of the rest of the code in the idle loop. */
        mai_cpuLoad = gsl_getSystemLoad();
        char msg[128];
        const int noChar = sniprintf( msg, sizeof(msg)
                                    , "CPU load: %u%%\r\n"
                                    , (mai_cpuLoad+5)/10
                                    );
        assert((unsigned)noChar < sizeOfAry(msg));
        sio_osWriteSerial(msg, (unsigned)noChar);

        static bool SBSS_OS(isOn_) = false;
        lbd_osSetLED(lbd_led_D5_red, isOn_ = !isOn_);

        /* Make spinning of the idle task observable in the debugger. */
        ++ mai_cntTaskIdle;

        /* Supervisor code like an OS or the idle task must not use the C lib's I/O
           functions like printf because they go into a system call handler of class full,
           which is not permitted for use from supervisor code. */
        sio_osWriteSerial(SIO_STR(Hello World!));

    } /* End of inifinite idle loop of RTOS. */

} /* End of main */
