#ifndef INT_DEFSTACKFRAME_INCLUDED
#define INT_DEFSTACKFRAME_INCLUDED
/**
 * @file int_defStackFrame.h
 * Definition of stack frame build up. This file is shared between C and assembly code.
 *
 * Copyright (C) 2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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


/*
 * Defines
 */

/* Define the offsets of words saved in the stack frame. After creation of the stack frame,
   the words are addressed by O_xxx(sp).
     Here for the common part of the stack frame of IVOR #4 and IVOR #8 handlers. */
#define O_R01      0        /* SP is stored at bottom of stack frame */
                            /* Offset 4 must not be used, will be written by sub-routine of
                               interrupt handler. */
#define O_SRR0     8        /* Address of instruction after end of preemption */
#define O_SRR1    12        /* Machine state after end of preemption */

#define O_RET     16        /* Return value of sys call handler (temporary storage) has
                               size 12 Byte. Caution, this structure is shared with the C
                               code. */
#define O_RET_RC    (O_RET+0)   /* Temporary storage of return value from system call */
#define O_RET_pSCSD (O_RET+4)   /* Temporary storage of Pointer to context save data of
                                   suspended context */
#define O_RET_pRCSD (O_RET+8)   /* Temporary storage of Pointer to context save data of
                                   resumed context */

/* Define the offsets of words saved in the stack frame. After creation of the stack frame,
   the words are addressed by O_I_xxx(sp).
     Here for the stack frame of the IVOR #4 External Interrupt handler. */
#define O_I_CR    28        /* volatile, saved on ISR entry */
#define O_I_LR    32        /* volatile, saved on ISR entry */
#define O_I_CTR   36        /* volatile, saved on ISR entry */
#define O_I_XER   40        /* volatile, saved on ISR entry */

#define O_I_R00   44        /* volatile, saved on ISR entry */
                            /* r2: Same, constant value for all contexts. Not saved */
#define O_I_R03   48        /* volatile, saved on ISR entry */
#define O_I_R04   52        /* volatile, saved on ISR entry */
#define O_I_R05   56        /* volatile, saved on ISR entry */
#define O_I_R06   60        /* volatile, saved on ISR entry */
#define O_I_R07   64        /* volatile, saved on ISR entry */
#define O_I_R08   68        /* volatile, saved on ISR entry */
#define O_I_R09   72        /* volatile, saved on ISR entry */
#define O_I_R10   76        /* volatile, saved on ISR entry */
#define O_I_R11   80        /* volatile, saved on ISR entry */
#define O_I_R12   84        /* volatile, saved on ISR entry */
#define O_I_R13   88        /* r13: Same, constant value for all contexts, but still saved on
                               context switch, because of optimization only. */
#define O_I_R14   92        /* volatile, saved on context switch */
#define O_I_R15   96        /* volatile, saved on context switch */
#define O_I_R16   100       /* volatile, saved on context switch */
#define O_I_R17   104       /* volatile, saved on context switch */
#define O_I_R18   108       /* volatile, saved on context switch */
#define O_I_R19   112       /* volatile, saved on context switch */
#define O_I_R20   116       /* volatile, saved on context switch */
#define O_I_R21   120       /* volatile, saved on context switch */
#define O_I_R22   124       /* volatile, saved on context switch */
#define O_I_R23   128       /* volatile, saved on context switch */
#define O_I_R24   132       /* volatile, saved on context switch */
#define O_I_R25   136       /* volatile, saved on context switch */
#define O_I_R26   140       /* volatile, saved on context switch */
#define O_I_R27   144       /* volatile, saved on context switch */
#define O_I_R28   148       /* volatile, saved on context switch */
#define O_I_R29   152       /* volatile, saved on context switch */
#define O_I_R30   156       /* volatile, saved on context switch */
#define O_I_R31   160       /* volatile, saved on context switch */

#define O_I_CPR   164       /* Current interrupt priority in suspended context */

#define S_I_StFr  168       /* No content bytes rounded to next multiple of eight */


/* Define the offsets of words saved in the stack frame. After creation of the stack frame,
   the words are addressed by O_SC_xxx(sp).
     Here for the stack frame of the IVOR #8 system call handler. */
#define O_SC_R14  28        /* volatile, saved on context switch */
#define O_SC_R15  32        /* volatile, saved on context switch */
#define O_SC_R16  36        /* volatile, saved on context switch */
#define O_SC_R17  40        /* volatile, saved on context switch */
#define O_SC_R18  44        /* volatile, saved on context switch */
#define O_SC_R19  48        /* volatile, saved on context switch */
#define O_SC_R20  52        /* volatile, saved on context switch */
#define O_SC_R21  56        /* volatile, saved on context switch */
#define O_SC_R22  60        /* volatile, saved on context switch */
#define O_SC_R23  64        /* volatile, saved on context switch */
#define O_SC_R24  68        /* volatile, saved on context switch */
#define O_SC_R25  72        /* volatile, saved on context switch */
#define O_SC_R26  76        /* volatile, saved on context switch */
#define O_SC_R27  80        /* volatile, saved on context switch */
#define O_SC_R28  84        /* volatile, saved on context switch */
#define O_SC_R29  88        /* volatile, saved on context switch */
#define O_SC_R30  92        /* volatile, saved on context switch */
#define O_SC_R31  96        /* volatile, saved on context switch */

#define S_SC_StFr  104      /* No content bytes rounded to next multiple of eight */


/* Define the offsets of words saved in the stack frame. After creation of the stack frame,
   the words are addressed by O_yyy_xxx(sp).
     Here for the stack frame of function int_simpleSystemCall that implements simple
   system calls. */
#define O_SSC_SRR0 O_SRR0   /* 8, shared with IVOR handler definition */
#define O_SSC_SRR1 O_SRR1   /* 12, shared with IVOR handler definition */

#define S_SSC_StFr 16       /* No content bytes rounded to next multiple of eight */


/* Define the offsets of words saved in the stack frame. After creation of the stack frame,
   the words are addressed by O_yyy_xxx(sp).
     Here for the assembler written context start function ccx_startContext. */
#define O_StCtxt_CTXT_ENTRY 8   /* Adress of entry function into new context */
#define S_StCtxt_StFr  16       /* No content bytes rounded to next multiple of eight */


/*
 * Global type definitions
 */


/*
 * Global data declarations
 */


/*
 * Global prototypes
 */



#endif  /* INT_DEFSTACKFRAME_INCLUDED */
