#ifndef SYC_SYSTEMCONFIGURATION_INCLUDED
#define SYC_SYSTEMCONFIGURATION_INCLUDED
/**
 * @file syc_systemConfiguration.h
 * Definition of global interface of module syc_systemConfiguration.c
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

#include "typ_types.h"


/*
 * Defines
 */

/** Helper for definition of process owned data objects. Is mapped onto #BSS_P1. */
#define BSS_PRC_REPORT(var)     BSS_P1(var)

/** Helper for definition of process owned data objects. Is mapped onto #DATA_P1. */
#define DATA_PRC_REPORT(var)    DATA_P1(var)

/** Helper for definition of process owned data objects. Is mapped onto #SBSS_P1. */
#define SBSS_PRC_REPORT(var)    SBSS_P1(var)

/** Helper for definition of process owned data objects. Is mapped onto #SDATA_P1. */
#define SDATA_PRC_REPORT(var)   SDATA_P1(var)

/** Helper for definition of process owned data objects. Is mapped onto #BSS_P2. */
#define BSS_PRC_FAIL(var)       BSS_P2(var)

/** Helper for definition of process owned data objects. Is mapped onto #DATA_P2. */
#define DATA_PRC_FAIL(var)      DATA_P2(var)

/** Helper for definition of process owned data objects. Is mapped onto #SBSS_P2. */
#define SBSS_PRC_FAIL(var)      SBSS_P2(var)

/** Helper for definition of process owned data objects. Is mapped onto #SDATA_P2. */
#define SDATA_PRC_FAIL(var)     SDATA_P2(var)

/** Helper for definition of process owned data objects. Is mapped onto #BSS_P3. */
#define BSS_PRC_SV(var)         BSS_P3(var)

/** Helper for definition of process owned data objects. Is mapped onto #DATA_P3. */
#define DATA_PRC_SV(var)        DATA_P3(var)

/** Helper for definition of process owned data objects. Is mapped onto #SBSS_P3. */
#define SBSS_PRC_SV(var)        SBSS_P3(var)

/** Helper for definition of process owned data objects. Is mapped onto #SDATA_P3. */
#define SDATA_PRC_SV(var)       SDATA_P3(var)


/*
 * Global type definitions
 */

/** The IDs of the processes in use. Note, the chosen number is the privledge level at the
    same time; the higher the number the higher the privileges. The permitted range is 1 ...
    #PRC_NO_PROCESSES. */
enum
{
    syc_pidReporting = 1 /// Unaffected, parallel running tasks for reporting and normal tests
    , syc_pidFailingTasks = 2   /// Process, where the failure producing tasks belong to
    , syc_pidSupervisor = 3     /// Control and evaluation tasks, watchdog

    , tmpNoPrc
    , syc_noProcessesInUse = tmpNoPrc-1
};


/** The enumeration of all events; the values are the event IDs. Actually, the ID is
    provided by the RTOS at runtime, when creating the event. However, it is guaranteed
    that the IDs, which are dealt out by rtos_createEvent() form the series 0, 1, 2, ...,
    7. So we don't need to have a dynamic storage of the IDs; we define them as constants
    and double-check by assertion that we got the correct, expected IDs from
    rtos_createEvent(). Note, this requires that the order of creating the events follows
    the order here in the enumeration. */
enum
{
    syc_idEvReporting = 0   /// Slow clock for printing results to the console
    , syc_idEvTest      /// Used for failure task and the controlling supervisor tasks
    , syc_idEvTestCtxSw /// Unrelated clock used to drive independent test of context switching
    , syc_idEvPIT2      /// Asynchronous event, used for high priority watchdog in
                        /// supervisor process 
    , syc_idEv17ms      /// Used for low priority task in failure process
    
    , syc_noEvents
};
 
 
/*
 * Global data declarations
 */

/** The current, averaged CPU load in tens of percent. */
extern volatile unsigned int syc_cpuLoad;

/** A counter of the invocations of the otherwise useless PIT3 ISR. */
extern volatile unsigned long long syc_cntISRPit3;


/*
 * Global prototypes
 */



#endif  /* SYC_SYSTEMCONFIGURATION_INCLUDED */
