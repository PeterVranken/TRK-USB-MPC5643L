#ifndef TCC_TESTCPPCOMPILATION_INCLUDED
#define TCC_TESTCPPCOMPILATION_INCLUDED
/**
 * @file tcc_testCppCompilation.h
 * Definition of global interface of module tcc_testCppCompilation.c
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

#ifdef __cplusplus
/** A dummy class to prove the software build for C++ sources. */
class HelloWorld
{
public:
    /** A class constructor to prove the correct software initialization. */
    HelloWorld(bool isEnglish);
    
    /** A test function. */
    unsigned int sayHello(void);

private:
    static unsigned int _noGreetings;
    bool isEnglish;
};
#endif


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */

#endif  /* TCC_TESTCPPCOMPILATION_INCLUDED */
#ifdef __cplusplus
extern "C"
{
#endif

/** C compatible wrapper to make the class interface callable from ordinary C. */
unsigned int tcc_sayHello(bool isEnglish);

#ifdef __cplusplus
}
#endif

