/**
 * @file cps_cppSupport.cpp
 * Some basic functions, which are called from the machine code emitted by gcc for C++
 * sources. The function permit to safely initialze static data objects.\n
 *   This file is an alternative to using -fno-threadsafe-statics in this simple
 * single-threaded environment: We provide an implementation for the otherwise missing
 * synchronization functions. The implementation depends on the environment; here we assume
 * the situation from the simple samples, which don't use an RTOS. Concurrent contexts are
 * the single-threaded main context and the interrupt handlers.\n
 *   See e.g. www.opensource.apple.com/source/libcppabi/libcppabi-14/src/cxa_guard.cxx
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
 * Local functions
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

extern "C"
{
#include "typ_types.h"
#include "ihw_initMcuCoreHW.h"
}


/*
 * Defines
 */
 
 
/*
 * Local type definitions
 */
 
/** The C++ mechanism uses a 64 Bit guard object to implement the operations. We model the
    provided 8 Byte memory area as a struct in order to get a well readable and
    maintainable implementation of the concept. */
typedef struct guard_noRTOS_t
{
    /** Initialization state of the guarded object: 0 not initialized, 1 initialization
        completed, 2: is currently being initialized. */
    volatile uint8_t state;

    /** A part of the guard memory is applied to implement the critical section we need. */
    uint32_t msr;

} guard_noRTOS_t;


 
/*
 * Local prototypes
 */
 
extern "C" int __cxa_guard_acquire(int64_t *guardObj);
extern "C" void __cxa_guard_release(int64_t *guardObj ATTRIB_UNUSED);
extern "C" void __cxa_guard_abort(uint64_t *pGuardObj);
extern "C" void halt();


/*
 * Data definitions
 */
 
 
/*
 * Function implementation
 */

/**
 * Decide for a given object (identified by its associated guard object) whether it is
 * already initialized or not.
 *   @return 
 * The function returns 0 for initialized ojects and 1 if the initialization should still
 * be done by the calling code and before use of the object.
 *   @pGuardObj
 * The associated guard object by reference. The object is an 8 Byte memory area, which is
 * guaranteed to be all zeros of system satrtup and when the guarded object is not yet
 * initialized.
 */
extern "C" int __cxa_guard_acquire(int64_t *pGuardObj)
{
    static_assert( sizeof(guard_noRTOS_t) == sizeof(int64_t)
                    &&  offsetof(guard_noRTOS_t, state) == 0
                    &&  offsetof(guard_noRTOS_t, msr) == 4
                  , "Bad definition of contents of C++ guard object"
                  );

    /* Normal function result 0: Object is initialized. */
    int result = 0;

    guard_noRTOS_t *pG = (guard_noRTOS_t*)pGuardObj;
    if(pG->state == 0)
    {
        /* The object is not yet initialized but there may be a concurrent context trying
           the same - we need to inhibit further context switches. */
        uint32_t msr = ihw_enterCriticalSection();

        /* We need to check the flag again - the potential competitor may have come first. */
        if(pG->state == 0)
        {
            /* We own the guard, we initialize the object. Store the information to later
               leave the critical section. */
            assert(pG->msr == 0);
            pG->msr = msr;

            /* The critical section avoids that an other context interferes. A second flag
               will ensure that the current context won't enter the initialization by
               recursion. There are no race conditions involved in accessing this flag.
                 Note, there's no healing from this situation. It depends on the platform,
               application and integration scenario, what the abort operation will actually
               mean. */
            if(pG->state == 2)
                halt();
            else
                pG->state = 2;

            /* Result 1 indicates to the calling code that it should go ahead with the
               object initialization. */
            result = 1;
        }
        else
        {
            /* The object has meanwhile been initialized by a competitor. Leave critical
               section and done. */
            ihw_leaveCriticalSection(msr);
        }
    }
    else
    {
        /* If we get here then the object is surely initialized. We don't require an
           expensive critical section in this case - which is fortunately the normal
           runtime case. */

    } /* End if(Cheap check for likely not yet initialized) */

    return result;

} /* End of __cxa_guard_acquire */


/**
 * This function is called when the initialization of the related data object has completed
 * after a recent call of __cxa_guard_acquire(), which had returned 1.
 *   @pGuardObj
 * The guard object associated with the data object by reference.
 */
extern "C" void __cxa_guard_release(int64_t *pGuardObj)
{
    /* This function can only be entered by the context, which had entered the critical
       section. There are no race conditions. */
    guard_noRTOS_t *pG = (guard_noRTOS_t*)pGuardObj;

    /* Final state: Object is initialized. */
    assert(pG->state == 2);
    pG->state = 1;

    /* Leave the critical section. We don't need to reset the field to NULL; there will no
       entry into the initializtaion code again and the assertion above won't fire. */
    ihw_leaveCriticalSection(pG->msr);

} /* End of __cxa_guard_release */



/**
 * This function is called when the initialization of the related data object has failed.
 * The initialization had been initiated by a recent call of __cxa_guard_acquire(), which
 * had returned 1.
 *   @pGuardObj
 * The guard object associated with the data object by reference.
 */
extern "C" void __cxa_guard_abort(uint64_t *pGuardObj)
{
    /* This function can only be entered by the context, which had entered the critical
       section. There are no race conditions. */
    guard_noRTOS_t *pG = (guard_noRTOS_t*)pGuardObj;

    /* Final state: Object is (still or again) uninitialized. */
    assert(pG->state == 2);
    pG->state = 0;

    /* Leave the critical section. We need to reset the field to NULL; there may easily be
       the next entry into the initialization code and the assertion above would fire. */
    ihw_leaveCriticalSection(pG->msr);
    pG->msr = 0;

    /* Everything should now be reset to the system startup state. */
    assert(*pGuardObj == 0ull);

} /* End of __cxa_guard_abort */



/**
 * The initialization process of a data object can fail if the C++ source code implements a
 * (forbidden) recursion, which requires the initialization of an object as element of the
 * initialization of that object. There's no recovery and we halt the software execution.\n
 *   The operation is similar to an "assert(false)"; the difference is that we try halting
 * the SW execution even in PRODUCTION compilation: The problem may easily point to a
 * non-static, data dependent runtime error and must always be handled.
 */
extern "C" void halt()
{
    assert(false);
    ihw_suspendAllInterrupts();
    while(true)
        ;
}


/**
 * We map the C++ new operator on the malloc function from the C lib.\n
 *   Note, in our environment, we can't throw an exception if no memory is available.
 */
void *operator new(unsigned int noBytes)
{
    void *p = malloc(noBytes);
    
    /* We can't throw an exception if no memory is available. We need to halt execution;
       the calling code won't do a NULL pointer check. */
    if(p == NULL)
        halt();
        
    return p;
}

/**
 * We map the C++ delete operator on the free function from the C lib.
 */
void operator delete(void *p)
{
    free(p);
}
