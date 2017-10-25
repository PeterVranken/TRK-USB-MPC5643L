/**
 * @file sio_serialIO.c
 * Support of the LINFlex device of the MPC5643L. The device is configured as UART and fed
 * by DMA. We get a serial RS 232 output channel of high throughput with a minimum of CPU
 * interaction.\n
 *   Input is done by interrupt on a received character. The bandwidth of the input channel
 * is by far lower than the output. This is fine for the normal use case, controlling an
 * application by some input commands, but would become a problem if the intention is to
 * download large data amounts, e.g. for a kind of boot loader.\n
 *   The API is a small set of basic read and write routines, which adopt the conventions
 * of the C standard library so that the C functions for formatted output become usable.
 * Note, the binding to the formatted output functions of the C library is not part of
 * this module.
 * (Formatted input is not possible through C standard functions.)
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
 *   sio_writeSerial
 *   sio_getChar
 *   sio_getLine
 * Local functions
 *   initPBridge
 *   initDMA
 *   initLINFlex
 *   dmaTransferCompleteInterrupt
 *   linFlexRxInterrupt
 *   registerInterrupts
 */

/*
 * Include files
 */

#include <unistd.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "sio_serialIO.h"
#include "ihw_initMcuCoreHW.h"



/*
 * Defines
 */
 
/** The MPC has two LINFlex devices. This define select the one to be used for serial
    output. Possible is the assignment of either 0 or 1.
      @remark With the TRK-USB-MPC5643L evaluation board the LINFlexD_0 is the preferred
    choice. This device is connected to the host machine through USB and can be used with a
    terminal software on the host without any additional hardware or wiring.
      @remark Setting this define to a value other than 0 has never been tested. */
#define IDX_LINFLEX_D 0

/** The DMA channel to serve the UART with sent data bytes. */
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

/** The size of the ring buffer for serial output can be chosen as a power of two of bytes.
      @remark Note, the permitted range of values depends on the reservation of space made
    in the linker control file. */
#define SERIAL_OUTPUT_RING_BUFFER_SIZE_PWR_OF_TWO   10

/** The size of the ring buffer for serial input as number of bytes. The maximum capacity
    is one Byte less than the size. */
#define SERIAL_INPUT_RING_BUFFER_SIZE   257

/** The default behavior of terminal programs is to send a CR at the end of a message. By
    configuration, this can also be a pair of CR and LF. This module handles this by
    compile time settings. Each of the two characters can be configured to be understood as
    end of line and the other one can be filtered out. If it is not filtered out then it
    behaves like any ordinary character, it becomes part of the read message that is passed
    on to the application.
      Here we have the end of line character. Should normally be carriage return but may
    also be the linefeed. */
#define SERIAL_INPUT_EOL    '\r'

/** See #SERIAL_INPUT_EOL for an explanation. Here we have a character to be filtered out
    from the input stream. Should normally be linefeed or inactive. Inactive is expressed
    by '\0' (but the zero byte is not filtered). */
#define SERIAL_INPUT_FILTERED_CHAR  '\n'

/** Compute the size of the output ring buffer as number of bytes. */
#define SERIAL_OUTPUT_RING_BUFFER_SIZE  (1u<<(SERIAL_OUTPUT_RING_BUFFER_SIZE_PWR_OF_TWO)) 

/** Used for index arithemtics: A mask for the index bits in an integer word. Here for the
    serial output buffer. */
#define SERIAL_OUTPUT_RING_BUFFER_IDX_MASK  (SERIAL_OUTPUT_RING_BUFFER_SIZE-1)


/* The LINFlex device to be used is selected by name depending on the setting of
   #IDX_LINFLEX_D. */
#if IDX_LINFLEX_D == 0
# define LINFLEX LINFLEX0
#elif IDX_LINFLEX_D == 1
# define LINFLEX LINFLEX1
#else
# error Invalid configuration, unknown LINFlex device specified
#endif


/*
 * Local type definitions
 */
 
 
/*
 * Local prototypes
 */
 
 
/*
 * Data definitions
 */
 
/** This development support variable counts the number of DMA transfers intiated since
    power-up, to do the serial output */
volatile unsigned long sio_serialOutNoDMATransfers = 0;

/** The ring buffer for serial output can be memntarily full. In such a case a sent message
    can be truncated (from a few bytes shortend up to entirely lost). This development
    support variable counts the number of message since power-up, which underwent
    trunction.
      @remark Because of the race conditions between serial I/O interrupt an application
    software can not clearly relate a change of \a sio_serialOutNoTruncatedMsgs to a
    particular character or message it gets from the read functions sio_getChar or
    sio_getLine. In particular, it must not try to reset the counter prior to a read
    operation in order to establish such a relation. The application will just know that
    there are garbled messages. */
volatile unsigned long sio_serialOutNoTruncatedMsgs = 0;

/** The ring buffer for serial output can be memntarily full. In such a case a sent message
    can be truncated (from a few bytes shortend up to entirely lost). This development
    support variable counts the number of truncated, lost message characters since
    power-up.
      @remark See \a sio_serialOutNoTruncatedMsgs for race conditions with the input
    functions of this module's API. Just the same holds for \a
    sio_serialOutNoLostMsgBytes. */ 
volatile unsigned long sio_serialOutNoLostMsgBytes = 0;

/** The ring buffer used for the DMA based serial output.
      @remark The size of the buffer is defined here in the C source code but there is a
    strong dependency on the linker control file, too. The log2(sizeOfBuffer) least
    significant bits of the buffer address need to be zero. The buffer address (and thus
    its alignment) is specified in the linker file, which therefore limits the maximum size
    of the buffer. */
static uint8_t SECTION(.heap.dmaRingBuffer) _serialOutRingBuf[SERIAL_OUTPUT_RING_BUFFER_SIZE];

/** The write index into the ring buffer used for serial output. Since we use bytes and
    since the log2(sizeOfBuffer) least significant bits of the buffer address are zero
    the address of the index element is \a _serialOutRingBuf | \a _serialOutRingBufIdxWrM,
    which is equal to \a _serialOutRingBuf + \a _serialOutRingBufIdxWrM.
      @remark The variable is only used modulo SERIAL_OUTPUT_RING_BUFFER_SIZE, i.e. the more
    significant bits don't care (but aren't necessarily zero). This is indicated by the M
    at the end of the name. */
static volatile unsigned int _serialOutRingBufIdxWrM = 0;

/** On-DMA-Complete interrupt and API function sio_writeSerial need to share the
    information, whether a transfer is currently running. The flag is set when all elder
    output had been completed and the client code demand a new output. It is reset when a
    DMA transfer completes and the client code has not demanded a new output meanwhile. */
static volatile bool _serialOutDmaTransferIsRunning = false;

/** The ring buffer used for the interrupt based serial input. No particular section is
    required. Due to the low performance requirements we can use any location and do normal
    address arithmetics. */
static volatile uint8_t _serialInRingBuf[SERIAL_INPUT_RING_BUFFER_SIZE];

/** A pointer to the end of the ring buffer used for serial input. This pointer facilitates
    the cyclic pointer update.
      @remark Note, the pointer points to the last byte in the buffer, not to the first
    address beyond as usually done. */
static volatile const uint8_t * _pEndSerialInRingBuf =
                                    &_serialInRingBuf[SERIAL_INPUT_RING_BUFFER_SIZE-1];

/** The pointer to the next write position in the ring buffer used for serial input. */
static volatile uint8_t * volatile _pWrSerialInRingBuf = &_serialInRingBuf[0];

/** The pointer to the next read position from the ring buffer used for serial input. The
    buffer is considered empty if \a _pWrSerialInRingBuf equals \a _pRdSerialInRingBuf,
    i.e. the buffer can contain up to SERIAL_INPUT_RING_BUFFER_SIZE-1 characters. */
static volatile uint8_t * volatile _pRdSerialInRingBuf = &_serialInRingBuf[0];

/** The number of received but not yet consumed end of line characters. Required for read
    line API function. */
static volatile unsigned int _serialInNoEOL = 0;

/** The number of lost characters due to overfull input ring buffer. */
volatile unsigned long sio_serialInLostBytes = 0; 

#ifdef DEBUG
/** Count all characters received since last reset. This variable is support in DEBUG
    compilation only. */
volatile unsigned long sio_serialInNoRxBytes = 0;
#endif

 
/*
 * Function implementation
 */

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
    /* Check preconditions for use of DMA with modulo source addressing. If this assert
       fires it may point to a inconsistency between the C source code and the linker
       control file, which provides the address of the buffer. */
    assert(((uintptr_t)_serialOutRingBuf & SERIAL_OUTPUT_RING_BUFFER_IDX_MASK) == 0);
    
    /* Initialize write to ring buffer. */
    _serialOutRingBufIdxWrM = 0;
    
    /* Initial load address of source data is the beginning of the ring buffer. */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD0_.B.SADDR =
                                    (vuint32_t)&_serialOutRingBuf[_serialOutRingBufIdxWrM];
    /* Read 1 byte per transfer. */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD4_.B.SSIZE = 0;
    /* After transfer, add 1 to the source address. */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD4_.B.SOFF = 1;
    /* After major loop, do not move the source pointer. Next transfer will read from next
       cyclic address. */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD12_.B.SLAST = 0;
    /* Source modulo feature is applied to implement the ring buffer. */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD4_.B.SMOD =
                                                SERIAL_OUTPUT_RING_BUFFER_SIZE_PWR_OF_TWO;

    /* Load address of destination is fixed. It is the byte input of the UART's FIFO. */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD16_.B.DADDR =
                                                        ((vuint32_t)&LINFLEX.BDRL.R) + 3;
    /* Write 1 byte per transfer. */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD4_.B.DSIZE = 0;
    /* After transfer, do not alter the destination address. */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD20_.B.DOFF = 0;
    /* After major loop, do not alter the destination address. */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD24_.B.DLAST_SGA = 0;
    /* Destination modulo feature is not used. */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD4_.B.DMOD = 0;

    /* Transfer 1 byte per minor loop */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD8_.B.SMLOE = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD8_.B.DMLOE = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD8_.B.MLOFF = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD8_.B.NBYTES = 1;

    /* Initialize the begining and current major loop iteration counts to zero. We will set
       it in the next call of sio_writeSerial. */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.BITER = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD20_.B.CITER = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD20_.B.CITER_LINKCH = 0;

    /* Do a single transfer; don't repeat, don't link to other channels. 1: Do once, 0:
       Continue by repeating all */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.D_REQ = 1;
    
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.INT_HALF = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.INT_MAJ = 1;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD20_.B.CITER_E_LINK = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.BITER_E_LINK = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.MAJOR_E_LINK = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.E_SG = 0;
    
    /* 0: No stalling, 3: Stall for 8 cycles after each byte; fast enough for serial com. */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.BWC = 3;
    
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.START = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.DONE = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.ACTIVE = 0;

    /* ERCA, 0x4: 1: Round robin for channel arbitration, 0: Priority controlled
       EDBG, 0x2: 1: Halt DMA when entering the debugger.
         Note, this setting affects all channels! */
    EDMA.DMACR.R = 0x2;

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
       the next use of sio_writeSerial. */
    
    /* Route LINFlex TX DMA request to eDMA channel DMA_CHN_FOR_SERIAL_OUTPUT.
         ENBL, 0x80: Enable channel
         SOURCE, 0x3f: Selection of DMAMUX input. The devices are hardwired to the DMAMUX
       and the index of a specific device can be found in table 18-4, MCU ref. manual, p.
       388. Index 22: LINFlexD_0, Tx, index 24: LINFlexD_1, Tx */
    DMAMUX.CHCONFIG[DMA_CHN_FOR_SERIAL_OUTPUT].R |= (0x80 + 22 + 2*IDX_LINFLEX_D);

} /* End of initDMA */




/**
 * Initialization of the MPC5643L's I/O device LINFlex_0. The device is put into UART mode
 * for serial in-/output.
 *   @param baudRate
 * The Baud rate in Hz. Allow values range from 10 .. 1000000, proven values range from 300
 * till 115200 Hz.
 *   @remark
 * To match the correct Baud rates the code assumes a peripheral clock rate of 120 MHz.
 *   @todo Make the selection of the LINFlex device (0/1) an argument
 */
static void initLINFlex(unsigned int baudRate)
{
    /* Avoid over-/underflow down below. */
    if(baudRate < 10)
        baudRate = 10;
    else if(baudRate > 1000000)
        baudRate = 1000000;
    
    /* Please find the UART register description in the MCU ref. manual, section 30.10, p.
       979ff. */

    /* Enter INIT mode. This is a prerequiste to access the other registers.
       INIT, 0x1: 1 init mode, 0 normal operation or sleep
       SLEEP, 0x2: 1 sleep mode, 0: normal operation */
    LINFLEX.LINCR1.R = 0x1;

    /* Wait for acknowledge of the INIT mode. */
    while (0x1000 != (LINFLEX.LINSR.R & 0xf000))
    {}

    /* Configure SIUL. Specify for the affected MCU pins, which function they have. We
       connect the RX and TX ports of the LINFlex_0 device with the MCU pins, that are
       connected to the USB-to-serial chip. The possible connections are (MCU ref. manual,
       table 3-5, p. 95ff):
       LINFlexD_0, TX: PB2
       LINFlexD_0, RX: PB3, PB7
       LINFlexD_1, TX: PD9, PF14
       LINFlexD_1, RX: PB13, PD12, PF15 */

    /* Principal register PCR of SIUL:
       SMC: irrelevant, 0x4000
       APC: digital pin use, 0x2000 = 0
       PA, 0xc00: output source select, n means ALTn, n=0 is GPIO
       OBE, 0x200: relevant only for ALTn!=0, better to set =0 otherwise
       IBE: input buffer, relevance unclear, 0x100=0 (off)/1 (on)
       ODE: Open drain, 0x20=0 (push/pull), 1 means OD
       SRC: Slew rate, 0x4=1 (fastest), 0 means slowest
       WPE: "weak pull-up", meaning unclear, 0x2=0 (off)
       WPS: Pull-up/down, irrelevant 0x1=1 (up)/0 (down) */
    if(IDX_LINFLEX_D == 0)
    {
        /* We connect the pair PB2/3, which is connected to the USB to serial converter
           MC9S08JM60CLD on the evaluation board. This permits direct connection to the RS
           232 through a virtual COM port visible on the host machine.
             We choose:
           TX: PA=1=0x400, OBE=0=0, IBE=0=0, ODE=0=0, SRC=1=0x4, WPE=0=0 => 0x404
           RX: PA=0=0, OBE=0=0, IBE=1=0x100 => 0x100 */
        SIU.PCR[18].R = 0x0404;     /* Configure pad PB2, TX, for ALT1: LINFlexD_0, TXD */
        SIU.PCR[19].R = 0x0100;     /* Configure pad PB3 for LINFlexD_0, RXD */
    }
    else
    {
        assert(IDX_LINFLEX_D == 1);
        
        /* We connect to the pair PD9/12, which is connected to the tower extension bus of
           the evaluation board. Using this pin pair requires additional, external wiring.
             We choose:
           TX: PA=2=0x800, OBE=0=0, IBE=0=0, ODE=0=0, SRC=1=0x4, WPE=0=0 => 0x804
           RX: PA=0=0, OBE=0=0, IBE=1=0x100 => 0x100 */
        SIU.PCR[57].R = 0x0804;     /* Configure pad PD9, TX, for ALT2: LINFlexD_1, TXD */
        SIU.PCR[60].R = 0x0100;     /* Configure pad PD12 for LINFlexD_0, RXD */
    }

    /* PSMI: Input select. */
    SIU.PSMI31.B.PADSEL = 0;    /* PSMI[31]=0 connects pin B3 with LINFlexD_0 RX. */

    /* Configure the LINFlex to operate in UART mode
       UART, 0x1: 0 for UART, 1 for LIN
         Note, the NXP samples set the UART bit prior to other bits in the same register in
       order to become able to write the other configuration bits. This has not been
       doubted although such behavior is not documented in the MCU manual, section 30.9. */
    LINFLEX.UARTCR.R = 0x0001;

    /* RDFLRFC, 0x1c00: (no bytes to receive - 1) in buffer mode or read FIFO fill amount
       RFBM: RX buffer/FIFO mode, 0x200, 0 means buffer, 1 FIFO mode
       TFBM: TX buffer/FIFO mode, 0x100, 0 means buffer, 1 FIFO mode
       PCE: Parity enable, 0x4, 0 means off
       WL: Word length, 0x80+0x2, value b01 means data 8 Bit
       RX, TX enable, 0x20 and 0x10, respectively (Can be set after leaving the init mode.) */
    LINFLEX.UARTCR.R = 0x0133; /* TX FIFO mode, RX buffer mode, 8bit data, no parity, Tx
                                    enabled, UART mode stays set */

    /// @todo It's unclear if it is always required to use channel 0 in UART mode.
    LINFLEX.DMATXE.R = 0x1; /* Enable DMA TX channel. */

    /* Configure baudrate:
       fsys is 120 MHz (peripheral clock).
       LFDIV = fsys / (16 * desired baudrate)
       LINIBRR.IBR = integer part of LFDIV
       LINFBRR.FBR = 16 * fractional part of LFDIV (after decimal point)

       for instance:
       LFDIV = 120e6/(16*19200) = 390.625
       LINIBRR.IBR = 390
       LINFBRR.FBR = 16*0.625 = 10 
       
       390:10 19200 bd, 65:2 115200 bd, 58:10 128000 bd, 29:5 256000 bd, 8:2 921600 bd
       
        19200 bd worked well with terminal.exe and putty
       115200 bd worked well with terminal.exe and putty
       128000 bd showed transmission errors with terminal.exe and putty
       256000 bd failed with terminal.exe and putty
       921600 bd failed with terminal.exe (not tried with putty) */
    const unsigned long IBR = 7500000ul / baudRate
                      , FBR = (7500000ul - IBR*baudRate)*16 / baudRate;
    assert((IBR & ~0xffffful) == 0  &&  (FBR & ~0xful) == 0);
    LINFLEX.LINIBRR.B.IBR = IBR;
    LINFLEX.LINFBRR.B.FBR = FBR;

#define USE_FIELD_ALIASES_LINFLEX xxx

    /* LINIER: Interrupt enable. The bits relate to the bits of same name in LINESR (error
       bits), LINSR and UARTSR (both status).
         BOIE: Buffer overrun could be read in handling of DBFIE
         DBFIE: should report FIFO full in reception mode
         DBEIETOIE: Should request new data for TX, UARTSR[TO] needs to be set
         DRIE: Interrupt on byte received, DRF set in UARTSR
         DTIE: Interrupt on byte sent, DTF set in UARTSR */
    LINFLEX.LINIER.B.DRIE = 1;
    
    /* GCR
       STOP: 0 for 1 or 1 for 2 stop bits
       SR: set 1 to reset counters, buffers and FIFO but keep configuration and operation */

    /* Enter normal mode again. */
    /// @todo Why don't we wait as on init?
    LINFLEX.LINCR1.R = 0x0; /* INIT, 0x1: 0, back to normal operation */
    
} /* End of initLINFlex */




/**
 * Interrupt handler for DMA channel DMA_CHN_FOR_SERIAL_OUTPUT.
 *   @remark
 * This interrupt must have a priority higher than any OS schedule relevant interrupt. The
 * application tasks using the serial channel must not become active.
 */
static void dmaTransferCompleteInterrupt(void)
{
    /* Note, most buffer addresses or indexes in this section of the code are understood as
       cylic, i.e. modulo the buffer size. This is indicated by an M as last character of
       the affectzed symbols but not mentioned again in the code comments. */
#define MODULO(bufIdx)    ((bufIdx) & SERIAL_OUTPUT_RING_BUFFER_IDX_MASK)

    /* Interrupt should be raised on transfer done. Reset of the bit by software is however
       not required. */
#define IRQ_MASK    (0x1 << DMA_CHN_FOR_SERIAL_OUTPUT)
    assert(EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.DONE == 1);
    assert((EDMA.DMASERQ.R & IRQ_MASK) == 0);
    assert(_serialOutDmaTransferIsRunning);
    
    /* MCU ref manual is ambiguous in how to reset the interrupt request bit: It says both,
       DMAINTL is a normal read modify write register and writing a 1 would reset the
       corresponding bit while writing a 0 has no effect. Tried out: the latter works
       well (and doesn't generate race conditions with other DMA channels like a
       read-modify-write). */
    EDMA.DMAINTL.R = IRQ_MASK;
    
    /* Check the cyclic address computation feature of the DMA in modulo mode. */
    assert(EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD0_.B.SADDR >= (uintptr_t)&_serialOutRingBuf[0]
           &&  EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD0_.B.SADDR
               < (uintptr_t)&_serialOutRingBuf[SERIAL_OUTPUT_RING_BUFFER_SIZE]
          );

    /* We need to retrigger the DMA transfer if the ring buffer has been written meanwhile
       with new data. */
    const uint32_t noBytesWrittenMeanwhile = 
                        MODULO(_serialOutRingBufIdxWrM - EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD0_.B.SADDR);

    /* Same pointer values is used as empty indication. Therefore it is not possible to
       entirely fill the buffer. Condition "less than" holds. */
    assert(noBytesWrittenMeanwhile < SERIAL_OUTPUT_RING_BUFFER_SIZE);

    if(noBytesWrittenMeanwhile > 0)
    {
        /* Set the number of bytes to transfer by DMA to the UART. */
        EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.BITER = noBytesWrittenMeanwhile;
        EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD20_.B.CITER = noBytesWrittenMeanwhile;

        /* Enable the UART to request bytes from the DMA. This initiates a subsequent DMA
           transfer. */
        ++ sio_serialOutNoDMATransfers;
        EDMA.DMASERQ.R = DMA_CHN_FOR_SERIAL_OUTPUT;
    }            
    else
    {
        /* No subsequent DMA transfer is immediately initiated, so the application code
           will need to start one the next time the API function sio_writeSerial is
           called. */
        _serialOutDmaTransferIsRunning = false;
    }
#undef MODULO
} /* End of dmaTransferCompleteInterrupt */



/**
 * Interrupt handler for UART RX event. A received character is read from the UART hadrware
 * and put into our ring buffer if there's space left. Otherwise the character is counted
 * as lost without further remedial action.
 */
static void linFlexRxInterrupt()
{
    /* Get the received byte. */
    /// @todo Record a buffer overrun bit in case the IRQ handler comes too slow
    const unsigned char c = LINFLEX.BDRM.B.DATA4;
    
#ifdef DEBUG
    ++ sio_serialInNoRxBytes;
#endif

    /* To support different terminal characteristics, one character can be configured to
       be silently ignored in the input stream. This shall normally be the linefeed
       character. */
    if(c != SERIAL_INPUT_FILTERED_CHAR)
    {
        /* Check for buffer full. Compute next write position at the same time. */
        volatile uint8_t * const pWrTmp = _pWrSerialInRingBuf
                       , * const pWrNext = pWrTmp < _pEndSerialInRingBuf? pWrTmp+1
                                                                        : &_serialInRingBuf[0];

        /* Put the byte into our buffer if there's enough room. */
        if(pWrNext != _pRdSerialInRingBuf)
        {
            *pWrTmp = c;
        
            /* Count the received end of line characters. (The variable is decremented on
               consumption of such a character.) */
            if(c == SERIAL_INPUT_EOL)
                ++ _serialInNoEOL;

            /* Update write position into ring buffer. This is at the same time the indication
               of the availablity of the new character to the application API functions. */
            _pWrSerialInRingBuf = pWrNext;
        }
        else    
        {
            /* Buffer overrun, count lost character. */
            ++ sio_serialInLostBytes;
        }
    
        /* Ensure that all relevant memory changes are visible before we leave the
           interrupt. */
        atomic_thread_fence(memory_order_seq_cst);
        
    } /* End if(Character discarded?) */

    /* Acknowlege the interrupt and enable the next one at the same time. */
    assert((LINFLEX.UARTSR.R & 0x4) != 0);
    LINFLEX.UARTSR.R = 0x4;  
    
} /* End of linFlexRxInterrupt */



/**
 * Our locally implemented interrupt handlers are registered at the operating system for
 * serving the required I/O devices (DMA and LINFlex 0 or 1).
 */
static void registerInterrupts(void)
{
/* Interrupt offsets taken from MCU reference manual, p. 936. The DMA interrupts for the
   different channels start with 11, e.g. 26 for DMA channel 15. */
#define IDX_DMA_IRQ         (11+DMA_CHN_FOR_SERIAL_OUTPUT)
#define IDX_LINFLEX_RX_IRQ  (79 + 20*IDX_LINFLEX_D)

    /* Register our IRQ handlers. Priority is chosen low for output DMA since we serve a
       slow data channel, which even has a four Byte queue inside. */
    /// @todo Priorities to be aligned with rest of application, consider intended RTOS
    ihw_installINTCInterruptHandler( &dmaTransferCompleteInterrupt
                                   , /* vectorNum */ IDX_DMA_IRQ
                                   , /* psrPriority */ INTC_PRIO_IRQ_DMA_FOR_SERIAL_OUTPUT
                                   , /* isPreemptable */ true
                                   );
    ihw_installINTCInterruptHandler( &linFlexRxInterrupt
                                   , /* vectorNum */ IDX_LINFLEX_RX_IRQ
                                   , /* psrPriority */ INTC_PRIO_IRQ_UART_FOR_SERIAL_INPUT
                                   , /* isPreemptable */ true
                                   );
#undef IDX_DMA_IRQ 
#undef IDX_LINFLEX_RX_IRQ
} /* End of registerInterrupts */



/**
 * Initialize the I/O devices for serial output, in particular, these are the LINFlex
 * device plus a DMA channel to serve it.
 *   @param baudRate
 * The Baud rate of in- and output in Hz. Allow values range from 10 .. 1000000, proven
 * values range from 300 till 115200 Hz.
 *   @remark
 * This function needs to be called at system initialization phase, when all External
 * Interrupts are still suspended.
 */
void ldf_initSerialInterface(unsigned int baudRate)
{
    initLINFlex(baudRate);

    /* Register the interrupt handler for DMA. */
    registerInterrupts();
    
    /* Initialize DMA and connect it to the UART. */
    initDMA();

    /* Empty receive buffer. */
    _pWrSerialInRingBuf =
    _pRdSerialInRingBuf = &_serialInRingBuf[0];
    
} /* End of ldf_initSerialInterface */



/** 
 * Principal API function for data output. A byte string is sent through the serial
 * interface. Actually, the bytes are queued for sending and the function is non-blocking. 
 *   @return
 * The number of queued bytes is returned. Normally, this is the same value as argument \a
 * noBytes. However, the byte sequence can be longer than the currently available space in
 * the send buffer. (Its size is fixed and no reallocation strategy is implemented.) The
 * tranmitted message will be truncated if the return value is less than function argument
 * \a noBytes.
 *   @param msg
 * The byte sequence to send. Note, this may be but is not necessarily a C string with zero
 * terminations. Zero bytes can be send, too.
 *   @param noBytes
 * The number of bytes to send. For a C string, this will mostly be \a strlen(msg).
 */
unsigned int sio_writeSerial(const char *msg, unsigned int noBytes)
{
    /// @todo limit to buffersize -1 to avoid overflow in address computation
    uint32_t msr = ihw_enterCriticalSection();
    {
        /* Note, most buffer addresses or indexes in this section of the code are
           understood as cylic, i.e. modulo the buffer size. This is indicated by an M as
           last character of the affectzed symbols but not mentioned again in the code
           comments. */
#define MODULO(bufIdx)    ((bufIdx) & SERIAL_OUTPUT_RING_BUFFER_IDX_MASK)

        /* The current, i.e. next, transfer address of the DMA is the first (cyclic)
           address, which we must not touch when filling the buffer. This is the (current)
           end of the free buffer area. */
        const uint32_t idxEndOfFreeSpaceM =
                                EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD0_.B.SADDR;
        
        /* The cyclic character of the buffer can require one or two copy operations to
           place the message. We compute the concrete index ranges to copy.
             Note the -1: Same index values are used as empty-buffer-indication. Therefore
           it is not possible to entirely fill the buffer. */
        const unsigned int noBytesFree =
                                MODULO(idxEndOfFreeSpaceM - _serialOutRingBufIdxWrM - 1);
         
        /* Avoid buffer overrun by saturation of the user demand and report the number of
           overrun events and the number of lost message characters. */
        if(noBytes > noBytesFree)
        {
            ++ sio_serialOutNoTruncatedMsgs;
            sio_serialOutNoLostMsgBytes += noBytes - noBytesFree;
            noBytes = noBytesFree;
        }
        
        /* How many bytes would fit until we have to wrap? This limits the first copy
           operation. */
        unsigned int noBytesTillEnd = MODULO(SERIAL_OUTPUT_RING_BUFFER_SIZE
                                             - _serialOutRingBufIdxWrM
                                            )
                   , noBytesAtEnd;
        if(noBytes <= noBytesTillEnd)
        {
            /* The message fits into the rest of the linear buffer, no wrapping required. */
            noBytesAtEnd = noBytes;
        }
        else
        {
            /* A portion of the message is placed at the end of the linear buffer and the
               rest of the message at its beginning. */
            noBytesAtEnd = noBytesTillEnd;
        }
               
        /* Always copy the first part of the message to the current end of the linear
           buffer. */
        uint8_t * const pDest = &_serialOutRingBuf[MODULO(_serialOutRingBufIdxWrM)];
        assert(pDest + noBytesAtEnd <= &_serialOutRingBuf[SERIAL_OUTPUT_RING_BUFFER_SIZE]);
        memcpy(pDest, msg, noBytesAtEnd);
        
        /* Copy the second part of the message at the beginning of the linear buffer if
           there is a remainder. */
        if(noBytes > noBytesAtEnd)
        {
            assert(noBytes - noBytesAtEnd < sizeof(_serialOutRingBuf));
            memcpy(&_serialOutRingBuf[0], msg+noBytesAtEnd, noBytes-noBytesAtEnd);
        }           
        
        _serialOutRingBufIdxWrM = _serialOutRingBufIdxWrM + noBytes;

        /* Start DMA only if there's no currently running transfer (from a write of
           before). If there is such a running transfer then the next transfer will be
           initiated from its on-complete-interrupt. */
        if(noBytes > 0  &&  !_serialOutDmaTransferIsRunning)
        {
            /* Set the number of bytes to transfer by DMA to the UART. */
            assert((unsigned)noBytes <= SERIAL_OUTPUT_RING_BUFFER_SIZE-1);
            EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.BITER = noBytes;
            EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD20_.B.CITER = noBytes;

            /* Enable the DMA channel to accept the UART's requests for bytes. This
               initiates a DMA transfer.
                 NOP, 0x80: 1: Ignore write to register (to permit 32 Bit access to more
               than one of these byte registers at a time)
                 SERQ0, 0x40: 0: Address channel with SERQ, 1: Enable all channels
                 SERQ, 0xf: Channel number */
            atomic_thread_fence(memory_order_seq_cst);
            ++ sio_serialOutNoDMATransfers;
            EDMA.DMASERQ.R = DMA_CHN_FOR_SERIAL_OUTPUT;

            /* The status, whether we have currently started a transfer or not is shared
               with the on-complete-interrupt. */
            _serialOutDmaTransferIsRunning = true;
        }
#undef MODULO
    }
    ihw_leaveCriticalSection(msr);
    
    /* noBytes is saturated to buffer size-1 and can't overflow in conversion to signed. */
    return noBytes;
    
} /* End of sio_writeSerial */




/**
 * Application API function to read a single character from serial input or EOF if there's
 * no such character received meanwhile.
 *   @return
 * The function is non-blocking. If the receive buffer currently contains no character it
 * returns EOF (-1). Otherwise it returns the earliest received character, which is still
 * in the buffer.
 *   @remark
 * The return of EOF does not mean that the stream has been closed. It's just a matter of
 * having no input data temporarily. On reception of the more characters the function will
 * continue to return them.
 */
signed int sio_getChar()
{
    signed int c;
    
    /* Reading the ring buffer is done in a critical section to ensure mutual exclusion
       with the it filling interrupt. */
    uint32_t msr = ihw_enterCriticalSection();
    {
        /* Check for buffer empty. */
        if(_pRdSerialInRingBuf != _pWrSerialInRingBuf)
        {
            c = *_pRdSerialInRingBuf;

            /* Keep track of the received but not yet consumed end of line characters. (The
               variable is incremented on reception of such a character.) */
            if(c == SERIAL_INPUT_EOL)
            {
                assert(_serialInNoEOL > 0);
                -- _serialInNoEOL;
            }

            /* Update read position in the ring buffer. This is at the same time the indication
               towards the interrupt of having the character consumed. */
            volatile uint8_t * const pRdTmp = _pRdSerialInRingBuf;
            _pRdSerialInRingBuf = pRdTmp < _pEndSerialInRingBuf ? pRdTmp+1 
                                                                : &_serialInRingBuf[0];
        }
        else    
        {
            /* Nothing in buffer, return EOF. */
            c = -1;
        }
    }
    ihw_leaveCriticalSection(msr);

    return c;

} /* End of sio_getChar */




/**
 * The function reads a line of text from serial in and stores it into the string pointed
 * to by \a str. It stops when either the end of line character is read or the serial input
 * buffer is exhausted, whichever comes first.\n
 *   Note, the latter condition means that the function is non-blocking - it doesn't wait
 * for further serial input.\n
 *   The end of line character, if found, is not copied into \a str. A terminating null
 * character is automatically appended after the characters copied to \a str.\n
 *   The end of line character is a part of this module's compile time configuration, see
 * #SERIAL_INPUT_EOL. Standard for terminals is '\r', not '\n'. The other character out of
 * these two can or cannot be part of the copied line of text, see
 * #SERIAL_INPUT_FILTERED_CHAR. This, too, is a matter of compile time configuration.
 *   @return
 * This function returns \a str on success, and NULL on error or if not enough characters
 * have been received meanwhile to form a complete line of text.
 *   @param str
 * This is the pointer to an array of chars where the C string is stored. \a str is the
 * empty string if the function returns NULL.
 *   @param sizeOfStr
 * The capacity of \a str in Byte. The maximum message length is one less since a
 * terminating zero character is always appended. A value of zero is caught by assertion.
 *   @remark
 * This function is a mixture of C lib's \a gets and \a fgets: Similar to \a gets it
 * doesn't put the end of line character into the result and similar to \a fgets it
 * respects the size of the receiving buffer.
 *   @remark 
 * Both, no data available yet and an empty input line of text return the same, empty C
 * string in \a str, but they differ in function return code, which is NULL or \a str,
 * respectively.
 *   @remark
 * On buffer overrun, an end of line won't be written into the internal receive buffer and
 * the truncated line will be silently concatenated with its successor. You may consider
 * observing the global variables \a sio_serialOutNoTruncatedMsgs or \a
 * sio_serialInLostBytes to recognize this situation. Note, because of the race conditions
 * between serial I/O interrupt and application software you can not clearly relate a
 * change of these variables to a particular message you get from this function. In
 * particular, you must not try to reset the counter prior to a read operation in order to
 * establish such a relation. Your application will just know that there are some garbled
 * messages.
 */
char *sio_getLine(char str[], unsigned int sizeOfStr)
{
    if(sizeOfStr == 0)
    {
        assert(false);
        return NULL;
    }
    else
    {
        /* Reserve space for a terminating zero byte. */
        -- sizeOfStr;
    }
        
    char *result = &str[0]
       , *pWrApp = result;

    /* Reading the ring buffer is done in a critical section to ensure mutual exclusion
       with the it filling interrupt. */
    uint32_t msr = ihw_enterCriticalSection();
    {
        volatile uint8_t * pRd = _pRdSerialInRingBuf;

        /* If no line has been received then we need to double-check that the buffer is not
           entirely full; if sowe were stuck: No new characters (i.e. no new line) could
           ever be received and the application would never again get a line of input.
             If we find a full buffer then we consider the entire buffer as a single line
           of input. */
        if(_serialInNoEOL == 0)
        {
            volatile uint8_t * const pWr = _pWrSerialInRingBuf
                           , * const pWrNext = pWr == _pEndSerialInRingBuf
                                               ? &_serialInRingBuf[0]
                                               : pWr + 1;
            if(pWrNext == pRd)
            {
                volatile uint8_t *pCopyFrom;
                unsigned int noBytesToCopy;

                /* pWr points immediately before pRd: Buffer is currently full. Copy
                   complete contents as a (pseudo-) line of text. */
                pCopyFrom = pRd;
                noBytesToCopy = SERIAL_INPUT_RING_BUFFER_SIZE-1;

                /** @todo Copy the determined ring buffer area. Consider wrapping at the
                    end of the linear area. */
                assert(false);
                
                /** Adjust read pointer such that it represents the empty buffer. */
                _pRdSerialInRingBuf = pWr;
            }
            else
            {
                /* No complete line of text has been read so far. */
                str[0] = '\0';
                result = NULL;
            }
        }
        else
        {
            /* A line of text is available in the buffer. We copy the bytes in naive
               loop instead of using memcpy, since we anyway need such a loop to find the
               next EOL character. */
            while(true)
            {
                /* We can have an empty buffer here, there's at minimum the EOL left. */
                assert(pRd != _pWrSerialInRingBuf);
                
                /* Get next input character. */
                char c = (char)*pRd;
                
                /* Loop termination is the first matching EOL character. */
                if(c == SERIAL_INPUT_EOL)
                {
                    /* Acknowldge consumption of EOL character but do not return the
                       (redundant) EOL character to the application. */
                    -- _serialInNoEOL;
                    
                    /* Advance read pointer: The EOL is consumed by this call of the
                       function. */
                    _pRdSerialInRingBuf = pRd == _pEndSerialInRingBuf? &_serialInRingBuf[0]
                                                                     : pRd + 1;
                    
                    /* End the copy loop. */
                    break;
                }
                
                /* Copy next character only if destination buffer still has room left. We
                   continue to consume the rest of the line if this is not the case. */
                if(sizeOfStr > 0)
                {
                    * pWrApp++ = *pRd;
                    -- sizeOfStr;
                }
                
                /* Cyclically advance read pointer. */
                pRd = pRd == _pEndSerialInRingBuf? &_serialInRingBuf[0]: pRd + 1;
                
            } /* End while(All characters to consume from ring buffer) */
            
            /* Write the terminating zero byte to make the text line a C string. */
            *pWrApp = '\0';
        }
    }
    ihw_leaveCriticalSection(msr);

    return result;

} /* End of sio_getLine */


#if 0
/// @todo Let the write API with file ID become part of the printf compilation unit
signed int write(int file, const void *msg, size_t noBytes)
{
    /// @todo Do we need EOL conversion? Likely yes
    /// @todo Write elipsis (...) in case of buffer-full caused truncation. This requires beforehand computation of possible length
    
    if(noBytes == 0  ||  msg == NULL  ||  (file != stdout->_file  &&  file != stderr->_file))
        return 0;
    else
        return sio_writeSerial(msg, noBytes);
        
} /* End of write */


unsigned long sio_read_noInvocations = 0;
int sio_read_fildes = -99;
void *sio_read_buf = (void*)-99;
size_t sio_read_nbytes = 99;
unsigned int sio_read_noErrors = 0
           , sio_read_noLostBytes = 0;

/// @todo This API belongs into the printf module. We need a raw read char or read line
ssize_t read(int fildes, void *buf, size_t nbytes)
{
    ++ sio_read_noInvocations;
    sio_read_fildes = fildes;
    sio_read_buf = buf;
    sio_read_nbytes = nbytes;

    ssize_t result;
    uint32_t msr = ihw_enterCriticalSection();
    {
        const unsigned int noCharsReceived = (unsigned int)(_pRdBuf - &_readBuf[0]);
        assert(noCharsReceived <= sizeof(_readBuf));
        
        /* Normally the clib request a maximum of data, so that the error situation of
           having more characters received as requested will normally not happen. */
        if(nbytes < noCharsReceived)
        {
            ++ sio_read_noErrors;
            sio_read_noLostBytes += noCharsReceived - nbytes;
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
            _serialInNoEOL = 0;
            
            result = (ssize_t)nbytes;
        }
    }
    ihw_leaveCriticalSection(msr);

    return result;

} /* End of read */
#endif



