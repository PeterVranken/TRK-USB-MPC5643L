# This configuration script is compatible with MPC564xL MCUs.
# Features:
# - Disables the Software Watchdog Timer (SWT) and Core WDT
# - Initializes the core MMU with a single 4GB page to allow application
# start debug
# - Initializes the system ECC SRAM
# - Sets the PC to the application start address from the internal flash.
# If the entry point isn't available PC is set to BAM start address.
# - Autodetects the LS/DP mode an applies different initializations:
# -- LS: initializes cores registers, all system SRAM
# -- DP: each core SRAM, starts the second core, sets entry point for core_1
#
# Rev. 1.2 - get IVPR, IVORxx values from symbolics
#           - searches & sets application entry point
# Rev. 1.3 - fix reading RCHW 
#
# VERSION: 1.3

######################################
# Initialize target variables
######################################
# var booke_vle: BOOKE = booke, VLE = vle
set booke_vle booke

######################################
## Register group definitions
######################################

# GPR registrer group
set GPR_GROUP "General Purpose Registers/"
# special purpose register group
set SPR_GROUP "e200z4 Special Purpose Registers/"
#TLB1 registers group
set TLB1_GROUP "regPPCTLB1/"

###############################################################
# Init core z4 registers for LockStep mode required to run
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

################################################
# Initialize the core registers: reset timers,
# MMU, set SPE support and set IVORs.
################################################
proc init_z4 {ivpr} {
    global GPR_GROUP
    global SPR_GROUP
    global booke_vle
    global TLB1_GROUP

       # reset watch dog timer
    reg ${SPR_GROUP}TCR = 0x0

    # TLB entry 0: base address = 0x0000_0000, 4GB, not guarded, cache inhibited, access all
    if {$booke_vle == "vle"} {
        # VLE page
        reg ${TLB1_GROUP}MMU_CAM0 = 0xB0000008FE0800000000000000000001
    } else {
        # BOOKE page
        reg ${TLB1_GROUP}MMU_CAM0 = 0xB0000008FC0800000000000000000001
    }

    # set IVPR vector base table high 16-bit and IVOR0 lower 16-bit
    set ivor0 0x[format %x [expr $ivpr & 0xFFFF]]
    reg ${SPR_GROUP}IVPR = 0x[format %x [expr $ivpr & 0xFFFF0000]]
    # Initiliaze only the first 16 IVORs
    for {set i 0} {$i < 16} {incr i} {
        # The IVOR branch table must have a 16 byte aligment.
        reg ${SPR_GROUP}IVOR${i} %d = [expr $ivor0 + $i * 0x10]
    }

    # enable SPE and Debug interrupts
    reg ${SPR_GROUP}MSR = 0x2002000
}

#################################################
# Initialize a RAM 'range' from 'start' address,
# downloading the init program at 0x4000_0000.
#################################################
proc init_ram {start range} {
      global GPR_GROUP
      global booke_vle

    puts "init ECC SRAM $start:$range"
      # default SRAM address
      set pstart 0x40000000
      # the offset from the given start address
      # at which the init prgram starts to init SRAM
    set offset 0x0
    # stmw write page size = 128 = 4bytes * 32 GPRS
    set psize 0x80

    if {$start == $pstart} {
        # init first 4 bytes (mem access) x 128 = 512
        # bytes to avoid reading the memory around PC
        # after stopping the core
        mem $start 256 = 0x0
        # base init address
        set offset 0x80
    }
    
    # address to start initialization
    set start [expr {$start + $offset}]
    
    # load add into GPR
    reg ${GPR_GROUP}GPR11 %d = $start

    # compute actual times stmw is called
    # and set counter
    set c [expr ((($range - $offset)/$psize))]
    reg ${GPR_GROUP}GPR12 %d = $c

    # execute init ram code
    if {$booke_vle == "vle"} {
        #mtctr r12
        mem $pstart = 0x7D8903A6
        #stmw r0,0(r11)
        mem [format %x [expr $pstart + 0x4]] = 0x180B0900
        #addi r11,r11,128
        mem [format %x [expr $pstart + 0x8]] = 0x1D6B0080
        #bdnz -8
        mem [format %x [expr $pstart + 0xc]] = 0x7A20FFF8
        # infinte loop
        #se_b *+0
        mem [format %x [expr $pstart + 0x10]] = 0xE8000000
    } else {
        #mtctr r12
        mem $pstart = 0x7D8903A6
        #stmw r0,0(r11)
        mem [format %x [expr $pstart + 0x4]] = 0xBC0B0000
        #addi r11,r11,128
        mem [format %x [expr $pstart + 0x8]] = 0x396B0080
        #bdnz -8
        mem [format %x [expr $pstart + 0xc]] = 0x4200FFF8
        # infinte loop
        #se_b *+0
        mem [format %x [expr $pstart + 0x10]] = 0x48000000    
    }
    
    # set PC to the first init instruction
    reg ${GPR_GROUP}PC = $pstart
    # execute init ram code
    # timeout 1 second to allow the code to execute
    go 1
    stop
    if {[reg ${GPR_GROUP}PC -np] != [expr $pstart + 0x10]} {
        puts  "Exception occured during SRAM initialization."
    } else {
        puts "SRAM initialized."
    }
}

#################################################################
# Tries to obtain the program entry point from binary debug info
# or by searching for the first flash boot sector. If neither
# is succesfull it returns the BAM start address.
#################################################################
proc get_entry_point {} {
    puts "Get application entry point ..."
    # check symbolic names first
    set reset_vector_addr 0x3
    catch {set reset_vector_addr [evaluate __startup]}
    # If no debug info available search for a FLASH boot sector
    if {$reset_vector_addr == 0x3} {
        # RCHW addresses of each Flash boot sector
        set SECTORS_ADD [list 0x0000 0x4000 0x10000 0x1C000 0x20000 0x30000]
        puts "Searching for boot sectors ..."
        set rchw_value 0x0
        foreach rchw $SECTORS_ADD {
            catch {set rchw_value [mem $rchw -np]}
            if {[expr $rchw_value & 0xFF0000] == 0x5A0000} {
                catch {set reset_vector_addr [mem [format "0x%x" [expr $rchw + 0x4]] -np]}
                set reset_vector_addr [format "0x%x" [expr $reset_vector_addr & 0xFFFFFFFF]]
                puts "Found boot sector at $rchw and entry point at $reset_vector_addr."  
                break
            }
        }        
    }
    if {$reset_vector_addr == 0x3} {
        puts "No entry point available. Set entry point to BAM start address."
        set reset_vector_addr 0xFFFFC000
    }
    return $reset_vector_addr
}  

######################################
# MCU system initialization
######################################
proc mpc564xl_init {} {
      global GPR_GROUP
      global booke_vle
      
      puts "Start initializing the MCU ..."
    reset hard
            
    # Explicitly stop Core0
    stop
    
    puts "initialize Core_0 ..."
    # Initialize core interrupts in order to catch any exceptions during
    # MCU initialization. The following sequence queries symbolics to
    # obtain the address of the wellknown symbol "ivor_branch_table".
    set ivor_table 0x40000000
    set r [catch {set ivor_table [evaluate ivor_branch_table_p0]}]
    init_z4 $ivor_table
    
    # Disable Watchdog Timer
    mem 0xFFF38010 = 0x0000C520
    mem 0xFFF38010 = 0x0000D928
    mem 0xFFF38000 = 0x8000010A

    # SRAM memory sizes for supported MCUs.
    # MPC564xL, where x  = 3
    # Supported memory LSM/DPM configuration(s):
    # (128K/64K) 0x20000/0x10000
    set mem_size 0x20000

    set SIUL_MIDR1 0x50000
    puts "reading SIUL.MIDR1 ..."
    set r [catch {set SIUL_MIDR1 [mem 0xC3F90004]}]
    if {$r == 0} {
        # get x from 564x
        set r [format %x [expr ($SIUL_MIDR1/0x10000) & 0xFFFF]]
        if {$r == 5643} {
            set majmask [expr ($SIUL_MIDR1 & 0xF0)]
            set minmask [expr ($SIUL_MIDR1 & 0xF)]
            if {$majmask == 0} {
                set cut 1    
            } else {
                if {$minmask == 1} {
                    set cut 3
                } else {
                    set cut 2
                }
            }
            puts [format "Current MCU found as MPC%sL cut %s" $r $cut]
        } else {
            puts  "Found MCU ID $r. The device isn't supported by current script."
        }
    } else {
        puts  "Unable to detect MCU."
    }

    # Core0 is configured but we need to make sure the core registers
    # are initialized in LSM before initializing the SRAM.
    set lsm_dpm "lsm"
    set SSCM_STATUS 0x80000000
    puts "reading SSCM.STATUS ..."
    catch {set SSCM_STATUS [mem 0xC3FD8000]}
    if {[expr $SSCM_STATUS & 0x80000000]} {
        puts "MCU found in lockstep mode."
          set lsm_dpm "lsm"
    } else {
        puts "MCU found in decoupled parallel mode."
        set lsm_dpm "dpm"
    }
    
    if {$lsm_dpm == "lsm"} {
        # initialize registers for LSM
        init_lsm_regs
          # initialize ECC SRAM for LSM
        init_ram 0x40000000 $mem_size
    } else {
          # initialize ECC SRAM for DPM Core0/1 
          set mrange  [format "0x%x" [expr $mem_size/2]]
        init_ram 0x40000000 $mrange
        # init Core1 SRAM
        init_ram 0x50000000 $mrange

        # check if core_1 is enabled in launch configuration
        set r [changecore 1]
        if {$r == 0} {
            changecore 0
            # don't wake core_1 if we don't know the entry point,
            # eg. for connect launch the core_0 code should wake core_1.
            set r [catch {set core_1_start [evaluate __start_p1]}]
            if {$r == 0} {
                # 1. set infinite loop in order to catch the Core 1
                # before executing start up code.
                # 2. activate Core1: set reset address
                # PC at __start_p1 in SSCM.DPMBOOT
                if {$booke_vle == "vle"} {
                    mem 0x50000000 = 0xE8000000
                    # vle bit set
                    mem 0xC3FD8018 = 0x50000002
                } else {
                    mem 0x50000000 = 0x48000000
                    # booke bit set
                    mem 0xC3FD8018 = 0x50000000
                }

                # enter key sequence to start second core
                # SSCM.DPMKEY
                mem 0xC3FD801C = 0x00005AF0
                mem 0xC3FD801C = 0x0000A50F

                # process commands on the second core
                changecore 1

                puts "initialize Core_1 ..."
                # explicitly stop the core                
                stop

                # init second core
                # query symbolics for second core ivor branch table address
                set ivor_table 0x50000000
                catch {set ivor_table [evaluate ivor_branch_table_p1]}
                init_z4 $ivor_table
                # set entry point for Core1
                reg ${GPR_GROUP}PC = $core_1_start
                # set SP to an unaligned (4 bytes) to avoid creating invalid stack frames
                reg ${GPR_GROUP}SP = 0x3
            }
        }
        changecore 0
    }

    # initialize PC after reset with application start address or BAM otherwise
       reg ${GPR_GROUP}PC = [get_entry_point]
    # set SP to an unaligned (4bytes) to avoid creating invalid stack frames
       reg ${GPR_GROUP}SP = 0x3

    puts "Finish initializing the MCU."
}
###############################
## Change core command channel.
###############################
proc changecore {b} {
    set r [catch {switchtarget -quiet -pid=$b}]
    # if this command fails use workaround
    if {$r == 1} {
        set a [switchtarget -quiet -cur]
         if {$b == 0} { set c [expr {$a - 1}]
        } else { set c [expr {$a + 1}] }
         set r [catch {switchtarget -quiet $c}]
    }
    return $r
}

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
 
# env setup
envsetup

# main entry
mpc564xl_init
