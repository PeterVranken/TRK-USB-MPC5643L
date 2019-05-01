/**
 * @file tcc_testCppCompilation.cpp
 * A dummy C++ module to basically prove the build capabilities for C++.
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
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

extern "C"
{
#include "typ_types.h"
}

#include "tcc_testCppCompilation.h"


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
 
/** One object of our test class is global in order to test if the constructor is still
    found and executed. */
static HelloWorld helloWorld_en(/* isEnglish */ true);

/** The class global counter of calls. */
unsigned int HelloWorld::_noGreetings = UINT_MAX;


/*
 * Function implementation
 */

/**
 * The class constructor initializes the class data such that the call of the constructor
 * and the correctness of its execution become visible when executing an interface
 * function.
 *   @param isEn
 * Boolean descision whether to have an English or German greeting.
 */
HelloWorld::HelloWorld(bool isEn)
    : isEnglish(isEn)
{
    /* The next statement checks the data section initialization for C++ class data. */
    if(_noGreetings == UINT_MAX)
        _noGreetings = 0;
    
} /* End of HelloWorld::HelloWorld */



/**
 * Class interface: Write the greeting to stdout.
 *   @return
 * The number of calls of this function is counted globally for all instances of the class.
 * Get the count. The very first call of this interface should return 1 -- otherwise the
 * data initialization is bad.
 */
unsigned int HelloWorld::sayHello()
{
    /* Globally count calls across all objects. */
    ++ _noGreetings;
    
    static unsigned int noGreetingsEn = 0
                      , noGreetingsDe = 0;
    if(isEnglish)
    {
        ++ noGreetingsEn;
        iprintf("Hello World (%u) from C++ code\r\n", _noGreetings);
    }
    else
    {
        ++ noGreetingsDe;
        iprintf("Ein Hallo an die Welt (%u) vom C++ Code\r\n", _noGreetings);
    }

    return _noGreetings;
    
} /* End of HelloWorld::sayHello */


extern "C"
{

/**
 * C compatible wrapper to make the class interface callable from ordinary C.
 */
unsigned int tcc_sayHello(bool isEnglish)
{
    if(isEnglish)
    {
        /* The English speaking object is globally instantiated and should work out of the
           box. */
        return helloWorld_en.sayHello();
    }
    else
    {
        /* The German speaking object is created on first use. It'll go onto the heap. */
        //static HelloWorld helloWorld_de = new HelloWorld(/* isEnglish */ false);
        static HelloWorld helloWorld_de(/* isEnglish */ false);
        return helloWorld_de.sayHello();
    }
} /* End of tcc_sayHello */


/** As an alternative to -fno-threadsafe-statics in this simple environment: We provide
    stubs for the otherwise misisng synchronization functions. */
int __cxa_guard_acquire(int64_t *guardObj ATTRIB_UNUSED)
{
    /* Ignore race conditions in this software. Always let the object be initialized. */
    return 1;
}

void __cxa_guard_release(int64_t *guardObj ATTRIB_UNUSED)
{
}


} /* End of extern "C" */


