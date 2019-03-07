/**
 * @file prr_processReporting.c
 * Implementation of task functions of process \a syc_pidReporting. This process has the
 * special ID 1, which makes the C libry accessible. The principal task from this process
 * uses printf from the library to regularly print status messages to the serial output.
 * The tasks of this process are not involved into the testing and we expect them to
 * continuously run, not being harmed by the failures produced by the other processes.\n
 *   The tasks and their implementation belong into the sphere of the protected user code.
 * They are are defined in the sphere of unprotected operating system code and anything
 * which relates to their configuration cannot be changed anymore by user code.
 *
 * Copyright (C) 2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   prr_taskReporting
 *   prr_taskTestContextSwitches
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

#include "typ_types.h"
#include "rtos.h"
#include "gsl_systemLoad.h"
#include "tcx_testContext.h"
#include "syc_systemConfiguration.h"
#include "prr_processReporting.h"


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
 
/** For debugging only: Exceution time of untrusted C lib function in CPU clock ticks. */
uint64_t SDATA_PRC_REPORT(prr_tiMaxDurationPrintf) = 0;


/*
 * Function implementation
 */

/**
 * Task function, cyclically activated every about 1000 ms. Used to print status
 * information to the serial output.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process.
 *   @param PID
 * A user task function gets the process ID as first argument.
 */
int32_t prr_taskReporting(uint32_t PID ATTRIB_UNUSED)
{
    const uint64_t tiStart = GSL_PPC_GET_TIMEBASE();
    iprintf( "CPU load is %u.%u%%. Stack reserve:\r\n"
             "  OS: %u Byte\r\n"
             "  PID 1: %u Byte\r\n"
             "  PID 2: %u Byte\r\n"
             "  PID 3: %u Byte\r\n"
             "Task activations (lost):\r\n"
#if 0
             "  task1ms: %lu (%u)\r\n"
             "  task3ms: %lu (%u)\r\n"
             "  task1s: %lu (%u)\r\n"
             "  taskNonCyclic: %lu (%u)\r\n"
             "  task17ms: %lu (%u)\r\n"
             "  taskOnButtonDown: %lu (%u)\r\n"
             "  taskCpuLoad: %lu (%u)\r\n"
#endif
             "  isrPit3: %llu (N/A)\r\n"
             "Process errors:\r\n"
             "  Total PID 1: %u\r\n"
             "  thereof Deadline missed: %u\r\n"
             "  Total PID 2: %u\r\n"
             "  thereof Deadline missed: %u\r\n"
             "  thereof User task abort: %u\r\n"
             "  Total PID 3: %u\r\n"
             "  thereof Deadline missed: %u\r\n"
           , syc_cpuLoad/10, syc_cpuLoad%10
           , rtos_getStackReserve(0)
           , rtos_getStackReserve(1)
           , rtos_getStackReserve(2)
           , rtos_getStackReserve(3)
#if 0
           , mai_cntTask1ms, rtos_getNoActivationLoss(idEv1ms)
           , mai_cntTask3ms, rtos_getNoActivationLoss(idEv3ms)
           , mai_cntTask1s, rtos_getNoActivationLoss(idEv1s)
           , mai_cntTaskNonCyclic, rtos_getNoActivationLoss(idEvNonCyclic)
           , mai_cntTask17ms, rtos_getNoActivationLoss(idEv17ms)
           , mai_cntTaskOnButtonDown, rtos_getNoActivationLoss(idEvOnButtonDown)
           , mai_cntTaskCpuLoad, rtos_getNoActivationLoss(idEvCpuLoad)
#endif
           , syc_cntISRPit3
           , rtos_getNoTotalTaskFailure(/* PID */ 1)
           , rtos_getNoTaskFailure(/* PID */ 1, IVR_CAUSE_TASK_ABBORTION_DEADLINE)
           , rtos_getNoTotalTaskFailure(/* PID */ 2)
           , rtos_getNoTaskFailure(/* PID */ 2, IVR_CAUSE_TASK_ABBORTION_DEADLINE)
           , rtos_getNoTaskFailure(/* PID */ 2, IVR_CAUSE_TASK_ABBORTION_USER_ABORT)
           , rtos_getNoTotalTaskFailure(/* PID */ 3)
           , rtos_getNoTaskFailure(/* PID */ 3, IVR_CAUSE_TASK_ABBORTION_DEADLINE)
           );
    const uint64_t tiDuration = GSL_PPC_GET_TIMEBASE() - tiStart;
    
    if(tiDuration > prr_tiMaxDurationPrintf)
        prr_tiMaxDurationPrintf = tiDuration;
        
    return 0;
    
} /* End of prr_taskReporting */


/**
 * Task function, cyclically activated every 11ms. It executes an assembler function, which
 * aims at testing correct context save/restore for most user registers and across context
 * switches.\n
 *   The function is blocking for a relative long while to provoke a lot of context
 * switches during testing. This produces significant CPU load.\n
 *   In DEBUG compilation any error will run into an assertion and halt the software. In
 * PRODUCTION compilation a process error is accounted. This is a clear problem report
 * because the process is designed to be at all error free.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process.
 *   @param PID
 * A user task function gets the process ID as first argument.
 */
int32_t prr_taskTestContextSwitches(uint32_t PID ATTRIB_UNUSED)
{
    /* The next call produces 100 * noCycles*(waitTimePerCycleInUS/1000) / 11 percent of
       CPU load. */
    tcx_testContext(/* noCycles */ 2, /* waitTimePerCycleInUS */ 2000);

    return 0;
    
} /* End of prr_taskTestContextSwitches */



