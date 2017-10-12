# Initialization script for Code Warrior 10.7 Debugger when running a MPC5643L application
# in Lockstep Mode and using the VLE instruction set.
#   See https://community.nxp.com/thread/431421:
#   The debugger behaves improperly insofar it doesn't read the MMU settings back after
# start of the SW. So it is not aware of the actual page settings made in the MMU. We need
# to inform CW by side effect: If letting it write the same register values as the SW
# itself will do anyway later then it adopts these settings for its own purpose.
#   The debugger is impacted by these settings at minimum to recognize the instruction set
# in use. It needs to know whether to disassemble as VLE or Book E. Furthermore, this
# awareness is required to properly set break points and to step through the code.
#   This script needs an update any time you alter the SW with respect to the MMU
# settings. See TODO below.
#
# This script is based on the NXP sample script MPC5643L_VLE.tcl.


######################################
# Initialize target variables
######################################
# var booke_vle: BOOKE = booke, VLE = vle
set booke_vle "vle"


######################################
## Register group definitions
######################################

# GPR registrer group
set GPR_GROUP "General Purpose Registers/"
# Special purpose register group
set SPR_GROUP "e200z4 Special Purpose Registers/"
# TLB1 registers group
set TLB1_GROUP "regPPCTLB1/"


###############################################################
# Init core z4 registers for lockstep mode required to run
# SRAM initialization program and general debugging.
# Must init all registers that are unaffected by system reset.
###############################################################
proc init_lsm_regs {} {
    global GPR_GROUP
    global SPR_GROUP
    
    puts "initialize core registers for LockStep mode"    
    # init group r0-r31
    reg ${GPR_GROUP}GPR0..${GPR_GROUP}GPR31 = 0x0
    reg ${GPR_GROUP}CR = 0
    reg ${GPR_GROUP}CTR = 0
    # reset watchdog timer
    reg ${SPR_GROUP}SPRG0..${SPR_GROUP}SPRG9 = 0x0
    reg ${SPR_GROUP}USPRG0 = 0x0
    reg ${SPR_GROUP}SRR0 = 0x0
    reg ${SPR_GROUP}SRR1 = 0x0
    reg ${SPR_GROUP}CSRR0 = 0x0
    reg ${SPR_GROUP}CSRR1 = 0x0
    reg ${SPR_GROUP}MCSRR0 = 0x0
    reg ${SPR_GROUP}MCSRR1 = 0x0
    reg ${SPR_GROUP}DSRR0 = 0x0
    reg ${SPR_GROUP}DSRR1 = 0x0
    reg ${SPR_GROUP}ESR = 0x0
    reg ${SPR_GROUP}SPEFSCR = 0x0
    reg ${GPR_GROUP}XER = 0x0
    reg ${SPR_GROUP}MCAR = 0x0
    reg ${SPR_GROUP}TBL = 0x0
    reg ${SPR_GROUP}TBU = 0x0
    reg ${SPR_GROUP}DVC1 = 0x0
    reg ${SPR_GROUP}DVC2 = 0x0
    reg ${SPR_GROUP}DBCNT = 0x0
    reg ${SPR_GROUP}DEAR = 0x0
    reg ${SPR_GROUP}DEC = 0x0
    reg ${SPR_GROUP}DECAR = 0x0
    reg ${SPR_GROUP}MAS0..${SPR_GROUP}MAS6 = 0x0
    # the other core registers are changed during init
    
}
# init_lsm_regs



#######################################################################
# Initialize the core registers: reset timers, MMU and set SPE support.
#######################################################################
proc initMMU {} {
    global SPR_GROUP
    global booke_vle
    global TLB1_GROUP

    # Reset watch dog timer.
    reg ${SPR_GROUP}TCR = 0x0

    # TLB entries as in actualy SW: The debugger don't read the values set by the SW in the
    # initialization phase back and might operate using wrong assumptions. This leads e.g.
    # to a bad interpretation of VLE code as Book E when displaying the disassembly.
    # Furthermore, setting break points and stepping fails.
    #   TODO This script needs an update any time you alter the SW with respect to the MMU
    # settings. The MMU table entries are reflected in the debugger's (virtual) device
    # regPPCTLB1. The register values of this device at runtime need to be initialized
    # here. The easiest way to get the required binary values of the registers is to start
    # the SW with the default setting (one MMU entry only, commented out), halt it after
    # having done the MMU initialization and using the debugger to read the values: See
    # Window registers, expand device regPPCTLB1 and copy & paste the values from the
    # debugger to here.
    if {$booke_vle == "vle"} {
        # VLE page
        
        # Default entry for VLE applications. This is the standard MMU setting made by the
        # BAM.
        #reg ${TLB1_GROUP}MMU_CAM0 = 0xB0000008FE0800000000000000000001
        
        reg ${TLB1_GROUP}MMU_CAM0  = 0x50000000fe0000000000000000000001
        reg ${TLB1_GROUP}MMU_CAM1  = 0x50000000fe00000000f0000000f00001
        reg ${TLB1_GROUP}MMU_CAM2  = 0x38000008fe0000004000000040000001
        reg ${TLB1_GROUP}MMU_CAM3  = 0x4000000ada0000008ff000008ff00001
        reg ${TLB1_GROUP}MMU_CAM4  = 0x3000000ada0000008ff400008ff40001
        reg ${TLB1_GROUP}MMU_CAM5  = 0x4800000ada000000c3f80000c3f80001
        reg ${TLB1_GROUP}MMU_CAM6  = 0x4800000ada000000ffe00000ffe00001
        reg ${TLB1_GROUP}MMU_CAM7  = 0x4000000ada000000fff00000fff00001
        reg ${TLB1_GROUP}MMU_CAM8  = 0x3000000ada000000fff40000fff40001
        reg ${TLB1_GROUP}MMU_CAM9  = 0x3000000ada000000fff90000fff90001
        reg ${TLB1_GROUP}MMU_CAM10 = 0x4000000ada000000fffc0000fffc0001
        
    } else {
        # BOOKE page
        # Default entry for Book E applications. This is the standard MMU setting made by
        # the BAM.
        reg ${TLB1_GROUP}MMU_CAM0 = 0xB0000008FC0800000000000000000001
    }

    # Enable SPE and Debug interrupts.
    reg ${SPR_GROUP}MSR = 0x2002000
}
# initMMU




########################################
## Environment Setup: 32bit meme access
## or numbers start with hex notation.
########################################
proc envsetup {} {
    radix x 
    config hexprefix 0x
    config MemIdentifier v 
    config MemWidth 32 
    config MemAccess 32 
    config MemSwap off
}
 

# Env setup
envsetup

# MMU setup
initMMU
