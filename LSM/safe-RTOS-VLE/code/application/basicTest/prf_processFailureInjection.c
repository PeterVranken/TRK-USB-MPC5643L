/**
 * @file prf_processFailureInjection.c
 * Implementation of task functions. The tasks and their implementation belong into the
 * sphere of the protected user code. They are are defined in the sphere of unprotected
 * operating system code and anything which relates to their configuration cannot be
 * changed anymore by user code.
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
 *   prf_nestStackInjectError
 *   prf_taskInjectError
 *   prf_task17ms
 *   prf_task1ms
 * Local functions
 *   random
 *   injectError
 */

/*
 * Include files
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "typ_types.h"
#include "del_delay.h"
#include "rtos.h"
#include "gsl_systemLoad.h"
#include "syc_systemConfiguration.h"
#include "prr_processReporting.h"
#include "prf_processFailureInjection.h"


/*
 * Defines
 */
 

/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
void prf_nestStackInjectError(unsigned int remainingLevels);
 
 
/*
 * Data definitions
 */
 
/** The next error to inject. This object is written by task prs_taskCommandError and read
    by prf_taskInjectError. There are no race conditions between these two tasks. */
prf_cmdFailure_t DATA_SHARED(prf_cmdFailure) = { .kindOfFailure = prf_kof_noFailure
                                                 , .noRecursionsBeforeFailure = 0
                                                 , .value = 0
                                                 , .address = 0
                                                 , .expectedNoProcessFailures = 0
                                                 , .expectedNoProcessFailuresTolerance = 0
                                                 , .expectedValue = 0
                                               };

/** Task invocation counter. Here for task1ms. */
static unsigned long SDATA_PRC_FAIL(_cntTask1ms) = 0;

/** A non discarded, global variable, used as RHS of read operations, which must not be
    optimzed out. */
volatile uint32_t BSS_PRC_FAIL(prf_u32ReadDummy);
    
/** A tiny assembler function located in OS RAM area and implemented as C array of
    instructions. We need to use the OS memory space for test case \a
    prf_kof_spCorruptAndWait, or #PRF_ENA_TC_PRF_KOF_SP_CORRUPT_AND_WAIT. */
static uint32_t _fctPrivilegedInstrInOSRAM[] SECTION(.data.OS._fctPrivilegedInstrInOSRAM) =
    { [0] = 0x7C008146  /* wrteei 1 */
    , [1] = 0x00040000  /* se_blr; se_illegal */
    };

/* The assembler routine prepared in \a prf_fctPrivilegedInstrInOSRAM cannot be called
   directly. The compiler tries branching there, which fails due to the distance ROM to RAM
   of more than 20 Bit. We need to install a function pointer. */
static void (* const _pFctPrivilegedInstrInOSRAM)(void) =
                                        (void(*)(void))(&_fctPrivilegedInstrInOSRAM[0]);


/*
 * Function implementation
 */

/**
 * Compute a random number in the range 0..#RAND_MAX.
 *   @return
 * Get the random number.
 *   @remark
 * The implementation of the function founds on a task call in another process, which makes
 * it quite expensive in terms of CPU load. Do not use it frequently.
 */
static uint32_t random(void)
{
    /* This simple implementation of rand() taken from
       https://code.woboq.org/userspace/glibc/stdlib/random_r.c.html, downloaded on Mar 18,
       2019. */
    static uint32_t state[1] SECTION(.sdata.P2.state) = {[0] = 1u};
    int32_t val = ((state[0] * 1103515245U) + 12345U) & 0x7fffffff;
    state[0] = (uint32_t)val;
    
    return (uint32_t)val;

} /* End of random */



/**
 * Implementation of the intentionally produced failures.
 */
static void injectError(void)
{
    switch(prf_cmdFailure.kindOfFailure)
    {
#if PRF_ENA_TC_PRF_KOF_JUMP_TO_RESET_VECTOR == 1
    case prf_kof_jumpToResetVector:
        ((void (*)(void))0x00000010)();
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_JUMP_TO_ILLEGAL_INSTR == 1
    case prf_kof_jumpToIllegalInstr:
        ((void (*)(void))0x00000008)();
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_NO_FAILURE == 1
    case prf_kof_noFailure:
        /* Here, we can test the voluntary task termination for a deeply nested stack. */
        rtos_terminateTask(0);
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_USER_TASK_ERROR == 1
    case prf_kof_userTaskError:
        rtos_terminateTask(-1);
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_PRIVILEGED_INSTR == 1
    case prf_kof_privilegedInstr:
        asm volatile ("wrteei 1\n\t" ::: /* Clobbers */ "memory");
        break;
#endif
        
#if PRF_ENA_TC_PRF_KOF_CALL_OS_API == 1
    case prf_kof_callOsAPI:
        rtos_OS_suspendAllInterruptsByPriority(15);
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_TRIGGER_UNAVAILABLE_EVENT == 1
    case prf_kof_triggerUnavailableEvent:
        rtos_triggerEvent(syc_idEvTest);
        break;
#endif
    
#if PRF_ENA_TC_PRF_KOF_WRITE_OS_DATA == 1
    case prf_kof_writeOsData:
        *(uint32_t*)prf_cmdFailure.address = prf_cmdFailure.value;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_WRITE_OTHER_PROC_DATA == 1
    case prf_kof_writeOtherProcData:
        *(uint32_t*)prf_cmdFailure.address = prf_cmdFailure.value;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_WRITE_ROM == 1
    case prf_kof_writeROM:
        *(uint32_t*)prf_cmdFailure.address = prf_cmdFailure.value;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_WRITE_PERIPHERAL == 1
    case prf_kof_writePeripheral:
        *(uint32_t*)prf_cmdFailure.address = prf_cmdFailure.value;
        break;
#endif


#if PRF_ENA_TC_PRF_KOF_READ_PERIPHERAL == 1
    case prf_kof_readPeripheral:
        prf_u32ReadDummy = *(uint32_t*)prf_cmdFailure.address;
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_INFINITE_LOOP == 1
    case prf_kof_infiniteLoop:
        while(true)
            ;
#endif
            
#if PRF_ENA_TC_PRF_KOF_MISALIGNED_WRITE == 1
    case prf_kof_misalignedWrite:
        {
            uint32_t regAry[4+1];
            asm volatile ( /* AssemblerTemplate */
                           "e_stmw  %%r28, 0(%0) /* This should be alright */\n\t"
                           "e_stmw  %%r28, 2(%0) /* This is misaligned */\n\t"
                         : /* OutputOperands */
                         : /* InputOperands */ "r" (&regAry[0])
                         : /* Clobbers */ "memory"
                         );
        }
        break;
#endif
        
#if PRF_ENA_TC_PRF_KOF_MISALIGNED_READ == 1
    case prf_kof_misalignedRead:
        {
            uint32_t regAry[4+1] = {0, 1, 2, 3, 4};
            asm volatile ( /* AssemblerTemplate */
                           "e_lmw   %%r28, 0(%0) /* This should be alright */\n\t"
                           "e_lmw   %%r28, 2(%0) /* This is misaligned */\n\t"
                         : /* OutputOperands */
                         : /* InputOperands */ "r" (&regAry[0])
                         : /* Clobbers */ "r28", "r29", "r30", "r31", "memory"
                         );
        }
        break;
#endif
        
#if PRF_ENA_TC_PRF_KOF_STACK_OVERFLOW == 1
    case prf_kof_stackOverflow:
        {
            /* If we get here, the error should already have happened but code execution
               should be still alright if we didn't exceed the stack to such an extend that
               we tried to get into a neighboured processes area. It's unclear when it'll
               take effect. */
#ifdef DEBUG
            extern uint8_t ld_stackStartP1[0];
            uint8_t dummy;
            assert(&dummy < ld_stackStartP1);
#endif
        }
        break;
#endif
        
#if PRF_ENA_TC_PRF_KOF_STACK_CLEAR_BOTTOM == 1
    case prf_kof_stackClearBottom:
        {
#ifdef DEBUG
            uint32_t varAtTopOfStack;
#endif
            extern uint8_t ld_stackEndP2[0];
            /* Due to the recursion prior to entry into this function we should have room
               enough to only write to a region above the current stack frame. */
            assert((uintptr_t)&varAtTopOfStack + 100u + 100u < (uintptr_t)&ld_stackEndP2[0]);
            memset(ld_stackEndP2-100u, 0x40, 100u);
        }
        break;
#endif
        
#if PRF_ENA_TC_PRF_KOF_SP_CORRUPT == 1
    case prf_kof_spCorrupt:
        asm volatile ("e_la  %%r1, 28(%%r1) /* Just change the stack pointer a bit */\n\t"
                     : /* OutputOperands */
                     : /* InputOperands */
                     : /* Clobbers */ "memory"
                     );
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_SP_CORRUPT_AND_WAIT == 1
    case prf_kof_spCorruptAndWait:
        {
            asm volatile ("\t.extern ld_stackEndP3\n\t"
                          "e_lis %%r1, ld_stackEndP3@ha /* Try to continue with sp from */\n\t"
                          "e_la  %%r1, ld_stackEndP3@l(%%r1) /* supervisor process */\n\t"
                          "se_b  .  /* and wait for next preemption */\n\t"
                         : /* OutputOperands */
                         : /* InputOperands */
                         : /* Clobbers */ "memory"
                         );
        }            
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_PRIVILEGED_AND_MPU == 1
    case prf_kof_privilegedAndMPU:
        _pFctPrivilegedInstrInOSRAM();
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_READ_SPR == 1
    case prf_kof_readSPR:
        asm volatile ("mfspr  %%r0, 1015 /* SPR 1015: MMUCFG */\n\t"
                     ::: /* Clobbers */ "r0"
                     );
        break;
#endif
#if PRF_ENA_TC_PRF_KOF_WRITE_SPR == 1
    case prf_kof_writeSPR:
        asm volatile ("se_li %%r0, 15\n\t"
                      "mtspr 688, %%r0 /* SPR 688: TLB0CFG */\n\t"
                     ::: /* Clobbers */ "r0"
                     );
        break;
#endif
#if PRF_ENA_TC_PRF_KOF_WRITE_SVSP == 1
    case prf_kof_writeSVSP:
        asm volatile ("se_li %%r0, 0\n\t"
                      "mtspr 272, %%r0 /* SPR 272: SPRG0, holding frozen SV sp */\n\t"
                     ::: /* Clobbers */ "r0"
                     );
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_CLEAR_SDA_PTRS == 1
    case prf_kof_clearSDAPtrs:
        asm volatile ("se_li    %%r2, 0\n\t"
                      "neg      %%r13, %%r13\n\t"
                     ::: /* Clobbers */ "r2", "r13"
                     );
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_CLEAR_SDA_PTRS_AND_WAIT == 1
    case prf_kof_clearSDAPtrsAndWait:
        asm volatile ("se_neg   %%r2\n\t"
                      "sub      %%r13, %%r13, %%r2\n\t"
                      "se_b   .\n\t"
                     ::: /* Clobbers */ "r2", "r13"
                     );
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_MMU_WRITE == 1
    case prf_kof_MMUWrite:
        /* Write to the address space before the RAM. Should lead to an MMU TBL miss
           exception. */
        *(volatile uint32_t*)(0x40000000 - sizeof(uint32_t)) = 0xff00a5a5;
        break;
#endif
    
#if PRF_ENA_TC_PRF_KOF_MMU_READ == 1
    case prf_kof_MMURead:
        {
            /* Read from the address space before the RAM. Should lead to an MMU TBL miss
               exception. */
            volatile uint32_t dummy ATTRIB_UNUSED =
                                        *(volatile uint32_t*)(0x40000000 - sizeof(uint32_t));
        }
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_MMU_EXECUTE == 1
    case prf_kof_MMUExecute:
        /* The jump to an address before the RAM should lead to an MMU TBL miss
           exception before it may/would come to an illegal instruction exception or else
           because of the non existing programm code. */
        ((void (*)(void))(0x40000000u - 2*sizeof(uint32_t)))();
    break;
#endif

#if PRF_ENA_TC_PRF_KOF_MMU_EXECUTE_2 == 1
    case prf_kof_MMUExecute2:
        /* The difference to PRF_ENA_TC_PRF_KOF_MMU_EXECUTE: Here we jump to an adress
           further ahead of RAM such that the 64 Bit instruction read wouldn't touch the
           RAM.
             PRF_ENA_TC_PRF_KOF_MMU_EXECUTE_2 produces a single IVOR #14 exception, while
           PRF_ENA_TC_PRF_KOF_MMU_EXECUTE produces a double exception, first the IVOR #14,
           but immediately preempted by an imprecise, delayed machine check exception
           raised by MPU. */
        ((void (*)(void))(0x40000000u - 200*sizeof(uint32_t)))();
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_TRAP == 1
    case prf_kof_trap:
        asm volatile ("twne %%r2, %%r2 /* Must not be trapped */\n\t"
                      "tweq %%r2, %%r2 /* Should be trapped, IVOR #6 */\n\t"
                     ::: /* Clobbers */ 
                     );
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_TLB_INSTR == 1
    case prf_kof_tlbInstr:
        asm volatile ("tlbivax %%r2, %%r13\n\t"
                     ::: /* Clobbers */ 
                     );
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_SPE_INSTR == 1
    case prf_kof_SPEInstr:
        asm volatile ("brinc  %%r0, %%r2, %%r13 /* This SPE instruction is permitted */\n\t"
                      "efsadd %%r0, %%r2, %%r13 /* Float instr. are permitted */\n\t"
                      "evmra %%r0, %%r2     /* 64 Bit SPE instructions are illegal */\n\t"
                     ::: /* Clobbers */ "r0"
                     );
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_BOOKE_FPU_INSTR == 1
    case prf_kof_BookEFPUInstr:
        /* frsp is recognized and correct encoded by the GNU assembler but not disassembled
           by the P&E debugger in CodeWarrior. Consequently, we don't see an IVOR #7 as
           aimed by this test case but an IVOR #6, for an illegal instruction. */
        asm volatile ("frsp 2, 3\n\t"
                     ::: /* Clobbers */ "cr1"
                     );
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_DCACHE_INSTR == 1
    case prf_kof_dCacheInstr:
        {
            /* The instruction dcbz is basically permitted but we don't have a D-cache. */
            uint32_t dummy;
            asm volatile ("dcbtst  0, %%r0, %0 /* Executed without exception */\n\t"
                          "dcbz    %%r0, %0 /* Fails because of missing cache, IVOR #5 */\n\t"
                         : /* OutputOperands */
                         : /* InputOperands */ "r" (&dummy)
                         : /* Clobbers */ "memory"
                         );
        }
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_UNDEF_SYS_CALL == 1
    case prf_kof_undefSysCall:
        rtos_systemCall(999, 1, 2 ,3, 4, 5, 6, 7, 8);
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_RANDOM_WRITE == 1
    case prf_kof_randomWrite:
        {
            unsigned int u;
            for(u=0; u<500; ++u)
            {
                /* We have three interesting address areas to write to, ROM: 0, 2^20 Byte,
                   RAM: 0x40000000, 2^17 Byte, peripheral: 0xf0000000, 2^28 Byte. We use 2
                   Bit from the random number to select one of the three blocks and use
                   n+1 Bit for the address in the block, where n is the number of
                   physically available bits, so that that we have both, potentially
                   forbidden physically available memory or unequipped memory. */
#if RAND_MAX < 0x7fffffff
# error Number of available random bits too little
#endif
                uint32_t address = random()
                       , area = (address & 0x60000000) >> 29
                       , word = random();
                switch(area)
                {
                case 0:
                    /* ROM */
                    *(volatile uint32_t*)(0x00000000 + (address & 0x1fffff)) = word;
                    break;
                    
                case 1:
                    /* RAM */
                    *(volatile uint32_t*)(0x40000000 + (address & 0x3ffff)) = word;
                    break;
                
                case 2:
                case 3:
                    /* Peripherals */
                    *(volatile uint32_t*)(0xf0000000 + (address & 0x0fffffff)) = word;
                    break;

                default:
                    assert(false);
                }
            } /* End for(Several write attempts, until first exception) */                
        }
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_RANDOM_READ == 1
    case prf_kof_randomRead:
        {
            unsigned int u;
            volatile unsigned int u32Dummy ATTRIB_UNUSED;
            for(u=0; u<500; ++u)
            {
                /* Random address is identical to test case prf_kof_randomWrite. */
                uint32_t address = random()
                       , area = (address & 0x60000000) >> 29;
                switch(area)
                {
                case 0:
                    /* ROM */
                    u32Dummy = *(volatile uint32_t*)(0x00000000 + (address & 0x1fffff));
                    break;
                    
                case 1:
                    /* RAM */
                    u32Dummy = *(volatile uint32_t*)(0x40000000 + (address & 0x3ffff));
                    break;
                
                case 2:
                case 3:
                    /* Peripherals */
                    u32Dummy = *(volatile uint32_t*)(0xf0000000 + (address & 0x0fffffff));
                    break;

                default:
                    assert(false);
                }
            } /* End for(Several read attempts, until first exception) */                
        }
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_RANDOM_JUMP == 1
    case prf_kof_randomJump:
        {
            /* A call of arbitrary code will lead to an exception with high likelihood.
               However, we can hit harmless code, too, and safely return here. The loop is
               not meant a repetition of the test case but aims at letting it virtually
               always produce a failure. */
            unsigned int u;
            for(u=0; u<500; ++u)
            {
                /* Random address is identical to test case prf_kof_randomWrite. */
                uint32_t address = random()
                       , area = (address & 0x60000000) >> 29;
                switch(area)
                {
                case 0:
                case 1:
                    /* ROM */
                    address = 0x00000000 + (address & 0x1fffff);
                    break;
                    
                case 2:
                    /* RAM */
                    address = 0x40000000 + (address & 0x3ffff);
                    break;
                
                case 3:
                    /* Peripherals */
                    address = 0xf0000000 + (address & 0x0fffffff);
                    break;

                default:
                    assert(false);
                }
#ifdef DEBUG
                /* The debugger can't be hindered from stopping at instruction se_illegal
                   (0x0000), which unfortunately appears often as memory pattern at
                   arbitrary addresses. This makes debugging of code containing this test
                   case almost impossible. We cannot avoid it but can reduce the likelihood
                   by rejecting addresses where we already see an se_illegal as first
                   instruction (e.g. zeroized bss sections). Safely catching illegal
                   instructions is anyway captured by another test case. */
                if(*(const uint16_t*)address == 0x0000)
                    continue;
#endif
                ((void (*)(void))address)();

            } /* End for(Several attempts to execute arbitrary code, until first exception) */
        }
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_MPU_EXC_BEFORE_SC == 1
    case prf_kof_mpuExcBeforeSc:
        asm volatile (".extern  prc_processAry\n\t"
                      "e_lis    %%r4, prc_processAry@ha\n\t"
                      "e_la     %%r4, prc_processAry@l(%%r4)\n\t"
                      "se_li    %%r3, 0         /* 0: System call terminate task */\n\t"
                      "se_stw   %%r3, 0(%%r4)   /* Invalid instruction, MPU, IVOR #1 */\n\t"
                      "se_sc\n\t"
                     ::: /* Clobbers */ "r3", "r4"
                     );
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_INVALID_CRIT_SEC == 1
    case prf_kof_invalidCritSec:
# error Test case not yet implemented
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_LEAVE_CRIT_SEC == 1
    case prf_kof_leaveCritSec:
# error Test case not yet implemented
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_INVOKE_RTOS_OS_RUN_TASK == 1
    case prf_kof_invokeRtosOsRunTask:
        {
            /* Try running a task in the reporting process using the generally forbidden OS
               API. */
            const prr_testStatus_t capturedStatus = { .noTestCycles = 0xfffffffeu };
            const prc_userTaskConfig_t userTaskConfig =  
                                    { .taskFct = (prc_taskFct_t)prr_taskReportWatchdogStatus
                                      , .tiTaskMax = 0
                                      , .PID = syc_pidReporting
                                    };
            int32_t result ATTRIB_DBG_ONLY =
                                rtos_OS_runTask( &userTaskConfig
                                               , /* taskParam */ (uint32_t)&capturedStatus
                                               );
            assert(result >= 0);
        }
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_INVOKE_RTOS_RUN_TASK_NO_PERMIT == 1
    case prf_kof_invokeRtosRunTaskWithoutPermission:
        {
            /* Try running a task in the reporting process. */
            if((random() & 0xf) == 0)
            {
                /*  A variant of the test case is first trying to manipulate the permission
                    vector. */
/// @todo _runTask_permissions is basically static. Undo manipulation after test
                extern uint16_t _runTask_permissions;
                _runTask_permissions = 0xffff;
            }
            const prr_testStatus_t capturedStatus = { .noTestCycles = 0xffffffffu };
            const prc_userTaskConfig_t userTaskConfig =  
                                    { .taskFct = (prc_taskFct_t)prr_taskReportWatchdogStatus
                                      , .tiTaskMax = 0
                                      , .PID = syc_pidReporting
                                    };
            int32_t result ATTRIB_DBG_ONLY =
                                rtos_runTask( &userTaskConfig
                                            , /* taskParam */ (uint32_t)&capturedStatus
                                            );
            assert(result >= 0);
        }
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_INVOKE_IVR_SYSTEM_CALL_BAD_ARGUMENT == 1
    case prf_kof_invokeIvrSystemCallBadArgument:
# error Test case not yet implemented
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_SYSTEM_CALL_ALL_ARGUMENTS_OKAY == 1
    case prf_kof_systemCallAllArgumentsOkay:
        {
            const uint8_t u8_4 = 4;
            static int16_t SDATA_PRC_FAIL(i16_cnt) = 0;
# error Test case not yet implemented. Implement special system call first
            int32_t result = rtos_systemCall( PRF_SYSCALL_TEST_ALL_ARGS
                                            , 1u
                                            , -2i
                                            , 3.14f
                                            , u8_4
                                            , 'x'
                                            , &main
                                            , i16_cnt
                                            );
            assert((uint32_t)result == ((uint32_t)~i16_cnt & 0xffffu));
            ++ i16_cnt;
        }
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_WAIT_INSTR == 1
    case prf_kof_waitInstr:
        asm volatile (".global  prf_testCaseWaitInstr\n"
                      "prf_testCaseWaitInstr:\n\t"
                      "wait\n\t"
                     :::
                     );
        break;
#endif

#if PRF_ENA_TC_PRF_KOF_ENA_FPU_EXC == 1
    case prf_kof_enableFpuExceptions:
        {
            /* SPEFSCR is user accessible. User may enable all FPU exceptions! */
            const uint32_t configSPEFSCR = 0x0000003f;
            asm volatile (".global  prf_testCaseEnableFpuExceptions\n"
                          "prf_testCaseEnableFpuExceptions:\n\t"
                          "mtspr    512, %0     /* SPR 512: SPEFSCR */\n\t"
                         : /* OutputOperands */
                         : /* InputOperands */ "r" (configSPEFSCR)
                         : /* Clobbers */
                         );
            
            /* Let's now provoke an exception. */
            volatile float d = 0.0f
                         , x ATTRIB_UNUSED = 1.0f / d;
        }
        break;
#endif

    default:
        assert(false);
    }
} /* End of injectError */



/* The next function needs to be compiled without optimization. The compiler will otherwise
   recognize the useless recursion and kick it out. */
#pragma GCC push_options
#pragma GCC optimize ("O0")
/**
 * Helper function: Calls itself a number of times in order to operate the fault injection
 * on different stack nesting levels. Then it branches into error injection.
 */
static void nestStackInjectError(unsigned int remainingLevels)
{
    if(remainingLevels > 0)
        nestStackInjectError(remainingLevels-1);
    else
        injectError();
        
} /* End of prf_nestStackInjectError */
#pragma GCC pop_options


/**
 * Task function, cyclically activated every 17ms.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process.
 *   @param PID
 * A user task function gets the process ID as first argument.
 */
int32_t prf_taskInjectError(uint32_t PID ATTRIB_UNUSED)
{
    nestStackInjectError(/* remainingLevels */ prf_cmdFailure.noRecursionsBeforeFailure);
    return 0;
    
} /* End of prf_taskInjectError */



/**
 * Task function, cyclically activated every 17ms. The task belongs to process \a
 * syc_pidFailingTasks. In this process it has the lowest priority.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process.
 *   @param PID
 * A user task function gets the process ID as first argument.
 */
int32_t prf_task17ms(uint32_t PID ATTRIB_UNUSED)
{
    /* We stay for a while here in this routine to enlarge the chance of becoming
       interrupted by the failure injecting task. Which means that this task's execution
       can be harmed by the injected errors, too. That should be caught, too. */
    del_delayMicroseconds(/* tiCpuInUs */ 1700);

    return 0;
    
} /* End of prf_task17ms */



/**
 * Task function, directly started from a regular timer ISR (PIT1). The task belongs to
 * process \a syc_pidFailingTasks. In this process it has the highest priority.
 *   @return
 * If the task function returns a negative value then the task execution is counted as
 * error in the process.
 *   @param PID
 * A user task function gets the process ID as first argument.
 *   @param taskParam
 * Different to "normal" RTOS scheduled user tasks may directly started tasks have a task
 * parameter. In this test we just apply it for a consistency check.
 *   
 */
int32_t prf_task1ms(uint32_t PID ATTRIB_UNUSED, uint32_t taskParam)
{
    static uint32_t SDATA_PRC_FAIL(cnt_) = 0;
    
    ++ _cntTask1ms;
    
    /* Normally, taskParam (counts of starts of this task) and local counter will always
       match. But since this task belongs to the failing process there are potential
       crashes of this task, too, and we can see a mismatch. We report them as task error
       and they will be counted as further process errors. */
    if(taskParam != cnt_++)
    {
        cnt_ = taskParam+1;
        return -1;
    }
    else
        return 0;
    
} /* End of prf_task1ms */



