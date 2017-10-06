#
# Makefile for GNU Make 3.81
#
# Locate all the external tools used by the other makefile fragments.
#
# This makefile fragment depends on functions.mk.
#
# Help on the syntax of this makefile is got at
# http://www.gnu.org/software/make/manual/make.pdf.
#

# Only uncomment the next line when running this fragment independently for maintenace
# purpose.
#include commonFunctions.mk

# Find the tools preferably in the folder specified by environment variable PPC_GNU_BASEDIR
# but look secondary in the system search path also. The system search path is expected as
# either colon or semicolon separated list of path designations in the environment variable
# PATH.
#   CAUTION: Blanks in path designations found in the environment variable PATH can't be
# processed. A search will not take place in those directories.
ifeq ($(OS),Windows_NT)
toolsSearchPath := $(subst ;, ,$(call w2u,$(PATH)))
else
toolsSearchPath := $(subst :, ,$(PATH))
endif
toolsSearchPath := $(PPC_GNU_BASEDIR) $(toolsSearchPath)
#$(info Search path for external tools: $(toolsSearchPath))

# Under Windows we have to look for gcc.exe rather than for gcc.
ifeq ($(OS),Windows_NT)
    dotExe := .exe
else
    dotExe :=
endif

# Now use the path search to get all absolute tool paths for later use.
cat := $(call pathSearch,$(toolsSearchPath),cat$(dotExe))
cp := $(call pathSearch,$(toolsSearchPath),cp$(dotExe))
echo := $(call pathSearch,$(toolsSearchPath),echo$(dotExe))
gawk := $(call pathSearch,$(toolsSearchPath),gawk$(dotExe))
awk := $(gawk)
gcc := $(call pathSearch,$(toolsSearchPath),powerpc-eabi-gcc$(dotExe))
g++ := $(call pathSearch,$(toolsSearchPath),powerpc-eabi-g++$(dotExe))
ar := $(call pathSearch,$(toolsSearchPath),powerpc-eabi-ar$(dotExe))
objcopy := $(call pathSearch,$(toolsSearchPath),powerpc-eabi-objcopy$(dotExe))
mkdir := $(call pathSearch,$(toolsSearchPath),mkdir$(dotExe))
mv := $(call pathSearch,$(toolsSearchPath),mv$(dotExe))
now := $(call pathSearch,$(toolsSearchPath),now$(dotExe))
pwd := $(call pathSearch,$(toolsSearchPath),pwd$(dotExe))
rm := $(call pathSearch,$(toolsSearchPath),rm$(dotExe))
rmdir := $(call pathSearch,$(toolsSearchPath),rmdir$(dotExe))
touch := $(call pathSearch,$(toolsSearchPath),touch$(dotExe))

# TODO The resource compiler is system specific. We've added the Windows variant only.
# Extend code for other systems if applicable.
ifeq ($(OS),Windows_NT)
    windres := $(call pathSearch,$(toolsSearchPath),windres$(dotExe))
endif

# The make tool is different: We need to use the same one as has been invoked by the user
# and as is executing this makefile fragment.
make := $(MAKE)

.PHONY: versionGCC helpGCC
versionGCC:
	$(info GCC: $(call u2w,$(gcc)))
	$(gcc) --version
helpGCC:
	$(gcc) -v --help

# A plausibility check that tools could be loacted.
ifeq ($(and $(make),$(gcc)),)
    $(info make: $(make), gcc: $(gcc), cp: $(cp))
    $(info Current working directory: $(shell cd))
    $(info $$(PPC_GNU_BASEDIR): $(PPC_GNU_BASEDIR))
    $(info $$(PATH): $(PATH))
    $(error Required GNU tools can't be located. Most probable reasons are: You \
            didn't install the Cycwin Unix tools, you didn't install GCC for PowerPC \
            or you didn't add the paths to the executables to the environment variable \
            PATH and you didn't set environment variable PPC_GNU_BASEDIR to point to \
            the installation of GCC)
else
    #$(info make: $(make), gcc: $(gcc), cp: $(cp))
endif
