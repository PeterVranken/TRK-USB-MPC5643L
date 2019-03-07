#ifndef PRF_PROCESSFAILUREINJECTION_INCLUDED
#define PRF_PROCESSFAILUREINJECTION_INCLUDED
/**
 * @file prf_processFailureInjection.h
 * Definition of global interface of module prf_processFailureInjection.c
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

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>


/*
 * Defines
 */


/*
 * Global type definitions
 */

/** The type of the command to inject the next error. Used in communication between the
    tasks prs_taskCommandError and prf_taskInjectError. */
typedef struct prf_cmdFailure_t
{
    /** Which error? */
    enum prf_kindOfFailure_t
    {
        prf_kof_jumpToResetVector
        , prf_kof_jumpToIllegalInstr
        , prf_kof_noFailure
        , prf_kof_userTaskError
        , prf_kof_privilegedInstr
        , prf_kof_callOsAPI
        , prf_kof_triggerUnavailableEvent
        , prf_kof_writeOsData
        , prf_kof_writeOtherProcData
        , prf_kof_infiniteLoop
        //, prf_kof_writeROM
        //, prf_kof_writePeripheral
        //, prf_kof_readPeripheral
        //, prf_kof_misalignedWrite
        //, prf_kof_misalignedRead
        
        , prf_kof_noFailureTypes    /** Total number of defined failure kinds */
        
    } kindOfFailure;
    
    /** This error to injected in which stack depth? */
    unsigned int noRecursionsBeforeFailure;
    
    /** General purpose argument for test case. */
    uint32_t value;
    
    /** General purpose pointer argument for test cases. */
    uint32_t address;
    
    /** Expected number of process errors resulting from the failure. */
    unsigned int expectedNoProcessFailures;
    
    /** Depending on the number of possibly affected tasks there may be an unsharpness in
        predicting the number of expected process errors. */
    unsigned int expectedNoProcessFailuresTolerance;
    
    /** Expected value for test case result. */
    uint32_t expectedValue;

} prf_cmdFailure_t;



/*
 * Global data declarations
 */

/** The next error to inject. This object is written by task prs_taskCommandError and read
    by prf_taskInjectError. There are no race conditions between these two tasks. */
extern prf_cmdFailure_t DATA_SHARED(prf_cmdFailure);


/*
 * Global prototypes
 */

/** User task intenionally producing severe errors, which would crash an unprotected RTOS. */
int32_t prf_taskInjectError(uint32_t PID);

/** User task in same process, running at lower priority. */
int32_t prf_task17ms(uint32_t PID);

/** User task in same process, running at higher priority. */
int32_t prf_task1ms(uint32_t PID, uint32_t taskParam);

#endif  /* PRF_PROCESSFAILUREINJECTION_INCLUDED */
