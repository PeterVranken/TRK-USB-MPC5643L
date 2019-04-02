/**
 * @file assert.c
 * The C standard macro assert is in case of GCC for PowerPC implemented by an external
 * function. This leaves it open, how the target platform should behave when an assertion
 * fires. This module implement the wanted behavior for this project.
 */
/* Module interface
 *   __assert_func
 * Local functions
 */

/* The entire contents of this file are not required in PRODUCTION compilation. */
#ifdef DEBUG

/*
 * Include files
 */

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "typ_types.h"
#include "ivr_ivorHandler.h"
#include "assert_defSysCalls.h"


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
 
/** The number of passed assert macros with \a false condition. If the assert function is
    configured to halt the SW in case (see #ASSERT_FAILURE_BEHAVIOR) then it becomes a
    Boolean flag, which indicates, whether an assertion has fired since reset. */
volatile uint32_t assert_noOccurances SECTION(.data.OS.assert_noOccurances) = 0;

/** If an assertion has fired: The name of the causing source file. Otherwise NULL. */
volatile const char *assert_fileName SECTION(.data.OS.assert_fileName) = NULL;

/** If an assertion has fired: The name of the causing function. Otherwise NULL. */
volatile const char *assert_funcName SECTION(.data.OS.assert_funcName) = NULL;

/** If an assertion has fired: The causing source line in the source file. Otherwise NULL. */
volatile signed int assert_line SECTION(.data.OS.assert_line) = -1;
 
/** If an assertion has fired: The failing condition. Otherwise NULL. */
volatile const char *assert_expression SECTION(.data.OS.assert_expression) = NULL;

/** If at least one assertion has fired: The maximum PID of all failing processes so far.
    With other words, the ID of the process with highest privileges, which had failed so
    far. The value is -1 as long as no assertion had fired at all. */
volatile int8_t assert_PID SECTION(.data.OS.assert_PID) = -1;


/*
 * Function implementation
 */

/**
 * This is the function, which is invoked by the assert macro if the condition is false. We
 * write the information about the location of the problem into global variables, where
 * they can be inspected with the debugger, disable all external interrupts and go into an
 * infinite loop.
 */
void _EXFUN(__assert_func, (const char *fileName, int line, const char *funcName, const char *expression))
{
    /* The actual implementation of the assert function is a system call. This makes the
       assert macro usable in OS and user contexts. Whether the next function returns or
       not depends on the chosen behavior of the assert function. See
       #ASSERT_FAILURE_BEHAVIOR for details. */
    ivr_systemCall(ASSERT_SYSCALL_ASSERT_FUNC, fileName, line, funcName, expression);
    
/// @todo Remove this obsolete loop or make it dependent on the chosen behavior. What shall we do with the _Noreturn?
    /* We place the infinite loop here to avoid the compiler warning about a _Noreturn
       function returning. A user task may block but if it has been started with deadline
       monitoring then the system is not blocked. (And anyway not the tasks of higher
       priority.) */
    while(true)
        ;
        
} /* End of __assert_func */

#endif /* DEBUG */