/**
 * @file lfd_linFlexDriver.c
 * Support of the LINFlex device of the MPC5643L. The device is configured as UART and fed
 * by DMA. We get a serial RS 232 output channel of high throughput with a minimum of CPU
 * interaction.\n
 *   Input is done by interrupt on a received character. The bandwidth of the input channel
 * is by far lower than the output. This is fine for the normal use case, controlling an
 * application by some input commands, but would become a problem if the intention is to
 * download large data chunks, e.g. for a kind of bott loader.\n
 *   The API is a small set of basic read and write routines, which adopt the conventions
 * of the C standard library so that the C functions for formatted output become usable.
 * (Formmated input is not possible through C standrad functions.)
 *
 * Copyright (C) 2017 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   ldf_initSerialInterface
 *   lfd_writeSerial
 * Local functions
 *   initPBridge
 *   initDMA
 *   write
 *   dmaCh15Interrupt
 *   linFlex0RxInterrupt
 *   registerInterrupts
 *   read
 *   initLINFlex
 */

/*
 * Include files
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "lfd_linFlexDriver.h"
#include "ihw_initMcuCoreHW.h"



/*
 * Defines
 */
 
/** The DMA channel to serve the UART with sent data bytes. */
/// @todo Make channel selectable: The implementation doesn't respectthis macro yet
#define DMA_CHN_FOR_SERIAL_OUTPUT   15

/** The interrupt priority for serial output. The interrupt is requested by the DMA when
    all bytes of the last recently initiated transfer are sent.
      @remark The chosen priority needs to be greater or equal than the priority of any
    context, that makes use of the API functions of this module. */
#define INTC_PRIO_IRQ_DMA_FOR_SERIAL_OUTPUT     1

/** The interrupt priority for serial input. The interrupt is requested by the UART when
    another byte has been received.
      @remark The chosen priority needs to be greater or equal than the priority of any
    context, that makes use of the API functions of this module.
      @remark Because of the larger UART buffer applied for serial output, this priority
    should normally be chosen higher than #INTC_PRIO_IRQ_DMA_FOR_SERIAL_OUTPUT. */
#define INTC_PRIO_IRQ_UART_FOR_SERIAL_INPUT     2

/** The size of the ring buffer can be chosen as a power of two of bytes.
      @remark Note, the permitted range of values depends on the reservation of space made
    in the linker control file. */
#define SERIAL_OUTPUT_RING_BUFFER_SIZE_PWR_OF_TWO   10

/** Compute the size of the ring buffer as number of bytes. */
#define SERIAL_OUTPUT_RING_BUFFER_SIZE  (1u<<(SERIAL_OUTPUT_RING_BUFFER_SIZE_PWR_OF_TWO)) 


/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
 
/*
 * Data definitions
 */
 
/** The ring buffer use for the DMA based serial output is provided by the linker control
    file.
      @todo The use of the entire heap area is preliminary only. */
extern uint8_t ld_memHeapStart[0];

/** The ring buffer use for the DMA based serial output is provided by the linker control
    file. This declaration gives access to its maximum size. */
extern uint8_t ld_memHeapSize[0];

/** The ring buffer use for the DMA based serial output is provided by the linker control
    file. We initialize a normal pointer to it. */
static uint8_t * const _serialOutRingBuf = ld_memHeapStart;

/** The maximum number of bytes, which are reserved for the ring buffer. The chosen, actual
    size needs to be no more than this. */
static const unsigned int _sizeOfSerialOutRingBuf = (uintptr_t)ld_memHeapSize;

static volatile unsigned long _cntIrqDmaCh = 0;

static unsigned char *_pWrRingBuf = NULL;
static volatile bool _dmaTransferIsRunning = false;

volatile unsigned long lfd_noRxBytes = 0;
static volatile unsigned char _readBuf[512]
                            , *_pRdBuf = &_readBuf[0];
static const unsigned char *_pRdBufEnd = &_readBuf[0] + sizeof(_readBuf);
static unsigned int _noEOL = 0;

unsigned long lfd_read_noInvocations = 0;
int lfd_read_fildes = -99;
void *lfd_read_buf = (void*)-99;
size_t lfd_read_nbytes = 99;
unsigned int lfd_read_noErrors = 0
           , lfd_read_noLostBytes = 0;
           

 
/*
 * Function implementation
 */

/// @todo Doesn't belong here, is general machine initialization. Can become part of
// startup or main
/**
 * Basic configuration of the peripheral bridge. A general purpose setting is chosen,
 * suitable for all of the samples in this project: All masters can access the peripherals
 * without access protection for any of them.
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
 * Initialize the DMA device. The chosen channel is set up to write the contents of a
 * cyclic buffer of fixed address and size into the UART.
 *   @remark
 * The DMA initialization is mostly related to the DMA channel in use (which can be
 * considered reserved for this purpose in all reasonable environments). However, this
 * function access some DMA registers, too, that affect all channels (e.g. channel
 * arbitration). This function will require changes, when the module is integrated into an
 * environment, where different DMA channels are applied for different, irrelated purposes.
 */
static void initDMA(void)
{
    /* Check preconditions for use of DMA with modulo source addressing. */
    assert(((uint32_t)_serialOutRingBuf & (SERIAL_OUTPUT_RING_BUFFER_SIZE-1)) == 0);
    
    /* Initialize write to ring buffer. */
    _pWrRingBuf = _serialOutRingBuf;
    
    /* Initial load address of source data is the beginning of the ring buffer. */
    EDMA.CHANNEL[15].TCDWORD0_.B.SADDR = (vuint32_t)&_serialOutRingBuf[0];
    /* Read 1 byte per transfer. */
    EDMA.CHANNEL[15].TCDWORD4_.B.SSIZE = 0;
    /* After transfer, add 1 to the source address. */
    EDMA.CHANNEL[15].TCDWORD4_.B.SOFF = 1;
    /* After major loop, do not move the source pointer. Next transfer will read from next
       cyclic address. */
    EDMA.CHANNEL[15].TCDWORD12_.B.SLAST = 0;
    /* Source modulo feature is applied to implement the ring buffer. */
    EDMA.CHANNEL[15].TCDWORD4_.B.SMOD = SERIAL_OUTPUT_RING_BUFFER_SIZE_PWR_OF_TWO;

    /* Load address of destination is fixed. It is the byte input of the UART's FIFO. */
    EDMA.CHANNEL[15].TCDWORD16_.B.DADDR = ((vuint32_t)&LINFLEX_0.BDRL.R) + 3;
    /* Write 1 byte per transfer. */
    EDMA.CHANNEL[15].TCDWORD4_.B.DSIZE = 0;
    /* After transfer, do not alter the destination address. */
    EDMA.CHANNEL[15].TCDWORD20_.B.DOFF = 0;
    /* After major loop, do not alter the destination address. */
    EDMA.CHANNEL[15].TCDWORD24_.B.DLAST_SGA = 0;
    /* Destination modulo feature is not used. */
    EDMA.CHANNEL[15].TCDWORD4_.B.DMOD = 0;

    /* Transfer 1 byte per minor loop */
    EDMA.CHANNEL[15].TCDWORD8_.B.SMLOE = 0;
    EDMA.CHANNEL[15].TCDWORD8_.B.DMLOE = 0;
    EDMA.CHANNEL[15].TCDWORD8_.B.MLOFF = 0;
    EDMA.CHANNEL[15].TCDWORD8_.B.NBYTES = 1;

    /* Initialize the begining and current major loop iteration counts to zero. We will set
       it in the next call of lfd_writeSerial. */
    EDMA.CHANNEL[15].TCDWORD28_.B.BITER = 0;
    EDMA.CHANNEL[15].TCDWORD20_.B.CITER = 0;
    EDMA.CHANNEL[15].TCDWORD20_.B.CITER_LINKCH = 0;

    /* Do a single transfer; don't repeat, don't link to other channels. */
    EDMA.CHANNEL[15].TCDWORD28_.B.D_REQ = 1; /* 1: Do once, 0: Continue by repeating all */
    EDMA.CHANNEL[15].TCDWORD28_.B.INT_HALF = 0;
    EDMA.CHANNEL[15].TCDWORD28_.B.INT_MAJ = 1;
    EDMA.CHANNEL[15].TCDWORD20_.B.CITER_E_LINK = 0;
    EDMA.CHANNEL[15].TCDWORD28_.B.BITER_E_LINK = 0;
    EDMA.CHANNEL[15].TCDWORD28_.B.MAJOR_E_LINK = 0;
    EDMA.CHANNEL[15].TCDWORD28_.B.E_SG = 0;
    EDMA.CHANNEL[15].TCDWORD28_.B.BWC = 3; /* 0: No stalling, 3: Stall for 8 cycles after
                                              each byte; fast enough for serial com. */
    EDMA.CHANNEL[15].TCDWORD28_.B.START = 0;
    EDMA.CHANNEL[15].TCDWORD28_.B.DONE = 0;
    EDMA.CHANNEL[15].TCDWORD28_.B.ACTIVE = 0;

    /* ERCA, 0x4: 1: Round robin for channel arbitration, 0: Priority controlled
       EDBG, 0x2: 1: Halt DMA when entering the debugger.
         Note, this setting affects all channels! */
    EDMA.DMACR.R = 0x00000002;

    /* We use priority controlled channel arbitration. All active channels need to have
       different priorities. The standard configuration is to set the priority to the
       channel number. This is the reset default of the hardware and we are not going to
       change it. The reset default disables preemptability or all channels.
         Note, this configuration affects all channels! */
#if 0
    unsigned int idxChn;
    for(idxChn=0; idxChn<16; ++idxChn)
    {
        /* ECP, 0x80: 1: Channel can be preempted, 0: cannot be preempted
           DPA, 0x40: 0: Channel can preempt, 1: cannot preempt others */
        EDMA.DCHPRI[idxChn].R = idxChn;
    }
#endif

    /* EDMA.DMAERQL.R: Not touched yet, we don't enable the channel yet. This will be done in
       the next use of lfd_writeSerial. */
    
    /* Route LINFlex0 TX DMA request to eDMA channel 15.
         ENBL, 0x80: Enable channel
         SOURCE, 0x3f: Selection of DMAMUX input. The devices are hardwired to the DMAMUX
       and the index of a specific device can be found in table 18-4, MCU ref. manual, p.
       388. Index 22: LINFlexD_0, Tx, index 24: LINFlexD_1, Tx
         @todo Make the LINFlex device selectable 
         @todo Make channel selectable. All config registers are addressed by (volatile
       unsigned char *)(DMAMUX_BASE_ADDR+DMA_CHN_FOR_SERIAL_OUTPUT) */
    DMAMUX.CHCONFIG15.R |= (0x80 + 22);

} /* End of initDMA */




/**
 * Interrupt handler for DMA channel 15.
 *   @remark
 * This interrupt must have a priority higher than any OS schedule relevant interrupt. The
 * application tasks using the serial channel must not become active.
 */
static void dmaCh15Interrupt(void)
{
    
    ++ _cntIrqDmaCh;
    
    /* Interrupt should be raised on transfer done. Reset of the bit by software is however
       not required. */
    assert(EDMA.CHANNEL[15].TCDWORD28_.B.DONE == 1);
    assert((EDMA.DMASERQ.R & 0x8000) == 0);
    assert(_dmaTransferIsRunning);
    
    /* MCU ref manual is ambiguous in how to reset the interrupt request bit: It says both,
       DMAINTL is a normal read modify write register and writing a 1 would reset the
       corresponding bit while writing a 0 has no effect. Tried out: the latter works
       well (and doesn't generate race conditions with other DMA channels like a
       read-modify-write). */
    EDMA.DMAINTL.R = 0x00008000;
    
    /* Check the cyclic address computation feature of the DMA in modulo mode. */
    assert(EDMA.CHANNEL[15].TCDWORD0_.B.SADDR
           >= (uintptr_t)&_serialOutRingBuf[0]
           &&  EDMA.CHANNEL[15].TCDWORD0_.B.SADDR
               < (uintptr_t)&_serialOutRingBuf[0] + SERIAL_OUTPUT_RING_BUFFER_SIZE
          );

    /* We need to retrigger the DMA transfer if the ring buffer has been written meanwhile
       with new data. */
    uint32_t noBytesWrittenMeanwhile = (uintptr_t)_pWrRingBuf
                                       - EDMA.CHANNEL[15].TCDWORD0_.B.SADDR;
    if(noBytesWrittenMeanwhile > SERIAL_OUTPUT_RING_BUFFER_SIZE)
        noBytesWrittenMeanwhile += SERIAL_OUTPUT_RING_BUFFER_SIZE;

    /* Same pointer values is used as empty indication. Therefore it is not possible to
       entirely fill the buffer. Condition "less than" holds. */
    assert(noBytesWrittenMeanwhile < SERIAL_OUTPUT_RING_BUFFER_SIZE);

    if(noBytesWrittenMeanwhile > 0)
    {
            
            /* Set the number of bytes to transfer by DMA to the UART. */
            /// @todo size limitation depending on DMA mode? assert? buffer size?
        EDMA.CHANNEL[15].TCDWORD28_.B.BITER = noBytesWrittenMeanwhile;
        EDMA.CHANNEL[15].TCDWORD20_.B.CITER = noBytesWrittenMeanwhile;
            
        /* Enable the UART to request bytes from the DMA. This initiates a DMA transfer. */
        EDMA.DMASERQ.R = 15; // eDMA request signal for channel 15 is enabled
    }            
    else
        _dmaTransferIsRunning = false;

} /* End of dmaCh15Interrupt */



/**
 * Interrupt handler for UART RX event.
 */
static void linFlex0RxInterrupt(void)
{
    ++ lfd_noRxBytes;
    
    /* Get the received byte and put it into our buffer. */
    const unsigned char c = LINFLEX_0.BDRM.B.DATA4;
    if(_pRdBuf < _pRdBufEnd)
    {
        * _pRdBuf++ = c;
        if(c == '\n')
            ++ _noEOL;
    }
    
    /// @todo Record a buffer overrun bit in case the IRQ handler comes too slow
    
    /* Acknowlege the interrupt and enable the next one at the same time. */
    assert((LINFLEX_0.UARTSR.R & 0x4) != 0);
    LINFLEX_0.UARTSR.R = 0x4;  
    
} /* End of linFlex0RxInterrupt */



/**
 * Our locally implemented interrupt handlers are registered at the operating system for
 * serving the required I/O devices (DMA and LINFlex 0 or 1).
 */
static void registerInterrupts(void)
{
/* Interrupt offsets taken from MCU reference manual, p. 936.
     26: DMA, channel 15. */
#define IDX_DMA_IRQ         (11+DMA_CHN_FOR_SERIAL_OUTPUT)
#define IDX_LINFLEX0_RX_IRQ (79)

    /* Register our IRQ handlers. Priority is chosen low for output DMA since we serve a
       slow data channel, which even has a four Byte queue inside. */
    /// @todo Priorities to be aligned with rest of application, consider intended RTOS
    ihw_installINTCInterruptHandler( &dmaCh15Interrupt
                                   , /* vectorNum */ IDX_DMA_IRQ
                                   , /* psrPriority */ INTC_PRIO_IRQ_DMA_FOR_SERIAL_OUTPUT
                                   , /* isPreemptable */ true
                                   );
    ihw_installINTCInterruptHandler( &linFlex0RxInterrupt
                                   , /* vectorNum */ IDX_LINFLEX0_RX_IRQ
                                   , /* psrPriority */ INTC_PRIO_IRQ_UART_FOR_SERIAL_INPUT
                                   , /* isPreemptable */ true
                                   );
#undef IDX_DMA_IRQ 
#undef IDX_LINFLEX0_RX_IRQ
} /* End of registerInterrupts */



/**
 * Initialization of the MPC5643L's I/O device LINFlex_0. The device is put into UART mode
 * for serial in-/output.
 *   @remark
 * To match the correct Baud rates the code assumes a peripheral clock rate of 120 MHz.
 *   @todo Make the selection of the LINFlex device (0/1) an argument
 */
static void initLINFlex()
{
    /* enter INIT mode */
    LINFLEX_0.LINCR1.R = 0x0081; /* BF=1, SLEEP=0, INIT=1 */
    // MCO, p.981: BF seems to be not relevant
    //LINFLEX_0.LINCR1.R = 0x0001;

    /* wait for the INIT mode */
    while (0x1000 != (LINFLEX_0.LINSR.R & 0xF000))
    {}

    /* SIUL: Configure pads. */
    /* PCR
       SMC: irrelevant, 0x4000
       APC: digital pin use, 0x2000 = 0
       PA: output source select, ALT1=LINFlexD_0, 0xc00=1
       OBE: irrelevant for ALT1, 0x200, better to set =0
       IBE: input buffer, relevance unclear, 0x100=0 (off)/1 (on)
       ODE: Open drain, 0x20=0 (push/pull), 1 means OD
       SRC: Slew rate, 0x4=1 (fastest), 0 means slowest
       WPE: "weak pull-up", meaning unclear, 0x2=0 (off)
       WPS: Pull-up/down, irrelevant 0x1=1 (up)/0 (down)

       TX: PA=1=0x400, OBE=0=0, IBE=0=0, ODE=0=0, SRC=1=0x4, WPE=0=0 => 0x404
       RX: PA=0=0, OBE=0=0, IBE=1=0x100 => 0x100
    */
    SIU.PCR[18].R = 0x0404;     /* Configure pad PB2, TX, for AF1 func: LIN0TX */
    SIU.PCR[19].R = 0x0100;     /* Configure pad PB3 for LIN0RX */

    /* PSMI: Input select. */
    SIU.PSMI31.B.PADSEL = 0;    /* PSMI[31]=0 connects pin B3 with LINFlexD_0 RX. */

    /* configure for UART mode */
    LINFLEX_0.UARTCR.R = 0x0001; /* set the UART bit first to be able to write the other
                                    bits. */

    /* RFBM: RX buffer/FIFO mode, 0x200, 0 means buffer
       TFBM: TX buffer/FIFO mode, 0x200, 0 means buffer
       PCE: Parity enable, 0x4, 0 means off
       WL: Word length, 0x80+0x20, value b01 means data 8 Bit
    */

    /* RX, TX enable, 0x20 and 0x10, respectively, can be set after leaving the init mode.
       RDFLRFC, 0x1c00: (no bytes to receive - 1) in buffer mode or read FIFO fill amount
       RFBM, 0x200: 0 is RX buffer mode, 1 is RX FIFO mode
    */
    /* This is successful single byte reception in buffer mode. */
    LINFLEX_0.UARTCR.R = 0x0133; /* TX FIFO mode, RX buffer mode, 8bit data, no parity, Tx
                                    enabled, UART mode */

    LINFLEX_0.DMATXE.R = 0x00000001; /* enable DMA TX channel */

    /* Configure baudrate. */
    /* assuming 120 MHz peripheral set 1 clock (fsys below)*/
    /* LFDIV = fsys / (16 * desired baudrate)
       LINIBRR = integer part of LFDIV
       LINFBRR = 16 * fractional part of LFDIV (after decimal point)

       for instance:
       LFDIV = 120e6/(16*19200) = 390.625
       LINIBRR = 390
       LINFBRR = 16*0.625 = 10
    */
    
    /* 390:10 19200 bd, 65:2 115200 bd, 58:10 128000 bd, 29:5 256000 bd, 8:2 921600 bd
        19200 bd worked well with terminal.exe and putty
       115200 bd worked well with terminal.exe and putty
       128000 bd showed transmission errors with terminal.exe and putty
       256000 bd failed with terminal.exe and putty
       921600 bd failed with terminal.exe (not tried with putty) */
    LINFLEX_0.LINIBRR.R = 65;
    LINFLEX_0.LINFBRR.R = 2;

    /* LINIER: Interrupt enable. The bits relate to the bits of same name in LINESR (error
       bits), LINSR and UARTSR (both status).
         BOIE: Buffer overrun could be read in handling of DBFIE
         DBFIE: should report FIFO full in reception mode
         DBEIETOIE: Should request new data for TX, UARTSR[TO] needs to be set
         DRIE: Interrupt on byte received, DRF set in UARTSR
         DTIE: Interrupt on byte sent, DTF set in UARTSR
    */
    LINFLEX_0.LINIER.B.DRIE = 1;
    
    /* GCR
       STOP: 0 for 1 or 1 for 2 stop bits
       SR: set 1 to reset counters, buffers and FIFO but keep configuration and operation
    */

    /* Enter NORMAL mode again. */
    /// @todo Why don't we wait as on init?
    LINFLEX_0.LINCR1.R = 0x0080; /* INIT=0 */
    
} /* End of initLINFlex */




/**
 * Initialize the I/O devices for serial output, in particular, these are the LINFlex
 * device plus a DMA channel to serve it.
 *   @param baudRate
 * The Baud rate of in- and output.
 *   @todo Selection of DMA channel and LINFlex device should become an option
 *   @remark
 * This function needs to be called at system initialization phase, when all External
 * Interrupts are still suspended.
 */
/// @todo Support selection of Baud rate
void ldf_initSerialInterface(unsigned int baudRate ATTRIB_UNUSED)
{
    /* Initialize the peripheral bridge to permit DMA accessing the peripherals. */
    initPBridge();

    initLINFlex();

    /* Register the interrupt handler for DMA. */
    registerInterrupts();
    
    /* Initialize DMA and connect it to the UART. An initial hello world string is
       transmitted. */
    initDMA();

    // Don't test here, no interrupts are running yet
    //const char msg[] = "Hello World!\r\n";
    //static volatile int noBytesWritten_;
    //noBytesWritten_ = write(stdout->_file, msg, sizeof(msg)-1);

} /* End of ldf_initSerialInterface */



/** 
 * Pricipal API function for data output. A byte string is sent through the serial
 * interface. Actually, the bytes are queued for sending and the function is non-blocking. 
 *   @return
 * The number of queued bytes is returned. Normally, this is the same value as argument \a
 * noBytes. However, the byte sequence can be longer than the currently available space in
 * the send buffer. (Its size is fixed and no reallocation strategy is implemented.) The
 * message will be truncated if the return value is less than function argument \a noBytes.
 *   @param msg
 * The byte sequence to send. Note, this may be but is not necessarily a C string with zero
 * terminations. Zero bytes can be send, too.
 *   @param noBytes
 * The number of bytes to send.
 */
signed int lfd_writeSerial(const void *msg, size_t noBytes)
{
    signed int noBytesWritten = (int)noBytes;
    const char *pRd = (const char*)msg;
    uint32_t msr = ihw_enterCriticalSection();
    {
        /* The current, i.e. next, transfer address of the DMA is the first (cyclic) address,
           which we must not touch when filling the buffer.
             Note, we read the DMA register only once. We could do this every time in the
           buffer-fill loop and benefit from the parallel operation of the DMA, which may
           release some bytes while our loop is being executed. We don't do so since we
           need to know the available buffer space beforehand in order to implement proper
           truncation of too long messages. */
        const unsigned char * const pRdDma =
                                    (const unsigned char*)EDMA.CHANNEL[15].TCDWORD0_.B.SADDR;
        
        /* Fill ring buffer until all bytes are written or the current read position of the
           DMA is reached. */
        do
        {
            /* Compute next cyclic write address. */
            /// @todo Simplify computation by having the end-of-buffer at hand. */
            unsigned char *pWrRingBufNext = _pWrRingBuf+1;
            if(((uintptr_t)pWrRingBufNext & (SERIAL_OUTPUT_RING_BUFFER_SIZE-1)) == 0)
                pWrRingBufNext -= SERIAL_OUTPUT_RING_BUFFER_SIZE;
            
            if(pWrRingBufNext == pRdDma)
            {
                /* Buffer is currently full, abort data copying. */
                /// @todo In DEBUG compilation, we should count all lost bytes and report them in a global variable
                break;
            }
            
            /* Put next character into the ring buffer. Note, that this is not a C string,
               we permit to transmit all bytes including zeros. */
            *_pWrRingBuf = *pRd;
            -- noBytes;
            _pWrRingBuf = pWrRingBufNext;
            ++ pRd;
        }
        while(noBytes > 0);

        noBytesWritten -= (int)noBytes;
        
        /* Start DMA only if there's no currently running transfer (from a write of
           before). If there is such a running transfer then the next transfer will be
           initiated from its on-complete-interrupt. */
        if(noBytesWritten > 0  &&  !_dmaTransferIsRunning)
        {
            /* Set the number of bytes to transfer by DMA to the UART. */
            assert((unsigned)noBytesWritten <= SERIAL_OUTPUT_RING_BUFFER_SIZE-1);
            EDMA.CHANNEL[15].TCDWORD28_.B.BITER = noBytesWritten;
            EDMA.CHANNEL[15].TCDWORD20_.B.CITER = noBytesWritten;
            
            /* Enable the DMA channel to accept the UART's requests for bytes. This
               initiates a DMA transfer.
                 NOP, 0x80: 1: Ignore write to register (to permit 32 Bit access to more
               than one of these byte registers at a time)
                 SERQ0, 0x40: 0: Address channel with SERQ, 1: Enable all channels
                 SERQ, 0xf: Channel number */
            EDMA.DMASERQ.R = 15;
            
            /* The status, whether we have currently started a transfer or not is shared
               with the on-complete-interrupt. */
            _dmaTransferIsRunning = true;
        }
    }
    ihw_leaveCriticalSection(msr);
    
    return noBytesWritten;
    
} /* End of lfd_writeSerial */




#if 0
/// @todo Let the write API with file ID become part of the printf compilation unit
signed int write(int file, const void *msg, size_t noBytes)
{
    /// @todo Do we need EOL conversion? Likely yes
    /// @todo Write elipsis (...) in case of buffer-full caused truncation. This requires beforehand computation of possible length
    
    if(noBytes == 0  ||  msg == NULL  ||  (file != stdout->_file  &&  file != stderr->_file))
        return 0;
    else
        return lfd_writeSerial(msg, noBytes);
        
} /* End of write */


/// @todo This API belongs into the printf module. We need a raw read char or read line
ssize_t read(int fildes, void *buf, size_t nbytes)
{
    ++ lfd_read_noInvocations;
    lfd_read_fildes = fildes;
    lfd_read_buf = buf;
    lfd_read_nbytes = nbytes;

    ssize_t result;
    uint32_t msr = ihw_enterCriticalSection();
    {
        const unsigned int noCharsReceived = (unsigned int)(_pRdBuf - &_readBuf[0]);
        assert(noCharsReceived <= sizeof(_readBuf));
        
        /* Normally the clib request a maximum of data, so that the error situation of
           having more characters received as requested will normally not happen. */
        if(nbytes < noCharsReceived)
        {
            ++ lfd_read_noErrors;
            lfd_read_noLostBytes += noCharsReceived - nbytes;
        }
        
        if(nbytes > 0  &&  noCharsReceived == 0)
        {
            /* Catch the case, which is not intended by the blocking clib function: We need
               but we don't have any character. We return an error code. If we simply
               return 0 bytes the clib will not come back to ask later for more
               characters.
                 Note, this attempt to make the clib patiently waiting for later input
               doesn't work neither. Its internal buffer handling seems to be corrupted,
               the character strings returned to gets depend on our data but is not the
               same; fragments are repeated, instead of returning nothing.
                 Incorrect work around: We return an EOL, which actually isn't in the
               stream. Some input functions from the clib will return the empty string,
               which is s bit of correct. */
            *(char*)buf = '\n';
            result = 1;
        }
        else if(nbytes == 0)
            result = 0;
        else
        {
            if(nbytes > noCharsReceived)
                nbytes = noCharsReceived;

            /* Extract data and clear buffer. */
            memcpy(buf, (const void*)&_readBuf[0], nbytes);
            _pRdBuf = &_readBuf[0];
            _noEOL = 0;
            
            result = (ssize_t)nbytes;
        }        
    }
    ihw_leaveCriticalSection(msr);

    return result;

} /* End of read */
#endif



