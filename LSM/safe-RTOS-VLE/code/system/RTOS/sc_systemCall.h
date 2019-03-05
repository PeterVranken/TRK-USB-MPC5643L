#ifndef SC_SYSTEMCALL_INCLUDED
#define SC_SYSTEMCALL_INCLUDED
/**
 * @file sc_systemCall.h
 * Definition of global interface of module sc_systemCall.c\n
 *   Note, this header file shares information between assembler and C code.
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

/** The number of supported system calls.\n
      If this define is increased than the initializer expression of \a
    sc_systemCallDescAry and the list of conditional defines #SC_SYSCALL_TABLE_ENTRY_0000,
    found if file sc_systemCall.c, need to be extended, too. */
#define SC_NO_SYSTEM_CALLS  64

/** Definition of the enumeration of the supported conformance classes for system call
    handlers. We have:\n
      Basic conformance class: The handler is a raw assembler implementation. The system
    call exception branches to the assembler code and it has full responsibility for stack
    switching, memory protection, return from interrupt, etc. This class is not usable with
    C code.\n
      Simple handler class: Such a handler can be implemented as C function. The system call
    arguments are accessible as arguments 2, 3, ... of this C function. (The first argument
    of the C function is reserved and must not be used.) The function is executed with all
    interrupt processing suspended and therefore it needs to be short. This handler type
    must be used for fast, immediate actions only, like a set or get function.\n
      Full handler class:  Such a handler can be implemented as C function. The system call
    arguments are accessible as arguments 2, 3, ... of this C function. (The first argument
    of the C function is reserved and must not be used.) The function is executed under
    normal conditions, it is for example preemptable by tasks and interruptsof higher
    priority. This is the normal class of a system call handler.\n
      Note, we need to use a define for this enumeration as the definition es shared with
    the implementing assembler code.\n
      Here, we have the enumeration value to declare a basic handler. */
#define SC_HDLR_CONF_CLASS_BASIC    0
#define SC_HDLR_CONF_CLASS_SIMPLE   1   /** Declaration of a simple handler. See
                                            #SC_CONF_CLASS_BASIC_HDLR for details. */
#define SC_HDLR_CONF_CLASS_FULL     2   /** Declaration of a full handler. See
                                            #SC_CONF_CLASS_BASIC_HDLR for details. */

#ifdef __STDC_VERSION__
/*
 * Global type definitions
 */


/** An entry in the table of system call service descriptors. */
typedef struct sc_systemCallDesc_t
{
    /** The pointer to the service implementation.\n
          This field is address at offset O_SCDESC_sr from the assembler code. */
    uint32_t addressOfFct;

    /** Conformance class of service handler. The values are according to enum
        #SC_HDLR_CONF_CLASS_BASIC and following. */
    uint32_t conformanceClass;

} sc_systemCallDesc_t;


/*
 * Global data declarations
 */

extern const sc_systemCallDesc_t sc_systemCallDescAry[SC_NO_SYSTEM_CALLS];


/*
 * Global static inline functions
 */

/**
 * Helper function for system call handler implementation: A system call handler must never
 * trust a user code provided pointer; Evidently not for write access but not even for read
 * operation (a read into the address space of peripherals can have a side effect). The
 * user code could make the system call handler overwrite some non-process owned data
 * objects, cause an access violation in the supervisor code or manipulate some
 * peripherals by sideeffect of a read-register operation.\n
 *   Normally, it's strongly disencouraged having pointers as arguments of system calls at
 * all. If not avoidable, one can use this helper function to check that a pointer points
 * into permitted address space and that all bytes of a data object pointed at are still in
 * that address space. Here for read access.\n
 *   Permitted address space is anywhere, where supervisor code may read without causing an
 * exception or any kind of sideeffect. In particular, these are the used portions of RAM
 * and ROM.
 *   @return
 * Get \a true if the pointer may be used for read access and \a false otherwise.
 *   @param address
 * The pointer value, or the beginning of the chunk of memory, which needs to be entirely
 * located in readable memory.
 *   @param noBytes
 * The size of the chunk of memory to be checked. Must not be less than one. (Checked by
 * assertion).
 */
static inline bool sc_checkUserCodeReadPtr(const void *address, size_t noBytes)
{
    assert(noBytes > 0);
    const uint8_t * const p = (uint8_t*)address;
    extern uint8_t ld_ramStart[0], ld_ramEnd[0], ld_romStart[0], ld_romEnd[0];
    
    return p >= ld_ramStart  &&  p+noBytes <= ld_ramEnd
           ||  p >= ld_romStart  &&  p+noBytes <= ld_romEnd;
    
} /* End of sc_checkUserCodeReadPtr */


/*
 * Global prototypes
 */



#endif  /* C code parts of the file */
#endif  /* SC_SYSTEMCALL_INCLUDED */
