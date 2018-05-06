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
                               size 16 Byte. Caution, this structure is shared with the C
                               code. */
#define O_RET_RC    (O_RET+0)   /* Temporary storage of return value from system call */
#define O_RET_pSCSD (O_RET+4)   /* Temporary storage of Pointer to context save data of
                                   suspended context */
#define O_RET_pRCSD (O_RET+8)   /* Temporary storage of Pointer to context save data of
                                   resumed context */
#define O_RET_pFct (O_RET+12)   /* Pointer to entry function of newly created context */

/* Define the offsets of words saved in the stack frame. After creation of the stack frame,
   the words are addressed by O_I_xxx(sp).
     Here for the stack frame of the IVOR #4 External Interrupt handler. */
#define O_I_CR    32        /* volatile, saved on ISR entry */
#define O_I_LR    36        /* volatile, saved on ISR entry */
#define O_I_CTR   40        /* volatile, saved on ISR entry */
#define O_I_XER   44        /* volatile, saved on ISR entry */

#define O_I_R00   48        /* volatile, saved on ISR entry */
                            /* r2: Same, constant value for all contexts. Not saved */
#define O_I_R03   52        /* volatile, saved on ISR entry */
#define O_I_R04   56        /* volatile, saved on ISR entry */
#define O_I_R05   60        /* volatile, saved on ISR entry */
#define O_I_R06   64        /* volatile, saved on ISR entry */
#define O_I_R07   68        /* volatile, saved on ISR entry */
#define O_I_R08   72        /* volatile, saved on ISR entry */
#define O_I_R09   76        /* volatile, saved on ISR entry */
#define O_I_R10   80        /* volatile, saved on ISR entry */
#define O_I_R11   84        /* volatile, saved on ISR entry */
#define O_I_R12   88        /* volatile, saved on ISR entry */
#define O_I_R13   92        /* r13: Same, constant value for all contexts, but still saved on
                               context switch, because of optimization only. */
#define O_I_R14   96        /* volatile, saved on context switch */
#define O_I_R15   100       /* volatile, saved on context switch */
#define O_I_R16   104       /* volatile, saved on context switch */
#define O_I_R17   108       /* volatile, saved on context switch */
#define O_I_R18   112       /* volatile, saved on context switch */
#define O_I_R19   116       /* volatile, saved on context switch */
#define O_I_R20   120       /* volatile, saved on context switch */
#define O_I_R21   124       /* volatile, saved on context switch */
#define O_I_R22   128       /* volatile, saved on context switch */
#define O_I_R23   132       /* volatile, saved on context switch */
#define O_I_R24   136       /* volatile, saved on context switch */
#define O_I_R25   140       /* volatile, saved on context switch */
#define O_I_R26   144       /* volatile, saved on context switch */
#define O_I_R27   148       /* volatile, saved on context switch */
#define O_I_R28   152       /* volatile, saved on context switch */
#define O_I_R29   156       /* volatile, saved on context switch */
#define O_I_R30   160       /* volatile, saved on context switch */
#define O_I_R31   164       /* volatile, saved on context switch */

#define O_I_CPR   168       /* Current interrupt priority in suspended context */

#define S_I_StFr  176       /* No content bytes rounded to next multiple of eight */


/* Define the offsets of words saved in the stack frame. After creation of the stack frame,
   the words are addressed by O_SC_xxx(sp).
     Here for the stack frame of the IVOR #8 system call handler. */
#define O_SC_R14   32       /* volatile, saved on context switch */
#define O_SC_R15   36       /* volatile, saved on context switch */
#define O_SC_R16   40       /* volatile, saved on context switch */
#define O_SC_R17   44       /* volatile, saved on context switch */
#define O_SC_R18   48       /* volatile, saved on context switch */
#define O_SC_R19   52       /* volatile, saved on context switch */
#define O_SC_R20   56       /* volatile, saved on context switch */
#define O_SC_R21   60       /* volatile, saved on context switch */
#define O_SC_R22   64       /* volatile, saved on context switch */
#define O_SC_R23   68       /* volatile, saved on context switch */
#define O_SC_R24   72       /* volatile, saved on context switch */
#define O_SC_R25   76       /* volatile, saved on context switch */
#define O_SC_R26   80       /* volatile, saved on context switch */
#define O_SC_R27   84       /* volatile, saved on context switch */
#define O_SC_R28   88       /* volatile, saved on context switch */
#define O_SC_R29   92       /* volatile, saved on context switch */
#define O_SC_R30   96       /* volatile, saved on context switch */
#define O_SC_R31   100      /* volatile, saved on context switch */

#define S_SC_StFr  104      /* No content bytes rounded to next multiple of eight */

/* Define the offsets of words saved in the stack frame. After creation of the stack frame,
   the words are addressed by O_I_xxx(sp).
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
