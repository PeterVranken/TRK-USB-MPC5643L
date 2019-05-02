/**
 * @file ihw_initMcuCoreHW.c
 * Collection of required HW initialization routines. The routines relate to the basic
 * operation of the MCU, which requires a minimum of configuration, e.g. clock settings.
 * Moreover, the interrupt controller is enabled here.\n
 *   Note, the MMU configuration belongs to the set of configurations required for basic
 * MCU operation, too, but this can't be offered here. Without MMU configuration, we could
 * not reach or execute the code offered in this module.
 *
 * Copyright (C) 2017-2018 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   ihw_dummyINTCInterruptHandler
 *   ihw_installINTCInterruptHandler
 *   ihw_initMcuCoreHW
 * Local functions
 *   clearCriticalFaultFlags
 *   clearNonCriticalFaultFlags
 *   initModesAndClks
 *   initPBridge
 *   initINTCInterruptController
 */

/*
 * Include files
 */

#include <stdint.h>
#include <limits.h>
#include <assert.h>

#include "typ_types.h"
#include "MPC5643L.h"
#include "ihw_initMcuCoreHW.h"


/*
 * Defines
 */


/*
 * Local type definitions
 */


/*
 * Local prototypes
 */

/** The empty default interrupt service routine. */
void ihw_dummyINTCInterruptHandler(void);


/*
 * Data definitions
 */

/** The table of function pointers into the actual IRQ handlers is implemented in the
    assembler code (There we have better control of the required alignment constraints.)\n
      Note, the entries in the table are normal, proper C functions; no considerations
    about specific calling conventions (e.g. without stack frame) or according type
    decorations need to be made. */
extern void (*int_INTCInterruptHandlerAry[256])(void);

#if DEBUG
/** If an interrupt is enabled in an I/O device but there's no handler registered at the
    INTC then a dummy handler is installed, which will halt the software in an assertion
    and report the causing interrupt in this global variable.
      @remark This is a development tool only and not compiled in PRODUCTION compilation. */
volatile uint32_t ihw_idxUnregisteredInterrupt = UINT_MAX;
#endif


/*
 * Function implementation
 */

/**
 * Clear critical faults in fault collection and control unit (FCCU).
 *   @remark
 * This code is based on NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file NXP main.c, l.
 * 91ff.
 */
static void clearCriticalFaultFlags()
{
    /** Critical fault key */
#define FCCU_CFK_KEY  0x618B7A50

    uint32_t u;
    for(u=0; u<4; ++u)
    {
        unsigned int cntAttempt = 0;
        while(cntAttempt++ < 100)
        {
            /* Initiate operation clear. */
            FCCU.CFK.R = FCCU_CFK_KEY;
            FCCU.CF_S[u].R = 0xffffffff;

            /* Wait for the completion of the operation - be it successfully or aborted. */
            unsigned int cntTimeout = 10000;
            while(cntTimeout-- > 0  &&  (FCCU.CTRL.B.OPS & 0x2) == 0)
            {}

            /* The MCU reference manual (22.6.8, p. 518f) suggests to read the cleared
               register back and test the bits. In case of failure one should repeat the
               sequence. */
            if(FCCU.CF_S[u].R == 0)
                break;
        }
    } /* for(All critical fault bit registers) */
#undef FCCU_CFK_KEY
} /* End of clearCriticalFaultFlags */



/**
 * Clear non critical faults in fault collection and control unit (FCCU).
 *   @remark
 * This code is based on NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file NXP main.c, l.
 * 105ff.
 */
static void clearNonCriticalFaultFlags()
{
    /** Non-critical fault key */
#define FCCU_NCFK_KEY 0xAB3498FE

    uint32_t u;
    for(u=0; u<4; ++u)
    {
        unsigned int cntAttempt = 0;
        while(cntAttempt++ < 100)
        {
            /* Initiate operation clear. */
            FCCU.NCFK.R = FCCU_NCFK_KEY;
            FCCU.NCF_S[u].R = 0xffffffff;

            /* Wait for the completion of the operation - be it successfully or aborted. */
            unsigned int cntTimeout = 10000;
            while(cntTimeout-- > 0  &&  (FCCU.CTRL.B.OPS & 0x2) == 0)
            {}

            /* The MCU reference manual (22.6.10, p. 520) suggests to read the cleared
               register back and test the bits. In case of failure one should repeat the
               sequence. */
            if(FCCU.NCF_S[u].R == 0)
                break;
        }
    } /* for(All non critical fault bit registers) */
#undef FCCU_NCFK_KEY
} /* clearNonCriticalFaultFlags */


/**
 * Configure the clocks of the MCU. After reset and until here it the internal RC
 * oscillator is used at low clock rate. We configure the device to run the CPU and its
 * peripherals at the maximum clock rate of 120 MHz.
 *   @param enableClkOutputAtPB6
 * The system clock rate, as used by CPU and peripherals, can be connected to an external
 * CPU output. Set this to \a true to make the clock signal be measurable at port PB6, MCU
 * pin 136.
 *   @remark
 * Flash configuration needs to be done prior to this function in order to let the flash
 * support the higher clock rates (e.g. wait state configuration).
 *   @remark
 * This code is based on NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file NXP main.c,
 * l. 144 ff.
 */
static void initModesAndClks(bool enableClkOutputAtPB6)
{
    /* Enable modes DRUN, RUN0, SAFE, RESET */
    ME.MER.R = 0x0000001D;

    CGM.OSC_CTL.R = 0x00800001;
    ME.DRUN.B.XOSCON = 1;

    /* Enter the drun mode, to update the configuration. */
    ME.MCTL.R = 0x30005AF0; /* Mode & Key */
    ME.MCTL.R = 0x3000A50F; /* Mode & Key inverted */

    /* Wait for mode entry to complete. */
    while(ME.GS.B.S_XOSC == 0)
    {}

    /* Wait for mode transition to complete. */
    while(ME.GS.B.S_MTRANS == 1)
    {}

    /* Check DRUN mode has been entered. */
    while(ME.GS.B.S_CURRENT_MODE != 3)
    {}

    /* Select Xosc as PLL source clock */
    CGM.AC3SC.R = 0x01000000; /* PLL0, system PLL */
    CGM.AC4SC.R = 0x01000000; /* PLL1, secondary PLL */

    /* Initialize PLL before turning it on (see MCU ref. manual, 27, p. 901ff):
       fsys = fcrystal*ndiv/idf/odf
       fvco = fcrystal/idf*ndiv
       fvco must be from 256 MHz to 512 MHz
       If we want fsys = 120 MHz: fvco = fsys*odf = 120 MHz * 4 = 480 MHz
       fsys =  40*72/6/4 = 120 MHz
       If we want fsys = 80 MHz: fvco = fsys*odf = 80 MHz * 4 = 320 MHz
       fsys =  40*64/8/4 = 80 MHz */

    /* PLL 0 runs at 120 MHz. */
    CGM.FMPLL[0].CR.B.IDF = 0x5;    /* FMPLL0 IDF=5 --> divide by 5+1=6 */
    CGM.FMPLL[0].CR.B.ODF = 0x1;    /* FMPLL0 ODF=1 --> divide by 2^(1+1)=4 */
    CGM.FMPLL[0].CR.B.NDIV = 72;    /* FMPLL0 NDIV=72 --> divide by 72 */
    CGM.FMPLL[0].CR.B.EN_PLL_SW = 1;/* Enable progressive clock switching for PLL 0 */

    /* We do not make use of the modulation capabilities of the PLLs and can thus use the
       same PLL for both CPU and peripherals. */
#if 0
    /* PLL 1 runs at 80 MHz */
    CGM.FMPLL[1].CR.B.IDF = 0x7;    /* FMPLL0 IDF=7 --> divide by 7+1=8 */
    CGM.FMPLL[1].CR.B.ODF = 0x1;    /* FMPLL0 ODF=1 --> divide by 2^(1+1)=4*/
    CGM.FMPLL[1].CR.B.NDIV = 64;    /* FMPLL0 NDIV=64 --> divide by 64 */
    CGM.FMPLL[1].CR.B.EN_PLL_SW = 1;/* Enable progressive clock switching for PLL 1*/
#endif

    ME.RUNPC[0].R = 0x000000FE;     /* Enable peripherals run in all modes. */
    ME.LPPC[0].R = 0x00000000;      /* Disable peripherals run in LP modes. */

    /* Mode Transition to enter RUN0 mode: */
#if 0
    ME.RUN[0].R = 0x001F00F4;       /* RUN0 cfg: 16MHzIRCON,OSC0ON,PLL0ON,PLL1ON,syclk=PLL0 */
#else
    ME.RUN[0].R = 0x001F0074;       /* RUN0 cfg: 16MHzIRCON,OSC0ON,PLL0ON,syclk=PLL0 */
#endif
    ME.MCTL.R = 0x40005AF0;         /* Enter RUN0 Mode & Key */
    ME.MCTL.R = 0x4000A50F;         /* Enter RUN0 Mode & Inverted Key */

    /* Wait for mode transition to complete */
    while(ME.GS.B.S_MTRANS == 1)
    {}
    /* Check RUN0 mode has been entered */
    while(ME.GS.B.S_CURRENT_MODE != 4)
    {}

    /* Configure the connection of the peripheral clock to the system clock. The PLL can be
       chosen and a divider. */
    CGM.AC0SC.R = 0x04000000;   /* Select PLL0 for aux clk 0.  */
    CGM.AC0DC.R = 0x80800000;   /* Enable PLL0 div by 1 as motor control and sine wave
                                   generator clock. See MCU ref. manual 11.3.1.5., p.225f. */
    CGM.AC1SC.R = 0x04000000;   /* Select PLL0 for aux clk 1.  */
    CGM.AC1DC.R = 0x80000000;   /* Enable PLL0 div by 1 as FlexRay clock. See MCU ref.
                                   manual 11.3.1.8., p.227. */
    CGM.AC2SC.R = 0x04000000;   /* Select PLL0 for aux clk 2  */
    CGM.AC2DC.R = 0x80000000;   /* Enable PLL0 div by 1 as FlexCAN clock. See MCU ref.
                                   manual 11.3.1.10., p.228f. */

    /* Enable CLKOUT on PB6. */
    if(enableClkOutputAtPB6)
    {
        SIU.PCR[22].R = 0x0600;     /* ALT1 - PCR[22] - PA = 0b01 */

        /* set CLKOUT divider of 4 */
        CGM.OCDSSC.B.SELDIV = 0x2;  /* Output selected Output Clock divided by 4. */
        CGM.OCDSSC.B.SELCTL = 0x2;  /* System PLL. */
        CGM.OCEN.B.EN = 1;          /* Enable CLKOUT signal. */
    }
    else
        CGM.OCEN.B.EN = 0;          /* Disable CLKOUT signal. */

} /* End of initModesAndClks */




/**
 * Basic configuration of the peripheral bridge. A general purpose setting is chosen,
 * suitable for all of the samples in this project: All masters can access the peripherals
 * without access protection for any of them.
 *   @remark
 * A true application would tend to do the peripheral bridge configuration much more
 * restrictive!
 */
static void initPBridge()
{
    /* Peripheral bridge is completely open; all masters can go through AIPS and the
       peripherals have no protection. */
    AIPS.MPROT0_7.R   = 0x77777777;
    AIPS.MPROT8_15.R  = 0x77000000;
    AIPS.PACR0_7.R    = 0x0;
    AIPS.PACR8_15.R   = 0x0;
    AIPS.PACR16_23.R  = 0x0;

    AIPS.OPACR0_7.R   = 0x0;
    AIPS.OPACR16_23.R = 0x0;
    AIPS.OPACR24_31.R = 0x0;
    AIPS.OPACR32_39.R = 0x0;
    AIPS.OPACR40_47.R = 0x0;
    AIPS.OPACR48_55.R = 0x0;
    AIPS.OPACR56_63.R = 0x0;
    AIPS.OPACR64_71.R = 0x0;
    AIPS.OPACR80_87.R = 0x0;
    AIPS.OPACR88_95.R = 0x0;

} /* End of initPBridge */



/**
 * Dummy interrupt handler. On initialization of the INTC
 * (initINTCInterruptController()), this function is put into all 256 interrupt vectors
 * in the table.\n
 *   The dummy handler can't reasonably service the interrupt. It would need to know the
 * source of interrupt to acknowledge the interrupt there (mostly the interrupt bit in the
 * status word of an I/O device needs to be cleared). Without doing this acknowledge, the
 * same interrupt would be served immediately again after return from the handler. This is
 * effectively an infinite loop. Better to report this as a problem - in DEBUG compilation
 * an assertion fires. In PRODUCTION compilation it does nothing and returns but the
 * initialization has given it a priority that will make the interrupt never be served at
 * all.\n
 *   To implement a real service, you would replace the default handler by your service
 * implementation using ihw_installINTCInterruptHandler().
 */
void ihw_dummyINTCInterruptHandler(void)
{
    /* If this assertion fired then you enabled an interrupt on hardware level (I/O device
       configuration) but you didn't use ihw_installINTCInterruptHandler() in your code to
       install an adequate service handler for it.
         You can find the address of the interrupt vector in register INTC_IACKR_PRC0, or
       0xfff48010. Subtract the address of the SW vector table int_INTCInterruptHandlerAry
       (see application map file) and devide by word size 4; this yields the interrupt
       index, which can be resolved to the interrupt source with help of the MCU reference
       manual, section 28.7, table 28-4. */
#if DEBUG
    /* We put the causing interrupt into a global debug variable for convenience. */
    ihw_idxUnregisteredInterrupt =
                         (INTC.IACKR_PRC0.R - (uint32_t)&int_INTCInterruptHandlerAry[0]) / 4;
    assert(false);
#endif
} /* End of ihw_dummyINTCInterruptHandler */



/**
 * Initialize the interrupt controller INTC. The interrupt table with all user code
 * handlers is initialized o contain a dummy handler for all interrupts and it is then
 * registered at the hardware device INTC for use.\n
 *   The interrupt default handler is ihw_dummyINTCInterruptHandler(). It does nothing in
 * PRODUCTION compilation but an assertion will fire in DEBUG compilation in order to
 * indicate the missing true handler for an enabled interrupt.
 *   Note, this function locally sets but does not touch the enable external interrupts bit
 * in the machine status register. You will call it normally at system startup time, when
 * all interrupts are still disabled, then call ihw_installINTCInterruptHandler()
 * repeatedly for all interrupts your code is interested in and eventually enable the
 * interrupt processing at the CPU.
 */
static void initINTCInterruptController() 
{
    /* Prepare the vector table with all interrupts being served by our problem reporting
       dummy handler. */
    unsigned int u;
    for(u=0; u<sizeOfAry(int_INTCInterruptHandlerAry); ++u)
    {
        /* Note, in DEBUG compilation we configure the dummy handler with a priority that
           will make it used; the reason is that the dummy handler - although it can't
           really do the job of interrupt servicing - can report the problem of a bad
           interrupt configuration in the user code. (It's assumed that debugger is
           available during development time.) In PRODUCTION compilation, the dummy handler
           will never serve because of the priority being zero. */
        ihw_installINTCInterruptHandler( ihw_dummyINTCInterruptHandler
                                       , /* vectorNum */ u
#ifdef DEBUG
                                       , /* psrPriority */ 1
#else
                                       , /* psrPriority */ 0
#endif
                                       , /* isPreemptable */ false
                                       );
    } /* for(All entries in the INTC vector table) */

    /* Normally, this function should always be called at the very first beginning, when
       all interrupts are still globally disabled at the CPU. However, we make it safe
       against deviating code constructs if we locally disable all interrupts. */
    uint32_t msr = ihw_enterCriticalSection();

    /* Block Configuration register, INTC_BCR0
       VTES_PRC0, 0x20: 0 for 4 Byte entries, 1 for 8 Byte entries
       HVEN_PRC0, 0x1, 0: SW vector, 1: HW vector mode */
    INTC.BCR.R = 0;

    /* The address of our vector table is stored in field VTBA_PRC0. Only the most
       significant 21 Bit will matter, the reset will at run-time be replaced by the index
       of the pending interrupt. */
    INTC.IACKR_PRC0.R = (uint32_t)&int_INTCInterruptHandlerAry[0];

    /* The current priority is set to 0. */
    INTC.CPR_PRC0.B.PRI = 0;

    /* Restore the machine status register including the enable external interrupt bit.
       For the normal, intended use case this won't have an effect. */
    ihw_leaveCriticalSection(msr);
    
} /* End of initINTCInterruptController */




/**
 * This function can be used to install an interrupt handler for a given interrupt vector.
 * It will also set the Priority Select Register for the source to the one given.
 *   @param interruptHandler
 * The interrupt handler. An ordinary C function in the user code area, which is called
 * when the according interrupt is served.
 *   @param vectorNum
 * All possible external interrupt sources are hardwired to the interrupt controller. They
 * are indentified by index. The table, which interrupt source (mostly I/O device) is
 * connected to the controller at which index can be found in the MCU reference manual,
 * section 28.7, table 28-4.
 *   @param psrPriority
 * The priority at which the interrupt is served. 0..15. 0 is useless, it would never be
 * served, 1 is the lowest real priority and 15 the highest. Preemption of a handler (if
 * enabled), which serves an interrupt of priority n will be possible only by another
 * interrupt of priority n+1 or higher.
 *   @param isPreemptable
 * For each interrupt it can be sayed, whether it is preemptable by other interrupts of
 * higher priority or not. If this is \a false then the interrupt handler will always be
 * entered with the status bit EE reset in the machine status register MSR.\n
 *   Note, a handler, which has been declared non-premptable is allowed to set the EE bit
 * itself. It can thus first do some operations without any race-conditions with other
 * interrupts and then continue without further locking normal interrupt processing.
 *   @remark
 * The function can be used at any time. It is possible to exchange a handler at run-time,
 * while interrrupts are being processed. However, the normal use case will rather be to
 * call this function for all required interrupts and only then call the other function
 * ihw_resumeAllInterrupts().\n
 *   This function must not be called for an interrupt number n from the context of that
 * interrupt n.
 *   @remark
 * This code is based on NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file
 * IntcInterrupts_p0.c, l. 204ff.
 */
void ihw_installINTCInterruptHandler( void (*interruptHandler)(void)
                                    , unsigned short vectorNum
                                    , unsigned char psrPriority
                                    , bool isPreemptable
                                    )
{
    /* We permit to use this function at any time, i.e. even while interrupts may occur. We
       need to disable them shortly to avoid inconsistent states (vector and priority). */
    uint32_t msr = ihw_enterCriticalSection();

    /* Set the function pointer in the ISR Handler table. We use the uppermost bit of the
       address to store the preemption information. This convention is known and considered
       by the assembler code that implements the common part of all INTC interrupts. */
    assert(((uintptr_t)interruptHandler & 0x80000000) == 0);
    if(isPreemptable)
    {
        int_INTCInterruptHandlerAry[vectorNum] =
                            (void (*)(void))((uintptr_t)interruptHandler | 0x80000000);
    }
    else
        int_INTCInterruptHandlerAry[vectorNum] = interruptHandler;

    /* Set the PSR Priority */
    INTC.PSR[vectorNum].B.PRI = psrPriority;

    ihw_leaveCriticalSection(msr);
    
} /* End of ihw_installINTCInterruptHandler */



/**
 * Initialize the MCU core hardware, such that it can be safely operated. This relates
 * mainly to the setup of the clocks and PLLs.\n
 *   Additionally, the INTC is configured to serve all the external interrupts in software
 * vector mode. However, before using an interrupt, you will still have to register your
 * handlers, see ihw_installINTCInterruptHandler().
 *   @remark
 * After return the MCU core is fully operational. Further HW initialization can be done in
 * the user code by implementing dedicated drivers. These will configure the I/O devices,
 * enable their interrupt and register the handler using
 * ihw_installINTCInterruptHandler().\n
 *   After having done this for all required devices the user code will call
 * ihw_enableAllInterrupts() to start full MCU operation.
 *   @remark
 * This code is based on NXP sample MPC5643L-LINFlex-UART-DMA-CW210, file main.c, l. 115ff.
 */
void ihw_initMcuCoreHW(void)
{
    /* Check you have cleared all the faults RGM prior to moving from SAFE/DRUN Modes. */
    if(RGM.FES.B.F_FCCU_SAFE || RGM.FES.B.F_FCCU_HARD)
    {
        RGM.FES.R;
        ME.IMTS.R = 0x00000001;
        clearCriticalFaultFlags();
        clearNonCriticalFaultFlags();
        RGM.FES.R = 0xFFFF;
        RGM.DES.R = 0xFFFF;
    }

    /* Initialize the clocks.
         Clock signal output is useless, the CPU pin BP6 is not connected on our eval
       board. */
    initModesAndClks(/* enableClkOutputAtPB6 */ false);

    /* Grant access to the bus masters to the peripherals, particularly CPU and DMA. */
    initPBridge();
    
    /* Initialize the interrupt controller for the external interrupts. No interrupts are
       enabled but the mechansim is in place to register handlers from the user code. */
    initINTCInterruptController();

    /* From here on the MCU is fully operational. Further HW initialization can be done in
       dedicated driver implementations according to the applictaion needs. */

} /* End of ihw_initMcuCoreHW */



