#ifndef PCP_SYSCALLPCP_INCLUDED
#define PCP_SYSCALLPCP_INCLUDED
/**
 * @file pcp_sysCallPCP.h
 * Definition of global interface of module pcp_sysCallPCP.S
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

#ifdef __STDC_VERSION__
# include <stdint.h>
# include <stdbool.h>
#endif


/*
 * Defines
 */

/** Interrupt priority of the scheduler. Needed here for assembler implementation of
    priority ceiling protocol.
      @remark The C code contains preprocessor code that ensures consistency with the
    according definition in the C code. */
#define PCP_KERNEL_PRIORITY        12

/** Index of implemented system call for setting a user context's current priority. */
#define PCP_SYSCALL_SUSPEND_ALL_INTERRUPTS_BY_PRIORITY  1


#ifdef __STDC_VERSION__
/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */



#endif  /* For C code compilation only */
#endif  /* PCP_SYSCALLPCP_INCLUDED */
