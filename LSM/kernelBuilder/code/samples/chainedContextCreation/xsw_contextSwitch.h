#ifndef XSW_CONTEXTSWITCH_INCLUDED
#define XSW_CONTEXTSWITCH_INCLUDED
/**
 * @file xsw_contextSwitch.h
 * Definition of global interface of module xsw_contextSwitch.c
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

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>

#include "int_interruptHandler.h"


/*
 * Defines
 */

/* The software is written as portable as reasonably possible. This requires the awareness
   of the C language standard it is compiled with. */
#if defined(__STDC_VERSION__)
# if (__STDC_VERSION__)/100 == 2011
#  define _STDC_VERSION_C11
# elif (__STDC_VERSION__)/100 == 1999
#  define _STDC_VERSION_C99
# endif
#endif


/*
 * Global type definitions
 */

/** The description of a new execution context. Used as argument to the context creation
    with xsw_sc_createContext().
      @remark The full functionality of this data type and the related system call
    xsw_sc_createContext() is not exploited in this sample. The definition of the system
    call has been taken from an earlier revision of kernelBuilder, where it still
    belonged to the framework. It became obsolete in the framework when the on-the-fly
    created contexts were introduced. This sample demonstrates how the elder system call
    funtionality can be modelled with the new framework design. */
typedef struct xsw_contextDesc_t
{
    /** A C functions, which is the entry point into the new execution context. */
    uint32_t (*executionEntryPoint)(uint32_t);
    
    /** The initial value of the stack pointer. The client code will allocate sufficient
        stack memory. This pointer will usually point at the first address beyond the
        allocated memory chunk; our stacks grow downward to lower addresses.\n
          Note, each pre-emption of a context by an asynchronous External Interrupt
        requires about 170 Bytes of stack space. If your application makes use of all
        interrupt priorities then you need to have 15*170 Byte as a minimum of stack space
        for safe operation, not yet counted the stack consumption of your application
        itself.\n
          Note, this lower bounds even holds if you apply the implementation of the
        priority ceiling protocol from the startup code to mutually exclude sets of
        interrupts from pre-empting one another, see
        https://community.nxp.com/message/993795 for details.\n
          The passed address needs to be 8 Byte aligned; this is double-checked by
        assertion. */
    void *stackPointer;
    
    /** The newly created context can be run either in user mode or in privileged mode.\n
          Node, the user mode should be preferred but can generally be used only if the
        whole system design supports this. All system level functions (in particular the
        I/O drivers) need to have an API, which is based on system calls. Even the most
        simple functions ihw_suspendAllInterrupts() and ihw_resumeAllInterrupts() are not
        permitted in user mode. */
    bool privilegedMode;
    
} xsw_contextDesc_t;



/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

/** Implementation of system call to create a new execution context. */
uint32_t xsw_sc_createContext( int_cmdContextSwitch_t *pCmdContextSwitch
                             , const xsw_contextDesc_t *pNewContextDesc
                             , bool runImmediately
                             , uint32_t initialData
                             , int_contextSaveDesc_t *pNewContextSaveDesc
                             , int_contextSaveDesc_t *pThisContextSaveDesc
                             );

/** System call implementation for switching to another context. */
uint32_t xsw_sc_switchContext( int_cmdContextSwitch_t *pCmdContextSwitch
                             , unsigned int idxOfResumedContext
                             , uint32_t signalToResumedContext
                             );

/** Main function of sample scheduler: Create and start contexts and never return. */
void _Noreturn xsw_startContextSwitching(void);

#endif  /* XSW_CONTEXTSWITCH_INCLUDED */
