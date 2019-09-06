/**
 * @file rtos_systemCall.c
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
 * Local functions
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include "typ_types.h"
#include "rtos.h"
#include "rtos_systemCall.h"

/*
 * Include section for I/O driver header files.
 *   The header files, which are included here declare the system calls, which are
 * implemented by the I/O drivers. Using preprocessor constructs we check for multiple
 * definitions of the same system call and we can add missing ones.
 */
#include "assert_defSysCalls.h"
#include "rtos_ivorHandler_defSysCalls.h"
#include "rtos_priorityCeilingProtocol_defSysCalls.h"
#include "rtos_process_defSysCalls.h"
#include "rtos_scheduler_defSysCalls.h"
#include "lbd_ledAndButtonDriver_defSysCalls.h"
#include "sio_serialIO_defSysCalls.h"


/*
 * Defines
 */

/** This table entry is used for those system table entries, which are not defined by any
    included I/O driver header file. The dummy table entry points to a no-operation
    service, which silently returns to the caller. */
#define RTOS_SYSCALL_DUMMY_TABLE_ENTRY  \
                        RTOS_SC_TABLE_ENTRY(rtos_scBscHdlr_sysCallUndefined, BASIC)

/* The preprocessor code down here filters all system call table entries, which are not
   defined by any of the included I/O driver header files and makes them become the dummy
   entry #RTOS_SYSCALL_DUMMY_TABLE_ENTRY. */
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0000
# error System call 0 has not been defined. This system call is required to terminate \
        a user task and is mandatory
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0001
# define RTOS_SYSCALL_TABLE_ENTRY_0001    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0002
# define RTOS_SYSCALL_TABLE_ENTRY_0002    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0003
# define RTOS_SYSCALL_TABLE_ENTRY_0003    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0004
# define RTOS_SYSCALL_TABLE_ENTRY_0004    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0005
# define RTOS_SYSCALL_TABLE_ENTRY_0005    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0006
# define RTOS_SYSCALL_TABLE_ENTRY_0006    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0007
# define RTOS_SYSCALL_TABLE_ENTRY_0007    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0008
# define RTOS_SYSCALL_TABLE_ENTRY_0008    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0009
# define RTOS_SYSCALL_TABLE_ENTRY_0009    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0010
# define RTOS_SYSCALL_TABLE_ENTRY_0010    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0011
# define RTOS_SYSCALL_TABLE_ENTRY_0011    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0012
# define RTOS_SYSCALL_TABLE_ENTRY_0012    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0013
# define RTOS_SYSCALL_TABLE_ENTRY_0013    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0014
# define RTOS_SYSCALL_TABLE_ENTRY_0014    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0015
# define RTOS_SYSCALL_TABLE_ENTRY_0015    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0016
# define RTOS_SYSCALL_TABLE_ENTRY_0016    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0017
# define RTOS_SYSCALL_TABLE_ENTRY_0017    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0018
# define RTOS_SYSCALL_TABLE_ENTRY_0018    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0019
# define RTOS_SYSCALL_TABLE_ENTRY_0019    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0020
# define RTOS_SYSCALL_TABLE_ENTRY_0020    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0021
# define RTOS_SYSCALL_TABLE_ENTRY_0021    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0022
# define RTOS_SYSCALL_TABLE_ENTRY_0022    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0023
# define RTOS_SYSCALL_TABLE_ENTRY_0023    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0024
# define RTOS_SYSCALL_TABLE_ENTRY_0024    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0025
# define RTOS_SYSCALL_TABLE_ENTRY_0025    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0026
# define RTOS_SYSCALL_TABLE_ENTRY_0026    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0027
# define RTOS_SYSCALL_TABLE_ENTRY_0027    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0028
# define RTOS_SYSCALL_TABLE_ENTRY_0028    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0029
# define RTOS_SYSCALL_TABLE_ENTRY_0029    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0030
# define RTOS_SYSCALL_TABLE_ENTRY_0030    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0031
# define RTOS_SYSCALL_TABLE_ENTRY_0031    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0032
# define RTOS_SYSCALL_TABLE_ENTRY_0032    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0033
# define RTOS_SYSCALL_TABLE_ENTRY_0033    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0034
# define RTOS_SYSCALL_TABLE_ENTRY_0034    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0035
# define RTOS_SYSCALL_TABLE_ENTRY_0035    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0036
# define RTOS_SYSCALL_TABLE_ENTRY_0036    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0037
# define RTOS_SYSCALL_TABLE_ENTRY_0037    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0038
# define RTOS_SYSCALL_TABLE_ENTRY_0038    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0039
# define RTOS_SYSCALL_TABLE_ENTRY_0039    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0040
# define RTOS_SYSCALL_TABLE_ENTRY_0040    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0041
# define RTOS_SYSCALL_TABLE_ENTRY_0041    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0042
# define RTOS_SYSCALL_TABLE_ENTRY_0042    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0043
# define RTOS_SYSCALL_TABLE_ENTRY_0043    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0044
# define RTOS_SYSCALL_TABLE_ENTRY_0044    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0045
# define RTOS_SYSCALL_TABLE_ENTRY_0045    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0046
# define RTOS_SYSCALL_TABLE_ENTRY_0046    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0047
# define RTOS_SYSCALL_TABLE_ENTRY_0047    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0048
# define RTOS_SYSCALL_TABLE_ENTRY_0048    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0049
# define RTOS_SYSCALL_TABLE_ENTRY_0049    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0050
# define RTOS_SYSCALL_TABLE_ENTRY_0050    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0051
# define RTOS_SYSCALL_TABLE_ENTRY_0051    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0052
# define RTOS_SYSCALL_TABLE_ENTRY_0052    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0053
# define RTOS_SYSCALL_TABLE_ENTRY_0053    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0054
# define RTOS_SYSCALL_TABLE_ENTRY_0054    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0055
# define RTOS_SYSCALL_TABLE_ENTRY_0055    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0056
# define RTOS_SYSCALL_TABLE_ENTRY_0056    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0057
# define RTOS_SYSCALL_TABLE_ENTRY_0057    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0058
# define RTOS_SYSCALL_TABLE_ENTRY_0058    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0059
# define RTOS_SYSCALL_TABLE_ENTRY_0059    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0060
# define RTOS_SYSCALL_TABLE_ENTRY_0060    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0061
# define RTOS_SYSCALL_TABLE_ENTRY_0061    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0062
# define RTOS_SYSCALL_TABLE_ENTRY_0062    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif
#ifndef RTOS_SYSCALL_TABLE_ENTRY_0063
# define RTOS_SYSCALL_TABLE_ENTRY_0063    RTOS_SYSCALL_DUMMY_TABLE_ENTRY
#endif

/* Here we have a very weak further test for inconsistencies. It aims at finding too many
   specified system calls. It will however not detect if one specifies further system calls
   with non consecutive indexes. */
#ifdef RTOS_SYSCALL_TABLE_ENTRY_0064
# error More system calls defined than declared table size. See #RTOS_NO_SYSTEM_CALLS
#endif


/** Companion of C's offsetof: The size of a field inside a struct. */
#define sizeoffield(type, fieldName) (sizeof(((type*)0)->fieldName))

/** The C code has an interface with the assembler code that implements the system call
    service. The interface is modeled twice, once as structs for C code and once as set of
    preprocessor macros, which hold size of data structures and offsets of fields. Here, we
    have a macro, which double-checks the equivalence of both definitions. The compiler
    will abort the compilation if there is an inconsistency.\n
      The macro needs to by compiled at least once, anywhere in the code. All checks are
    done at compile-time and the compiler won't emit any machine code; therefore, it
    doesn't matter where to place the macro. */
#define CHECK_INTERFACE_S2C                                                                 \
    _Static_assert( sizeof(systemCallDesc_t) == SIZE_OF_SC_DESC                             \
                    &&  offsetof(systemCallDesc_t, addressOfFct) == O_SCDESC_sr             \
                    &&  sizeoffield(systemCallDesc_t, addressOfFct) == 4                    \
                    &&  offsetof(systemCallDesc_t, conformanceClass) == O_SCDESC_confCls    \
                    &&  sizeoffield(systemCallDesc_t, conformanceClass) == 4                \
                  , "struct systemCallDesc_t: Inconsistent interface between"               \
                    " assembler and C code"                                                 \
                  )


/*
 * Local type definitions
 */

/** An entry in the table of system call service descriptors. */
typedef struct systemCallDesc_t
{
    /** The pointer to the service implementation.\n
          This field is address at offset O_SCDESC_sr from the assembler code. */
    uint32_t addressOfFct;

    /** Conformance class of service handler. The values are according to enum
        #RTOS_HDLR_CONF_CLASS_BASIC and following. */
    uint32_t conformanceClass;

} systemCallDesc_t;


/*
 * Local prototypes
 */

/** The assembler implementation of the no operation dummy system call.\n
      Note, despite of the C style prototype, this is not a C callable function. The calling
    convention is different to C. This is the reason, why we declare it here instead of
    publishing it globally in rtos_ivorHandler.h. */
extern void rtos_scBscHdlr_sysCallUndefined(void);

/** This is a dummy function only. */
void rtos_checkInterfaceAssemblerToC(void);


/*
 * Data definitions
 */

/** The global, constant table of system call descriptors. */
const systemCallDesc_t rtos_systemCallDescAry[RTOS_NO_SYSTEM_CALLS]
    SECTION(.text.ivor.rtos_systemCallDescAry) =
{
    [0] = RTOS_SYSCALL_TABLE_ENTRY_0000
    , [1] = RTOS_SYSCALL_TABLE_ENTRY_0001
    , [2] = RTOS_SYSCALL_TABLE_ENTRY_0002
    , [3] = RTOS_SYSCALL_TABLE_ENTRY_0003
    , [4] = RTOS_SYSCALL_TABLE_ENTRY_0004
    , [5] = RTOS_SYSCALL_TABLE_ENTRY_0005
    , [6] = RTOS_SYSCALL_TABLE_ENTRY_0006
    , [7] = RTOS_SYSCALL_TABLE_ENTRY_0007
    , [8] = RTOS_SYSCALL_TABLE_ENTRY_0008
    , [9] = RTOS_SYSCALL_TABLE_ENTRY_0009
    , [10] = RTOS_SYSCALL_TABLE_ENTRY_0010
    , [11] = RTOS_SYSCALL_TABLE_ENTRY_0011
    , [12] = RTOS_SYSCALL_TABLE_ENTRY_0012
    , [13] = RTOS_SYSCALL_TABLE_ENTRY_0013
    , [14] = RTOS_SYSCALL_TABLE_ENTRY_0014
    , [15] = RTOS_SYSCALL_TABLE_ENTRY_0015
    , [16] = RTOS_SYSCALL_TABLE_ENTRY_0016
    , [17] = RTOS_SYSCALL_TABLE_ENTRY_0017
    , [18] = RTOS_SYSCALL_TABLE_ENTRY_0018
    , [19] = RTOS_SYSCALL_TABLE_ENTRY_0019
    , [20] = RTOS_SYSCALL_TABLE_ENTRY_0020
    , [21] = RTOS_SYSCALL_TABLE_ENTRY_0021
    , [22] = RTOS_SYSCALL_TABLE_ENTRY_0022
    , [23] = RTOS_SYSCALL_TABLE_ENTRY_0023
    , [24] = RTOS_SYSCALL_TABLE_ENTRY_0024
    , [25] = RTOS_SYSCALL_TABLE_ENTRY_0025
    , [26] = RTOS_SYSCALL_TABLE_ENTRY_0026
    , [27] = RTOS_SYSCALL_TABLE_ENTRY_0027
    , [28] = RTOS_SYSCALL_TABLE_ENTRY_0028
    , [29] = RTOS_SYSCALL_TABLE_ENTRY_0029
    , [30] = RTOS_SYSCALL_TABLE_ENTRY_0030
    , [31] = RTOS_SYSCALL_TABLE_ENTRY_0031
    , [32] = RTOS_SYSCALL_TABLE_ENTRY_0032
    , [33] = RTOS_SYSCALL_TABLE_ENTRY_0033
    , [34] = RTOS_SYSCALL_TABLE_ENTRY_0034
    , [35] = RTOS_SYSCALL_TABLE_ENTRY_0035
    , [36] = RTOS_SYSCALL_TABLE_ENTRY_0036
    , [37] = RTOS_SYSCALL_TABLE_ENTRY_0037
    , [38] = RTOS_SYSCALL_TABLE_ENTRY_0038
    , [39] = RTOS_SYSCALL_TABLE_ENTRY_0039
    , [40] = RTOS_SYSCALL_TABLE_ENTRY_0040
    , [41] = RTOS_SYSCALL_TABLE_ENTRY_0041
    , [42] = RTOS_SYSCALL_TABLE_ENTRY_0042
    , [43] = RTOS_SYSCALL_TABLE_ENTRY_0043
    , [44] = RTOS_SYSCALL_TABLE_ENTRY_0044
    , [45] = RTOS_SYSCALL_TABLE_ENTRY_0045
    , [46] = RTOS_SYSCALL_TABLE_ENTRY_0046
    , [47] = RTOS_SYSCALL_TABLE_ENTRY_0047
    , [48] = RTOS_SYSCALL_TABLE_ENTRY_0048
    , [49] = RTOS_SYSCALL_TABLE_ENTRY_0049
    , [50] = RTOS_SYSCALL_TABLE_ENTRY_0050
    , [51] = RTOS_SYSCALL_TABLE_ENTRY_0051
    , [52] = RTOS_SYSCALL_TABLE_ENTRY_0052
    , [53] = RTOS_SYSCALL_TABLE_ENTRY_0053
    , [54] = RTOS_SYSCALL_TABLE_ENTRY_0054
    , [55] = RTOS_SYSCALL_TABLE_ENTRY_0055
    , [56] = RTOS_SYSCALL_TABLE_ENTRY_0056
    , [57] = RTOS_SYSCALL_TABLE_ENTRY_0057
    , [58] = RTOS_SYSCALL_TABLE_ENTRY_0058
    , [59] = RTOS_SYSCALL_TABLE_ENTRY_0059
    , [60] = RTOS_SYSCALL_TABLE_ENTRY_0060
    , [61] = RTOS_SYSCALL_TABLE_ENTRY_0061
    , [62] = RTOS_SYSCALL_TABLE_ENTRY_0062
    , [63] = RTOS_SYSCALL_TABLE_ENTRY_0063
};


/*
 * Function implementation
 */

/**
 * This is a dummy function, which can be removed as soon as a true function is implemented
 * in this module. We need to have at least one function in order to place the assertion
 * statement for double-checking the binary built-up of the interface for system calls
 * between assembly code and C code -  a check, which is neither done implicitly by C
 * compiler nor by assembler.\n
 *   The linker shall recognize that the function is nowhere called and discard it from the
 * flashed binary files.
 *   @remark
 * Do never call this function. It's just needed for the compilation process.
 */
void rtos_checkInterfaceAssemblerToC(void)
{
    CHECK_INTERFACE_S2C;

} /* End of rtos_checkInterfaceAssemblerToC */