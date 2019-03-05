#
# Generic Makefile for GNU Make 3.81
#
# Compilation and linkage of C(++) code.
#
# Help on the syntax of this makefile is got at
# http://www.gnu.org/software/make/manual/make.pdf.
#
# Copyright (C) 2012-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by the
# Free Software Foundation, either version 3 of the License, or any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
# for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# Preconditions
# =============
#
# The makefile is intended to be executed by the GNU make utility 3.81.
#   The name of the project needs to be assigned to the makefile macro projectName, see
# heading part of the code section of this makefile.
# The system search path needs to contain the location of the GNU compiler/linker etc. This
# is the folder containing e.g. gcc or gcc.exe.
#   For your convenience, the Windows path should contain the location of the GNU make
# processor. If you name this file either makefile or GNUmakefile you will just have to
# type "make" in order to get your make process running.
#   This makefile does not handle blanks in any paths or file names. Please rename your
# paths and files accordingly prior to using this makefile.
#
# Targets
# =======
#
# The makefile provides several targets, which can be combined on the command line. Get
# some help on the available targets by invoking the makefile using
#   make help
#
# Options
# =======
#
# Options may be passed on the command line.
#   The follow options may be used:
#   CONFIG: The compile configuration is one out of DEBUG (default) or PRODUCTION. By
# means of defining or undefining macros for the C compiler, different code configurations
# can be produced. Please refer to the comments below to get an explanation of the meaning
# of the supported configurations and which according #defines have to be used in the C
# source code files.
#
# Input Files
# ===========
#
# The makefile compiles and links all source files which are located in a given list of
# source directories. The list of directories is a variable set in the calling makefile,
# please look the variable srcDirList below.
#   A second list of files is found as cFileListExcl. These C/C++ or assembler files are
# excluded from build.


# General settings for the makefile.
#$(info Makeprocessor in use is $(MAKE) $(MAKECMDGOALS))

# Include some required makefile functionality.
include $(sharedMakefilePath)operatingSystem.mk
include $(sharedMakefilePath)commonFunctions.mk
include $(sharedMakefilePath)locateTools.mk

# The name of the project is used for several build products. Should have been set in the
# calling makefile but we have a reasonable default.
project ?= appName

# To support the build of different (test) applications, we append the name of those.
target := $(project)

# The name of the executable file. This is the final build product. It is requested from
# outside this makefile fragment through this name.
projectExe := $(target).s19

# Access help as default target or by several names. This target needs to be the first one
# in this file.
.PHONY: h help targets usage
.DEFAULT_GOAL := help
h help targets usage:
	$(info Usage: make [-s] [-k] [CONFIG=<configuration>] [APP=<sourceCodeFolder>] [SAVE_TMP=1] {<target>})
	$(info CONFIG: <configuration> is one out of DEBUG (default) or PRODUCTION.)
	$(info APP: safe-RTOS-VLE is a library rather than a self-contained application. It \
           can be linked with some sample client code. <sourceCodeFolder> is a folder, \
           which contains the source files of the selected sample. Default is \
           code/application/default/)
	$(info SAVE_TMP: Set to one will make the preprocessed C(++) sources and the assembler \
           files appear in the target directory bin/ppc/<configuration>/obj/)
	$(info Available targets are:)
	$(info - build: Build the executable)
	$(info - compile: Compile all C(++) and assembler source files, but no linkage etc.)
	$(info - clean: Delete all application files generated by the build process)
	$(info - cleanDep: Delete all dependency files, e.g. after changes of #include \
             statements or file renaming)
	$(info - rebuild: Same as clean and build together)
	$(info - bin/ppc/<configuration>/obj/<cFileName>.o: Compile a single C(++) or \
             assembler module)
	$(info - <cFileName>.i: Preprocess a single C(++) or assembler module. Build product \
             is sibling of source file)
	$(info - <cFileName>.asm: Generate an assembler listing for a single C(++) or \
             assembler module. Build product is sibling of source file)
	$(info - versionGCC: Print information about which compiler is used)
	$(info - helpGCC: Print usage text of compiler)
	$(info - builtinMacrosGCC: Print builtin #define's of compiler for given configuration)
	$(info - help: Print this help)
	$(error)

# Concept of compilation configurations: (TBC)
#
# Configuration PRODCUTION:
# - no self-test code
# - no debug output
# - no assertions
#
# Configuration DEBUG:
# + all self-test code
# + debug output possible
# + all assertions active
#
CONFIG ?= DEBUG
ifeq ($(CONFIG),PRODUCTION)
    ifeq ($(MAKELEVEL),1)
        $(info Compiling $(target) for production)
    endif
    cDefines := -DPRODUCTION -DNDEBUG
else ifeq ($(CONFIG),DEBUG)
    ifeq ($(MAKELEVEL),1)
        $(info Compiling $(target) for debugging)
    endif
    cDefines := -DDEBUG
else
    $(error Please set CONFIG to either PRODUCTION or DEBUG)
endif
#$(info $(CONFIG) $(cDefines))

# Where to place all generated products?
targetDir := $(call binFolder)

# Ensure existence of target directory.
.PHONY: makeDir
makeDir: | $(targetDir)obj
$(targetDir)obj:
	-$(mkdir) -pv $@

# Some core variables have already been set prior to reading this common part of the
# makefile. These variables are:
#   project: Name of the sub-project; used e.g. as name of the executable
#   srcDirList: a blank separated list of directories holding source files
#   cFileListExcl: a blank separated list of source files (with extension but without path)
# excluded from the compilation of all *.S, *.c and *.cpp
#   incDirList: a blank separated list of directories holding header files. The directory
# names should end with a slash. The list must not comprise common, project independent
# directories and nor must the directories listed in srcDirList be included
#   sharedMakefilePath: The path to the common makefile fragments like this one

# Include directories common to all sub-projects are merged with the already set project
# specific ones.
incDirList := $(call w2u,$(call trailingSlash,$(incDirList)))
#$(info incDirList := $(incDirList))

# Do a clean-up of the list of source folders.
ifeq ($(MAKELEVEL),0)
    srcDirList := $(call trailingSlash,$(srcDirList) $(APP))
endif

# Determine the list of files to be compiled.
cFileList := $(call rwildcard,$(srcDirList),*.c *.cpp *.S)

# Remove the various paths. We assume unique file names across paths and will search for
# the files later. This strongly simplifies the compilation rules. (If source file names
# were not unique we could by the way not use a shared folder obj for all binaries.)
#   Before we remove the directories from the source file designations, we need to extract
# and keep these directories. They are needed for the VPATH search and for compiler include
# instructions. Note, the $(sort) is not for sorting but to eliminate duplicates.
srcDirListExpanded := $(sort $(dir $(cFileList)))
cFileList := $(notdir $(cFileList))
# Subtract each excluded file from the list.
cFileList := $(filter-out $(cFileListExcl), $(cFileList))
#$(info cFileList := $(cFileList)$(call EOL)srcDirListExpanded := $(srcDirListExpanded))
# Translate C source file names in target binary files by altering the extension and adding
# path information.
objList := $(cFileList:.cpp=.o)
objList := $(objList:.c=.o)
objList := $(objList:.S=.o)
objListWithPath := $(addprefix $(targetDir)obj/, $(objList))
#$(info objListWithPath := $(objListWithPath))

# Include the dependency files. Do this with a failure tolerant include operation - the
# files are not available after a clean.
-include $(patsubst %.o,%.d,$(objListWithPath))

# Blank separated search path for source files and their prerequisites permit to use auto
# rules for compilation.
#$(info VPATH: $(srcDirListExpanded) $(targetDir))
VPATH := $(srcDirListExpanded) $(targetDir)

# Selection of the target. These configuration settings determine the set of machine
# instructions to be used. To ensure consistency, they are passed to the assembler, the
# compiler and the linker, even if not all of the settings affect all of the tools. (The
# linker is affected as it may have an impact on the selection of appropriate libraries.)
#   Floating point support:
#   The target selection impacts how floating point operations are implemented.
# The optimal configuration is: Only use single float operations and use the hardware
# floating point unit.
#   Use of software emulation for floating point operations is supported, too.
#   Use different data addressing modes:
#   Standard setting is the use of short addressing modes for "small" data objects in RAM
# and ROM. The memory space for the short addressing mode is limited it should therefore
# not be flooded with large objects. We limit the data size to 8 Byte (-G8). The short data
# addressing mode is enabled by -meabi -msdata=default.
#   The code compiles with long addressing for all data objects with -meabi -msdata=none.
# Code size and CPU load grow a bit. This may become an issue if the small data areas of
# 64k each (RAM and ROM) become too small to hold all "small" data objects. Prior to
# disabling the mode you should first try to reduce the size limit to 4 or 2 Byte.
useSoftwareEmulation := 0
targetFlags := -mcpu=e200z4 -mbig-endian -mvle -misel=yes -meabi -msdata=default -G8        \
               -mregnames                                                                   \
               -fshort-double -fsingle-precision-constant
ifeq ($(useSoftwareEmulation),1)
    targetFlags += -msoft-float
else
    targetFlags += -mhard-float
endif

# Choose optimization level for production compilation.
#   Note, we need the setting here since it has to be passed identically to both, assembler
# and C source files; if we optimize for size then some related assembler code is
# conditionally compiled, which is otherwise not.
    # O0: 100%
    # Og: 72%
    # O3: 41%
    # O2: 41%
    # O1: 52%
    # Os: 50%, requires linkage of crtsavres.S
    # Ofast: 41%, likely same as -O3
productionCodeOptimization := -Os
# Choose C library.
#   The switch -specs=nosys.specs links the generically implemented C library against a
# stub library that satisfies the low level I/O routines without providing true
# functionality. We must not use this switch if we want to have the printf
# functionality with true I/O. If we remove this switch we need however to provide our
# own I/O stub to satisfy the low level I/O of the clib. See file nwl_ioSysNewLib.c.
cClibSpec := --sysroot=$(call w2u,$(GCC_POWERPC_HOME))/powerpc-eabivle/newlib
#cClibSpec += -specs=nosys.specs

# Pattern rules for assembler language source files.
asmFlags = $(targetFlags)                                                                   \
           -Wall                                                                            \
           -MMD -Wa,-a=$(patsubst %.o,%.lst,$@)                                             \
           $(foreach path,$(call noTrailingSlash,$(srcDirListExpanded) $(incDirList)),-I$(path))\
           $(cDefines) $(foreach def,$(defineList),-D$(def))                                \
           -Wa,-g -Wa,-gdwarf-2

ifneq ($(CONFIG),DEBUG)
    asmFlags += $(productionCodeOptimization)
endif
#$(info asmFlags := $(asmFlags))

$(targetDir)obj/%.o: %.S
	$(info Assembling source file $<)
	$(gcc) -c $(asmFlags) -o $@ $<


# Pattern rules for compilation of, C and C++ source files.

# TODO We could decide to not add -Winline if we optimize for size. With -Os it's quite
# normal that an inline function is not implemented as such.
cFlags = $(targetFlags) -mno-string                                                         \
         -fno-common -fno-exceptions -ffunction-sections -fdata-sections                    \
         -fshort-enums -fdiagnostics-show-option -finline-functions -fmessage-length=0      \
         -fzero-initialized-in-bss -fno-tree-loop-optimize                                  \
         -Wall -Wno-main -Wno-old-style-declaration -Wextra -Wstrict-overflow=4             \
         -Wmissing-declarations -Wno-parentheses -Wdiv-by-zero -Wcast-align -Wformat        \
         -Wformat-security -Wignored-qualifiers -Wsign-conversion -Wsign-compare            \
         -Werror=missing-declarations -Werror=implicit-function-declaration                 \
         -Wno-nested-externs -Werror=int-to-pointer-cast -Werror=pointer-sign               \
         -Werror=pointer-to-int-cast -Werror=return-local-addr -Werror=missing-prototypes   \
         -Werror=missing-field-initializers                                                 \
         $(cClibSpec) -MMD -Wa,-a=$(patsubst %.o,%.lst,$@) -std=gnu11                       \
         $(foreach path,$(call noTrailingSlash,$(srcDirListExpanded) $(incDirList)),-I$(path))\
         $(cDefines) $(foreach def,$(defineList),-D$(def))
ifeq ($(SAVE_TMP),1)
    # Debugging the build: Put preprocessed C file and assembler listing in the output
    # directory
    cFlags += -save-temps=obj -fverbose-asm
endif
# Debug settings see https://gcc.gnu.org/onlinedocs/gcc/Debugging-Options.html#Debugging-Options
ifeq ($(CONFIG),DEBUG)
    cFlags += -g3 -gdwarf-2 -Og -Winline
else
    cFlags += -g1 -gdwarf-2 $(productionCodeOptimization)
    ifneq ($(productionCodeOptimization),-Os)
        cflags += -Winline
    endif
endif
#$(info cFlags := $(cFlags))

$(targetDir)obj/%.o: %.c
	$(info Compiling C file $<)
	$(gcc) -c $(cFlags) -o $@ $<

#$(targetDir)obj/%.o: %.cpp
#	$(info Compiling C++ file $<)
#	$(gcc) -c $(cFlags) -o $@ $<

# Create a preprocessed source file, which is convenient to debug complex nested macro
# expansion, or an assembler listing, which is convenient to understand the
# close-to-hardware parts of the code.
#   Note, different to the compilers does the assembler not take argument -o. It writes the
# output to stdout and we need redirection.
%.i %.asm: %.S
	$(info Preprocessing assembler file $(notdir $<) to text file $(patsubst %.S,%$(suffix $@),$<))
	$(gcc) $(if $(call eq,$(suffix $@),.i),-E,-S) $(filter-out -MMD,$(cFlags)) $< $(if $(call eq,$(suffix $@),.i),-o, >) $(patsubst %.S,%$(suffix $@),$<)

%.i %.asm: %.c
	$(info Preprocessing C file $(notdir $<) to text file $(patsubst %.c,%$(suffix $@),$<))
	$(gcc) $(if $(call eq,$(suffix $@),.i),-E,-S) $(filter-out -MMD,$(cFlags)) -o $(patsubst %.c,%$(suffix $@),$<) $<

%.i %.asm: %.cpp
	$(info Preprocessing C++ file $(notdir $<) to text file $(patsubst %.cpp,%$(suffix $@),$<))
	$(gcc) $(if $(call eq,$(suffix $@),.i),-E,-S) $(filter-out -MMD,$(cFlags)) -o $(patsubst %.cpp,%$(suffix $@),$<) $<


## A general rule enforces rebuild if one of the configuration files changes
#$(objListWithPath): GNUmakefile ../shared/makefile/compileAndLink.mk                        \
#                    ../shared/makefile/locateTools.mk ../shared/makefile/commonFunctions.mk \
#                    ../shared/makefile/parallelJobs.mk


# More than 30 Years of DOS & Windows but the system still fails to handle long command
# lines. We write the names of all object files line by line into a simple text file and
# will only pass the name of this file to the linker.
$(targetDir)obj/listOfObjFiles.txt: $(objListWithPath)
	$(info Create linker input file $@)
	$(file >$@,$^)

# Let the linker create the flashable binary file.
#   CAUTION: An unsolved problem with GCC 4.9.4 is the switch -fshort-double, which is
# highly desired in software emulation mode and unavoidable in hardware mode. There seems
# to be no set of libraries, which has been compiled in this mode. If we call a library
# function using the type double for in- or output then the code doesn't crash but produces
# bad results. A statement like x=exp(y); will produce the wrong result for x. We need to
# do, what is anyway strongly recommended - using the float variants of these library
# functions. Our example would become x=expf(y);, which works as expected. This problem can
# only be solved by rebuilding the C library with our settings.
#   Note, there's no such function for printf & co, e.g. printff. See sample "printf" for
# a proposed, particular work-around.
#   -nostartfiles: The compiler generates the call of a function __eabi(stackPointer) at
# the entry into main. __eabi is a function in the clib, which does the initialization of
# the clib, in particular, the C++ constructors seem to be called. If -nostartfiles is
# given then the linker won't link the related files from the clib but the compiler still
# calls __eabi() so that we need to offer a stub then. The clib is not initialized and it
# can be that elements from it won't work.
lFlags = -Wl,-Tmakefile/linkerControlFile.ld -Wl,--gc-sections $(targetFlags)               \
         -Wl,-sort-common -Wl,-Map="$(targetDir)$(target).map" -Wl,--cref                   \
         -Wl,--warn-common,--warn-once,--orphan-handling=warn -Wl,-g                        \
         $(cClibSpec)

$(targetDir)$(target).elf: $(targetDir)obj/listOfObjFiles.txt makefile/linkerControlFile.ld
	$(info Linking project. Mapfile is $(targetDir)$(target).map)
	$(gcc) $(lFlags) -o $@ @$< -lm

# Create hex file from linker output.
$(targetDir)$(target).s19: $(targetDir)$(target).elf
	$(objcopy) -O ihex $< $(patsubst %.elf,%.hex,$<)
	$(objcopy) -O symbolsrec $< $(patsubst %.elf,%.s19,$<)

# Delete all dependency files ignoring (-) the return code from Windows because of non
# existing files.
.PHONY: cleanDep
cleanDep:
	-$(rm) -f $(targetDir)obj/*.d

# Delete all application products ignoring (-) the return code from Windows because of non
# existing files.
.PHONY: clean
clean:
	-$(rm) -f $(targetDir)obj/*
	-$(rm) -f $(targetDir)$(target).*
