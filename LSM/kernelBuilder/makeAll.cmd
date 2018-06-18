@echo off
:: See usage message for help. See
:: http://en.wikibooks.org/wiki/Windows_Programming/Programming_CMD for syntax of batch
:: scripts.
goto LStart
:LUsage
echo usage: makeAll
echo   Build all kernelBuilder samples in all compilation configurations
goto :eof

:: 
:: Copyright (c) 2018, Peter Vranken, Germany (mailto:Peter_Vranken@Yahoo.de)
:: 

:LStart
REM if /i "%1" == "" goto LUsage
if /i "%1" == "-h" goto LUsage
if /i "%1" == "--help" goto LUsage
if /i "%1" == "-?" goto LUsage
if /i "%1" == "/?" goto LUsage
if /i "%1" == "/h" goto LUsage
:: Limit the allowed number of parameters.
if not "%1" == "" goto LUsage

setlocal

:: Process all command line arguments in a loop.
:: The loop variable needs to be a colon followed by a single character.
for %%A in (alternatingContexts chainedContextCreation simpleRTOS simpleSampleFromReadme) do (
    echo Processing kernelBuilder sample %%A
    call :buildAllConfigs code/samples/%%A/
)

:: End script before the parser runs into the local function definitions!
goto :eof

:: This is a local function. Code is executed only where function is invoked using CALL.
:buildAllConfigs
setlocal
    :: Add your commands here using parameters %1, %2, ... and setting %retval%
    echo mingw32-make -s SAVE_TMP=1 build APP=%1 INSTR=BOOK_E CONFIG=DEBUG
    mingw32-make -s SAVE_TMP=1 build APP=%1 INSTR=BOOK_E CONFIG=DEBUG
    echo mingw32-make -s SAVE_TMP=1 build APP=%1 INSTR=BOOK_E CONFIG=PRODUCTION
    mingw32-make -s SAVE_TMP=1 build APP=%1 INSTR=BOOK_E CONFIG=PRODUCTION
    echo mingw32-make -s SAVE_TMP=1 build APP=%1 INSTR=VLE CONFIG=DEBUG
    mingw32-make -s SAVE_TMP=1 build APP=%1 INSTR=VLE CONFIG=DEBUG
    echo mingw32-make -s SAVE_TMP=1 build APP=%1 INSTR=VLE CONFIG=PRODUCTION
    mingw32-make -s SAVE_TMP=1 build APP=%1 INSTR=VLE CONFIG=PRODUCTION

endlocal & set result=%retval% 
