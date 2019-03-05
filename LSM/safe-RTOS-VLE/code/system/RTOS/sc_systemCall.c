/**
 * @file sc_systemCall.c
 *
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
 * Module inline interface
 *   sc_checkUserCodeReadPtr
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
#include "sc_systemCall.h"

/*
 * Include section for I/O driver header files.
 *   The header files, which are included here declare the system calls, which are
 * implemented by the I/O drivers. Using preprocessor constructs we check for multiple
 * definitions of the same system call and we can add missing ones.
 */
#include "assert_defSysCalls.h"
#include "ivr_ivorHandler_defSysCalls.h"
#include "pcp_sysCallPCP_defSysCalls.h"
#include "prc_process_defSysCalls.h"
#include "rtos_defSysCalls.h"
#include "lbd_ledAndButtonDriver_defSysCalls.h"
#include "sio_serialIO_defSysCalls.h"
//#include "mai_main_defSysCalls.h"


/*
 * Defines
 */

/** This table entry is used for those system table entries, which are not defined by any
    included I/O driver header file. The dummy table entry points to a no-operation
    service, which silently returns to the caller. */
#define SC_SYSCALL_DUMMY_TABLE_ENTRY                                    \
            { .addressOfFct = (uint32_t)ivr_scBscHdlr_sysCallUndefined  \
            , .conformanceClass = SC_HDLR_CONF_CLASS_BASIC              \
            }

/* The preprocessor code down here filters all system call table entries, which are not
   defined by any of the included I/O driver header files and makes them become the dummy
   entry #SC_SYSCALL_DUMMY_TABLE_ENTRY. */
#ifndef SC_SYSCALL_TABLE_ENTRY_0000
# error System call 0 has not been defined. This system call is required to terminate \
        a user task and is mandatory
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0001
# define SC_SYSCALL_TABLE_ENTRY_0001    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0002
# define SC_SYSCALL_TABLE_ENTRY_0002    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0003
# define SC_SYSCALL_TABLE_ENTRY_0003    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0004
# define SC_SYSCALL_TABLE_ENTRY_0004    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0005
# define SC_SYSCALL_TABLE_ENTRY_0005    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0006
# define SC_SYSCALL_TABLE_ENTRY_0006    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0007
# define SC_SYSCALL_TABLE_ENTRY_0007    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0008
# define SC_SYSCALL_TABLE_ENTRY_0008    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0009
# define SC_SYSCALL_TABLE_ENTRY_0009    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0010
# define SC_SYSCALL_TABLE_ENTRY_0010    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0011
# define SC_SYSCALL_TABLE_ENTRY_0011    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0012
# define SC_SYSCALL_TABLE_ENTRY_0012    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0013
# define SC_SYSCALL_TABLE_ENTRY_0013    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0014
# define SC_SYSCALL_TABLE_ENTRY_0014    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0015
# define SC_SYSCALL_TABLE_ENTRY_0015    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0016
# define SC_SYSCALL_TABLE_ENTRY_0016    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0017
# define SC_SYSCALL_TABLE_ENTRY_0017    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0018
# define SC_SYSCALL_TABLE_ENTRY_0018    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0019
# define SC_SYSCALL_TABLE_ENTRY_0019    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0020
# define SC_SYSCALL_TABLE_ENTRY_0020    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0021
# define SC_SYSCALL_TABLE_ENTRY_0021    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0022
# define SC_SYSCALL_TABLE_ENTRY_0022    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0023
# define SC_SYSCALL_TABLE_ENTRY_0023    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0024
# define SC_SYSCALL_TABLE_ENTRY_0024    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0025
# define SC_SYSCALL_TABLE_ENTRY_0025    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0026
# define SC_SYSCALL_TABLE_ENTRY_0026    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0027
# define SC_SYSCALL_TABLE_ENTRY_0027    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0028
# define SC_SYSCALL_TABLE_ENTRY_0028    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0029
# define SC_SYSCALL_TABLE_ENTRY_0029    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0030
# define SC_SYSCALL_TABLE_ENTRY_0030    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0031
# define SC_SYSCALL_TABLE_ENTRY_0031    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0032
# define SC_SYSCALL_TABLE_ENTRY_0032    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0033
# define SC_SYSCALL_TABLE_ENTRY_0033    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0034
# define SC_SYSCALL_TABLE_ENTRY_0034    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0035
# define SC_SYSCALL_TABLE_ENTRY_0035    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0036
# define SC_SYSCALL_TABLE_ENTRY_0036    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0037
# define SC_SYSCALL_TABLE_ENTRY_0037    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0038
# define SC_SYSCALL_TABLE_ENTRY_0038    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0039
# define SC_SYSCALL_TABLE_ENTRY_0039    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0040
# define SC_SYSCALL_TABLE_ENTRY_0040    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0041
# define SC_SYSCALL_TABLE_ENTRY_0041    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0042
# define SC_SYSCALL_TABLE_ENTRY_0042    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0043
# define SC_SYSCALL_TABLE_ENTRY_0043    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0044
# define SC_SYSCALL_TABLE_ENTRY_0044    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0045
# define SC_SYSCALL_TABLE_ENTRY_0045    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0046
# define SC_SYSCALL_TABLE_ENTRY_0046    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0047
# define SC_SYSCALL_TABLE_ENTRY_0047    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0048
# define SC_SYSCALL_TABLE_ENTRY_0048    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0049
# define SC_SYSCALL_TABLE_ENTRY_0049    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0050
# define SC_SYSCALL_TABLE_ENTRY_0050    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0051
# define SC_SYSCALL_TABLE_ENTRY_0051    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0052
# define SC_SYSCALL_TABLE_ENTRY_0052    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0053
# define SC_SYSCALL_TABLE_ENTRY_0053    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0054
# define SC_SYSCALL_TABLE_ENTRY_0054    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0055
# define SC_SYSCALL_TABLE_ENTRY_0055    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0056
# define SC_SYSCALL_TABLE_ENTRY_0056    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0057
# define SC_SYSCALL_TABLE_ENTRY_0057    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0058
# define SC_SYSCALL_TABLE_ENTRY_0058    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0059
# define SC_SYSCALL_TABLE_ENTRY_0059    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0060
# define SC_SYSCALL_TABLE_ENTRY_0060    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0061
# define SC_SYSCALL_TABLE_ENTRY_0061    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0062
# define SC_SYSCALL_TABLE_ENTRY_0062    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef SC_SYSCALL_TABLE_ENTRY_0063
# define SC_SYSCALL_TABLE_ENTRY_0063    SC_SYSCALL_DUMMY_TABLE_ENTRY
#endif

/* Here we have a very weak further test for inconsitencies. Will not detect if one
   specifies further system calls with non consecutive indexes. */
#ifdef SC_SYSCALL_TABLE_ENTRY_0064
# error More system calls defined than declared table size. See #SC_NO_SYSTEM_CALLS
#endif


/*
 * Local type definitions
 */


/*
 * Local prototypes
 */

/** The assembler implementation of the no operation dummy system call.\n
      Note, despite of the C style prototype, this is not a C callable function. The calling
    convention is different to C. This is the reason, why we declare it here instead of
    publishing it globally in ivr_ivorHandler.h. */
extern void ivr_scBscHdlr_sysCallUndefined(void);


/*
 * Data definitions
 */

/** The global, constant table of system call descriptors. */
const sc_systemCallDesc_t sc_systemCallDescAry[SC_NO_SYSTEM_CALLS]
    SECTION(.text.ivor.sc_systemCallDescAry) =
{
    [0] = SC_SYSCALL_TABLE_ENTRY_0000
    , [1] = SC_SYSCALL_TABLE_ENTRY_0001
    , [2] = SC_SYSCALL_TABLE_ENTRY_0002
    , [3] = SC_SYSCALL_TABLE_ENTRY_0003
    , [4] = SC_SYSCALL_TABLE_ENTRY_0004
    , [5] = SC_SYSCALL_TABLE_ENTRY_0005
    , [6] = SC_SYSCALL_TABLE_ENTRY_0006
    , [7] = SC_SYSCALL_TABLE_ENTRY_0007
    , [8] = SC_SYSCALL_TABLE_ENTRY_0008
    , [9] = SC_SYSCALL_TABLE_ENTRY_0009
    , [10] = SC_SYSCALL_TABLE_ENTRY_0010
    , [11] = SC_SYSCALL_TABLE_ENTRY_0011
    , [12] = SC_SYSCALL_TABLE_ENTRY_0012
    , [13] = SC_SYSCALL_TABLE_ENTRY_0013
    , [14] = SC_SYSCALL_TABLE_ENTRY_0014
    , [15] = SC_SYSCALL_TABLE_ENTRY_0015
    , [16] = SC_SYSCALL_TABLE_ENTRY_0016
    , [17] = SC_SYSCALL_TABLE_ENTRY_0017
    , [18] = SC_SYSCALL_TABLE_ENTRY_0018
    , [19] = SC_SYSCALL_TABLE_ENTRY_0019
    , [20] = SC_SYSCALL_TABLE_ENTRY_0020
    , [21] = SC_SYSCALL_TABLE_ENTRY_0021
    , [22] = SC_SYSCALL_TABLE_ENTRY_0022
    , [23] = SC_SYSCALL_TABLE_ENTRY_0023
    , [24] = SC_SYSCALL_TABLE_ENTRY_0024
    , [25] = SC_SYSCALL_TABLE_ENTRY_0025
    , [26] = SC_SYSCALL_TABLE_ENTRY_0026
    , [27] = SC_SYSCALL_TABLE_ENTRY_0027
    , [28] = SC_SYSCALL_TABLE_ENTRY_0028
    , [29] = SC_SYSCALL_TABLE_ENTRY_0029
    , [30] = SC_SYSCALL_TABLE_ENTRY_0030
    , [31] = SC_SYSCALL_TABLE_ENTRY_0031
    , [32] = SC_SYSCALL_TABLE_ENTRY_0032
    , [33] = SC_SYSCALL_TABLE_ENTRY_0033
    , [34] = SC_SYSCALL_TABLE_ENTRY_0034
    , [35] = SC_SYSCALL_TABLE_ENTRY_0035
    , [36] = SC_SYSCALL_TABLE_ENTRY_0036
    , [37] = SC_SYSCALL_TABLE_ENTRY_0037
    , [38] = SC_SYSCALL_TABLE_ENTRY_0038
    , [39] = SC_SYSCALL_TABLE_ENTRY_0039
    , [40] = SC_SYSCALL_TABLE_ENTRY_0040
    , [41] = SC_SYSCALL_TABLE_ENTRY_0041
    , [42] = SC_SYSCALL_TABLE_ENTRY_0042
    , [43] = SC_SYSCALL_TABLE_ENTRY_0043
    , [44] = SC_SYSCALL_TABLE_ENTRY_0044
    , [45] = SC_SYSCALL_TABLE_ENTRY_0045
    , [46] = SC_SYSCALL_TABLE_ENTRY_0046
    , [47] = SC_SYSCALL_TABLE_ENTRY_0047
    , [48] = SC_SYSCALL_TABLE_ENTRY_0048
    , [49] = SC_SYSCALL_TABLE_ENTRY_0049
    , [50] = SC_SYSCALL_TABLE_ENTRY_0050
    , [51] = SC_SYSCALL_TABLE_ENTRY_0051
    , [52] = SC_SYSCALL_TABLE_ENTRY_0052
    , [53] = SC_SYSCALL_TABLE_ENTRY_0053
    , [54] = SC_SYSCALL_TABLE_ENTRY_0054
    , [55] = SC_SYSCALL_TABLE_ENTRY_0055
    , [56] = SC_SYSCALL_TABLE_ENTRY_0056
    , [57] = SC_SYSCALL_TABLE_ENTRY_0057
    , [58] = SC_SYSCALL_TABLE_ENTRY_0058
    , [59] = SC_SYSCALL_TABLE_ENTRY_0059
    , [60] = SC_SYSCALL_TABLE_ENTRY_0060
    , [61] = SC_SYSCALL_TABLE_ENTRY_0061
    , [62] = SC_SYSCALL_TABLE_ENTRY_0062
    , [63] = SC_SYSCALL_TABLE_ENTRY_0063
};


/*
 * Function implementation
 */

