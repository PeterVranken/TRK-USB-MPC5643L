/**
 * @file sio_serialIO.c
 * Support of the LINFlex device of the MPC5643L for serial text I/O. The device is
 * configured as UART and fed by DMA. We get a serial RS 232 output channel of high
 * throughput with a minimum of CPU interaction.\n
 *   Input is done by interrupt on a received character. The bandwidth of the input channel
 * is by far lower than the output. This is fine for the normal use case, controlling an
 * application by some input commands, but would become a problem if the intention is to
 * download large data amounts, e.g. for a kind of boot loader.\n
 *   The API is a small set of basic read and write routines, which adopt the conventions
 * of the C standard library so that the C functions for formatted output become usable.\n
 * Note, the binding to the formatted output functions of the C library is not part of
 * this module.\n
 *   Note, formatted input is not possible through C standard functions.
 *
 * Copyright (C) 2017-2019 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
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
 *   sio_initSerialInterface
 *   sio_scFlHdlr_writeSerial
 *   sio_writeSerial (inline)
 *   sio_osWriteSerial
 *   sio_osGetChar
 *   sio_osGetLine
 * Local functions
 *   configSIULForUseWithOpenSDA
 *   configDMA
 *   configLINFlex
 *   linFlexRxInterrupt
 *   registerInterrupts
 */

/*
 * Include files
 */

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "MPC5643L.h"
#include "typ_types.h"
#include "sio_serialIO_defSysCalls.h"
#include "rtos_ivorHandler.h"
#include "rtos.h"



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

/** The interrupt priority for serial input. The interrupt is requested by the UART when
    another byte has been received. The range is 1..15.
      @remark The chosen priority needs to be greater than the priority of any context,
    that makes use of the input related API functions of this module. */
#define INTC_PRIO_IRQ_UART_FOR_SERIAL_INPUT     6

/** The size of the ring buffer for serial output can be chosen as a power of two of bytes.
      @remark Note, the permitted range of values depends on the reservation of space made
    in the linker control file. The macro here needs to be maintained in sync with the
    symbol ld_noBitsDmaRingBuffer, that is maintained in the linker file. */
#define SERIAL_OUTPUT_RING_BUFFER_SIZE_PWR_OF_TWO   10

/** The size of the ring buffer for serial input as number of bytes. The maximum capacity
    is one Byte less than the size. */
#define SERIAL_INPUT_RING_BUFFER_SIZE   257

/** The default behavior of terminal programs is to send a CR at the end of a message. By
    configuration, this can also be a pair of CR and LF. For serial input, this module
    handles this by compile time settings. Each of the two characters can be configured to
    be understood as end of line and the other one can be filtered out. If it is not
    filtered out then it behaves like any ordinary character, it becomes part of the read
    message that is passed on to the application.\n
      Here we have the end of line character. Should normally be carriage return but may
    also be the linefeed. Which one can depend on the terminal software you use.\n
      Note, for serial output, this module doesn't handle EOL at all. */
#define SERIAL_INPUT_EOL    '\r'

/** See #SERIAL_INPUT_EOL for an explanation. Here we have a character to be filtered out
    from the input stream. Should normally be the other one as configured for
    #SERIAL_INPUT_EOL. Filtering inactive is expressed by '\0' (but the zero byte is not
    filtered). */
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
volatile unsigned long sio_serialOutNoDMATransfers SECTION(.sbss.OS) = 0;

/** The ring buffer for serial output can be momentarily full. In such a case a sent
    message can be truncated (from a few bytes shortend up to entirely lost). This
    development support variable counts the number of messages since power-up, which
    underwent trunction.
      @remark Because of the race conditions between serial I/O interrupt and application
    software can not clearly relate a change of \a sio_serialOutNoTruncatedMsgs to a
    particular character or message it sends with sio_osWriteSerial(). In particular, it must
    not try to reset the counter prior to a read operation in order to establish such a
    relation. The application will just know that there are garbled messages. */
volatile unsigned long sio_serialOutNoTruncatedMsgs SECTION(.sbss.OS) = 0;

/** The ring buffer for serial output can be momentarily full. In such a case a sent
    message can be truncated (from a few bytes shortend up to entirely lost). This
    development support variable counts the number of truncated, lost message characters
    since power-up.
      @remark See \a sio_serialOutNoTruncatedMsgs for race conditions with the output
    functions of this module's API. Just the same holds for \a
    sio_serialOutNoLostMsgBytes. */ 
volatile unsigned long sio_serialOutNoLostMsgBytes SECTION(.sbss.OS) = 0;

/** The ring buffer used for the DMA based serial output.
      @remark The size of the buffer is defined here in the C source code but there is a
    strong dependency on the linker control file, too. The log2(sizeOfBuffer) least
    significant bits of the buffer address need to be zero. The buffer address (and thus
    its alignment) is specified in the linker file, which therefore limits the maximum size
    of the buffer. */
static _Alignas(SERIAL_OUTPUT_RING_BUFFER_SIZE) uint8_t
                                    _serialOutRingBuf[SERIAL_OUTPUT_RING_BUFFER_SIZE]
                                    SECTION(.dmaRingBuffer._serialOutRingBuf);

/** The write index into the ring buffer used for serial output. Since we use bytes and
    since the log2(sizeOfBuffer) least significant bits of the buffer address are zero
    the address of the index element is \a _serialOutRingBuf | \a _serialOutRingBufIdxWrM,
    which is equal to \a _serialOutRingBuf + \a _serialOutRingBufIdxWrM.
      @remark The variable is only used modulo SERIAL_OUTPUT_RING_BUFFER_SIZE, i.e. the more
    significant bits don't care (but aren't necessarily zero). This is indicated by the M
    at the end of the name. */
static volatile unsigned int _serialOutRingBufIdxWrM SECTION(.sbss.OS) = 0;

/** The ring buffer used for the interrupt based serial input. No particular section is
    required. Due to the low performance requirements we can use any location and do normal
    address arithmetics. */
static volatile uint8_t _serialInRingBuf[SERIAL_INPUT_RING_BUFFER_SIZE] SECTION(.sbss.OS);

/** A pointer to the end of the ring buffer used for serial input. This pointer facilitates
    the cyclic pointer update.
      @remark Note, the pointer points to the last byte in the buffer, not to the first
    address beyond as usually done. */
static volatile const uint8_t * _pEndSerialInRingBuf SECTION(.sdata.OS) =
                                    &_serialInRingBuf[SERIAL_INPUT_RING_BUFFER_SIZE-1];

/** The pointer to the next write position in the ring buffer used for serial input. */
static volatile uint8_t * volatile _pWrSerialInRingBuf SECTION(.sdata.OS) =
                                                                    &_serialInRingBuf[0];

/** The pointer to the next read position from the ring buffer used for serial input. The
    buffer is considered empty if \a _pWrSerialInRingBuf equals \a _pRdSerialInRingBuf,
    i.e. the buffer can contain up to SERIAL_INPUT_RING_BUFFER_SIZE-1 characters. */
static volatile uint8_t * volatile _pRdSerialInRingBuf SECTION(.sdata.OS) =
                                                                    &_serialInRingBuf[0];

/** The number of received but not yet consumed end of line characters. Required for read
    line API function. */
static volatile unsigned int _serialInNoEOL SECTION(.sbss.OS) = 0;

/** The number of lost characters due to overfull input ring buffer. */
volatile unsigned long sio_serialInLostBytes SECTION(.sbss.OS) = 0; 

#ifdef DEBUG
/** Count all characters received since last reset. This variable is supported in DEBUG
    compilation only. */
volatile unsigned long sio_serialInNoRxBytes SECTION(.sbss.OS) = 0;
#endif

 
/*
 * Function implementation
 */

/**
 * Initialize the external pin mapping. If LINFlexD_0 is configured then the LINFlexD
 * device can be used without additional hardware or wiring for the serial communication
 * with the openSDA chip on the TRK-USB-MPC5643L evaluation board.\n
 *   Additional wiring is required for LINFlexD_1. The ports PD9/12 are used for TX, RX,
 * respectively.\n
 *   Other pin mappings are not supported. Usually there is more than one choices per
 * LINFlexD device. If the code is run on another board it depends which pins to be used.
 * This cannot be anticipated by the code offered here.
 */
static void configSIULForUseWithOpenSDA(void)
{
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

} /* End of configSIULForUseWithOpenSDA */



/**
 * Initialize the DMA device. The chosen channel is set up to write the contents of a
 * cyclic buffer of fixed address and size into the UART.
 *   @remark
 * The DMA initialization is mostly related to the DMA channel in use (which is considered
 * globally reserved for this purpose). However, this function accesses some DMA registers,
 * too, that affect all channels (e.g. channel arbitration). This function will require
 * changes, when the module is integrated into an environment, where different DMA channels
 * are applied for different, unrelated purposes.
 */
static void configDMA(void)
{
    /* Check preconditions for use of DMA with modulo source addressing. If this assertion
       fires it may point to a inconsistency between the C source code and the linker
       control file, which provides the address of the buffer. */
    assert(((uintptr_t)_serialOutRingBuf & SERIAL_OUTPUT_RING_BUFFER_IDX_MASK) == 0);
    
    /* The linker script is required to provide a properly aligned buffer without risking
       to loose lots of RAM because of the alignment. Therefore, the linker script itself
       has a constant for the size of the buffer. We need to double check the consistency
       of linker configuration with C code. */
    extern uint8_t ld_noBitsDmaRingBuffer[0] ATTRIB_DBG_ONLY;
    assert((uintptr_t)ld_noBitsDmaRingBuffer == SERIAL_OUTPUT_RING_BUFFER_SIZE_PWR_OF_TWO);

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
       it in the next call of sio_osWriteSerial. */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.BITER = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD20_.B.CITER = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD20_.B.CITER_LINKCH = 0;

    /* Do a single transfer; don't repeat, don't link to other channels. 1: Do once, 0:
       Continue by repeating all */
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.D_REQ = 1;
    
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.INT_HALF = 0;
    EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.INT_MAJ = 0;
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
       the next use of sio_osWriteSerial. */
    
    /* Route LINFlex TX DMA request to eDMA channel DMA_CHN_FOR_SERIAL_OUTPUT.
         ENBL, 0x80: Enable channel
         SOURCE, 0x3f: Selection of DMAMUX input. The devices are hardwired to the DMAMUX
       and the index of a specific device can be found in table 18-4, MCU ref. manual, p.
       388. Index 22: LINFlexD_0, Tx, index 24: LINFlexD_1, Tx */
    DMAMUX.CHCONFIG[DMA_CHN_FOR_SERIAL_OUTPUT].R |= (0x80 + 22 + 2*IDX_LINFLEX_D);

} /* End of configDMA */




/**
 * Initialization of the MPC5643L's I/O device LINFlex_0. The device is put into UART mode
 * for serial in-/output.
 *   @param baudRate
 * The Baud rate in Hz. Allow values range from 10 .. 1000000, proven values range from 300
 * till 115200 Hz. ("Proven" relates to the TRK-USB-MPC6543L connected to a Windows host
 * through openSDA and USB.)
 *   @remark
 * To match the correct Baud rates the code assumes a peripheral clock rate of 120 MHz.
 */
static void configLINFlex(unsigned int baudRate)
{
    /* Avoid over-/underflow down below. */
    if(baudRate < 10)
        baudRate = 10;
    else if(baudRate > 1000000)
        baudRate = 1000000;
    
    /* Please find the UART register description in the MCU ref. manual, section 30.10, p.
       979ff. */

    /* Enter INIT mode. This is a prerequisite to access the other registers.
       INIT, 0x1: 1 init mode, 0 normal operation or sleep
       SLEEP, 0x2: 1 sleep mode, 0: normal operation */
    LINFLEX.LINCR1.R = 0x1;

    /* Wait for acknowledge of the INIT mode. */
    while((LINFLEX.LINSR.R & 0xf000) != 0x1000 /* initialization mode*/)
    {}

    /* Configure the LINFlex to operate in UART mode
       UART, 0x1: 0 for UART, 1 for LIN
         The UART bit is set prior to other bits in the same register in
       order to become able to write the other configuration bits. */
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

       Example:
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

    /* Clear all possibly pending status bits by w2c access. RM 30.10.6, p. 992. */
    LINFLEX.UARTSR.R = 0x0000ffafu;

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
    LINFLEX.LINCR1.R = 0x0; /* INIT, 0x1: 0, back to normal operation */
    
    /* According to RM 30.10.3 we would expect LINSR.LINS transit to 2, to indicate the idle
       state. However, the following loop is endless, LINS changes from 1 (initialization
       mode) to 0 (sleep mode) and remains there. Nonetheless, the UART is worimg well. */
    //while((LINFLEX.LINSR.R & 0xf000) != 0x2000 /* idle */){}

} /* End of configLINFlex */




/**
 * Interrupt handler for UART RX event. A received character is read from the UART hardware
 * and put into our ring buffer if there's space left. Otherwise the character is counted
 * as lost without further remedial action.
 */
static void linFlexRxInterrupt(void)
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
#if SERIAL_INPUT_FILTERED_CHAR != 0
    if(c != SERIAL_INPUT_FILTERED_CHAR)
#endif
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
               of the availability of the new character to the application API functions. */
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

    /* Acknowlege the interrupt by w2c and enable the next one at the same time. */
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
#define IDX_LINFLEX_RX_IRQ  (79 + 20*IDX_LINFLEX_D)

    /* Register our IRQ handler. */
    rtos_osRegisterInterruptHandler( &linFlexRxInterrupt
                                   , /* vectorNum */ IDX_LINFLEX_RX_IRQ
                                   , /* psrPriority */ INTC_PRIO_IRQ_UART_FOR_SERIAL_INPUT
                                   , /* isPreemptable */ true
                                   );
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
void sio_initSerialInterface(unsigned int baudRate)
{
    /* Connect the LINFlexD device with the external MCU pins.
         If LINFlexD_0 is configured on the evaluation board TRK-USB-MPC5643L, then
       communication with the host computer via the openSDA chip and the USB connection
       becomes possible. */
    configSIULForUseWithOpenSDA();
    
    /* Configure the LINFlex device for serial in- and output. */
    configLINFlex(baudRate);

    /* Register the interrupt handler serial RX. */
    registerInterrupts();
    
    /* Initialize DMA for writing into the UART. */
    configDMA();

    /* Empty receive buffer. */
    _pWrSerialInRingBuf =
    _pRdSerialInRingBuf = &_serialInRingBuf[0];
    
} /* End of sio_initSerialInterface */



/** 
 * System call handler for entry into data output. A byte string is sent through the serial
 * interface. Actually, the bytes are queued for sending and the function is non-blocking. 
 *   @return
 * The number of queued bytes is returned. Normally, this is the same value as argument \a
 * noBytes. However, the byte sequence can be longer than the currently available space in
 * the send buffer. (Its size is fixed and no reallocation strategy is implemented.) The
 * tranmitted message will be truncated if the return value is less than function argument
 * \a noBytes.
 *   @param PID
 * The process ID of the calling task.
 *   @param msg
 * The byte sequence to send. Note, this may be but is not necessarily a C string with zero
 * terminations. Zero bytes can be sent, too.
 *   @param noBytes
 * The number of bytes to send. For a C string, this will mostly be \a strlen(msg).
 *   @remark
 * This function must never be called directly. The function is only made for placing it in
 * the global system call table.
 */
unsigned int sio_scFlHdlr_writeSerial( uint32_t PID ATTRIB_UNUSED
                                     , const char *msg
                                     , unsigned int noBytes
                                     )
{
    /* The system call handler gets a pointer to the message to print. We need to validate
       that this pointer, coming from the untrusted user code doesn't break our safety
       concept. A user process may read only from all used ROM and all used RAM. */
    if(!rtos_checkUserCodeReadPtr(msg, noBytes))
    {
        /* The user specified memory region is not entirely inside the permitted,
           accessible range. This is a severe user code error, which is handeld with an
           exception, task abort and counted error. */
        rtos_osSystemCallBadArgument();
    }
    
    /* After checking the potentially bad user input we may delegate it to the "normal"
       function implementation. */
    return sio_osWriteSerial(msg, noBytes);

} /* End of sio_scFlHdlr_writeSerial */
        
        

        
/** 
 * Principal API function for data output. A byte string is sent through the serial
 * interface. Actually, the bytes are queued for sending and the function is
 * non-blocking.\n
 *   The function can be called from any context. However, it must not be
 * called untill function sio_initSerialInterface() has completed.
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
 *   @remark
 * This function must be called by trusted code in supervisor mode only. It belongs to the
 * sphere of trusted code itself.
 */
unsigned int sio_osWriteSerial( const char *msg
                              , unsigned int noBytes
                              )
{
    /* Do not interfere with a (possibly) running DMA transfer if we don't really need to
       do anything. */
    if(noBytes == 0)
        return 0;

    /* The manipulation if the output buffer and the DMA registers is done inside a
       critical section, which implements mutual exclusion of all contexts. So
       any context can safely apply this function. */
    uint32_t msr = rtos_osEnterCriticalSection();
    {
        /* Stop the (possibly) running DMA channel.
             See 19.2.1.15 and RM of MPC5748G, 70.5.8.1: Coherently stop a DMA channel with
           the ability of resuming it later. */
        while((EDMA.DMAHRSL.R & (0x1<<DMA_CHN_FOR_SERIAL_OUTPUT)) != 0)
        {}
        EDMA.DMACERQ.R = DMA_CHN_FOR_SERIAL_OUTPUT;

        /* Note, most buffer addresses or indexes in this section of the code are
           understood as cyclic, i.e. modulo the buffer size. This is indicated by an M as
           last character of the affected symbols but not mentioned again in the code
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
        const unsigned int noBytesTillEnd = MODULO(- _serialOutRingBufIdxWrM);
        unsigned int noBytesAtEnd;
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
        
        /* Apply a memory barrier to ensure that all data is in memory before we (re-)start
           the DMA transfer. */
        atomic_thread_fence(memory_order_seq_cst);

        _serialOutRingBufIdxWrM = _serialOutRingBufIdxWrM + noBytes;

        /* Start DMA. We can do this unconditionally because we have filtered the special
           situation of not writing any new character. */
        const uint32_t noBytesPending = MODULO(_serialOutRingBufIdxWrM - idxEndOfFreeSpaceM);
        assert(noBytesPending > 0);
    
        /* Set the number of bytes to transfer to the UART by DMA.
             Note, here we have a problem with the NXP support file MPC5643L.h. The
           same value needs to be written to the two fields CITER and BITER of the
           Transfer Control Words 5 and 7, respectively. These fields are defined
           conditionally, depending on the channel-to-channel linking bit e_link - they
           have either 9 or 15 Bit. This could be mapped by a support file e.g. in form
           of a union, which allows both variants. Unfortunately, MPC5643L.h defines
           CITER and BITER unconditionally but differently. We use the 15 Bit length and
           may use MPC5643L.h to access BITER but must not use the support file to
           access CITER. */
        assert((unsigned)noBytesPending <= SERIAL_OUTPUT_RING_BUFFER_SIZE-1);
        EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD28_.B.BITER = noBytesPending;
        //EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD20_.B.CITER = noBytesPending;
        const uint16_t doff = 0;
        EDMA.CHANNEL[DMA_CHN_FOR_SERIAL_OUTPUT].TCDWORD20_.R =
                                                    ((noBytesPending & 0x7fff) << 16) | doff;

        /* Enable the DMA channel to accept the UART's requests for bytes. This
           initiates or resumes the DMA transfer.
             NOP, 0x80: 1: Ignore write to register (to permit 32 Bit access to more
           than one of these byte registers at a time)
             SERQ, 0x40: 0: Address channel with SERQ, 1: Enable all channels
             SERQ, 0xf: Channel number */
        EDMA.DMASERQ.R = DMA_CHN_FOR_SERIAL_OUTPUT;
        ++ sio_serialOutNoDMATransfers;

#undef MODULO
    }
    rtos_osLeaveCriticalSection(msr);
    
    return noBytes;
    
} /* End of sio_osWriteSerial */



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
 *   @remark
 * This function must be called by trusted code in supervisor mode only. It belongs to the
 * sphere of trusted code itself.
 */
signed int sio_osGetChar(void)
{
    signed int c;
    
    /* Reading the ring buffer is done in a critical section to ensure mutual exclusion
       with the it filling interrupt. */
    uint32_t msr = rtos_osEnterCriticalSection();
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
    rtos_osLeaveCriticalSection(msr);

    return c;

} /* End of sio_osGetChar */




/**
 * The function reads a line of text from serial in and stores it into the string pointed
 * to by \a str. It stops when the end of line character is read and returns an
 * empty string if no such character has been received since system start or the previous
 * call of this function.\n
 *   Note, the latter condition means that the function is non-blocking - it doesn't wait
 * for further serial input.\n
 *   The end of line character, if found, is not copied into \a str. A terminating zero
 * character is automatically appended after the characters copied to \a str.\n
 *   The end of line character is a part of this module's compile time configuration, see
 * #SERIAL_INPUT_EOL. Standard for terminals is '\r', not '\n'. The other character out of
 * these two can or cannot be part of the copied line of text, see
 * #SERIAL_INPUT_FILTERED_CHAR. This, too, is a matter of compile time configuration.
 *   @return
 * This function returns \a str on success, and NULL on error or if not enough characters
 * have been received meanwhile to form a complete line of text.\n
 *   Note the special situation of a full receive buffer without having received any end of
 * line character. The system would be stuck - later received end of line characters would
 * be discarded becaus eof the full buffer and this function could never again return a
 * line of text. Therefore the function will return the complete buffer contents at once as
 * a line of input.
 *   @param str
 * This is the pointer to an array of chars where the C string is stored. \a str is the
 * empty string if the function returns NULL.
 *   @param sizeOfStr
 * The capacity of \a str in Byte. The maximum message length is one less since a
 * terminating zero character is always appended. A value of zero is caught by assertion.\n
 *   Note, if \a sizeOfStr is less than the line of text to be returned then the complete
 * line of text will nonetheless be removed from the receive buffer. Some characters from
 * the input stream would be lost.
 *   @remark
 * The serial interface is not restricted to text characters. If the source sends a zero
 * byte then this byte will be placed into the client's buffer \a str and it'll truncate
 * the message that is interpreted as C string.
 *   @remark
 * This function is a mixture of C lib's \a gets and \a fgets: Similar to \a gets it
 * doesn't put the end of line character into the result and similar to \a fgets it
 * respects the size of the receiving buffer.
 *   @remark 
 * Both, no data available yet and an empty input line of text return the same, empty C
 * string in \a str, but they differ in function return code, which is NULL or \a str,
 * respectively.
 *   @remark
 * On buffer overrun, i.e. if the client code didn't invoke this function fast enough, an
 * end of line won't be written into the internal receive buffer and the truncated line
 * will be silently concatenated with its successor. You may consider observing the global
 * variable \a sio_serialInLostBytes to recognize this situation. Note, because of the race
 * conditions between serial I/O interrupt and application software you can not clearly
 * relate a change of these variables to a particular message you get from this function.
 * In particular, you must not try to reset the counter prior to a read operation in order
 * to establish such a relation. Your application will just know that there is some garbled
 * input.
 *   @remark
 * This function must be called by trusted code in supervisor mode only. It belongs to the
 * sphere of trusted code itself.
 */
char *sio_osGetLine(char str[], unsigned int sizeOfStr)
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
    uint32_t msr = rtos_osEnterCriticalSection();
    {
        volatile uint8_t * pRd = _pRdSerialInRingBuf;

        /* If no line has been received then we need to double-check that the buffer is not
           entirely full; if so we were stuck: No new characters (i.e. no newline) could
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

                /* pWr points immediately before pRd: Buffer is currently full. Copy
                   complete contents as a (pseudo-) line of text. */
                unsigned int noBytesToCopy = SERIAL_INPUT_RING_BUFFER_SIZE-1;
                /// @todo Count and report lost characters at least in DEBUG compilation
                if(noBytesToCopy > sizeOfStr)
                    noBytesToCopy = sizeOfStr;

                /* Copy the ring buffer (limited to the requested number of characters).
                   Consider wrapping at the end of the linear area. */
                const unsigned int noBytesTillEnd = (unsigned int)(_pEndSerialInRingBuf - pRd)
                                                    + 1;
                unsigned int noBytesAtEnd;
                if(noBytesTillEnd >= noBytesToCopy)
                {
                    /* The requested number of characters is still found at the end of the
                       ring buffer. */
                    noBytesAtEnd = noBytesToCopy;
                }
                else
                {
                    /* We need to copy a second character sequence from the beginning of
                       the ring buffer into the destination. */
                    noBytesAtEnd = noBytesTillEnd;
                }
                
                assert(noBytesAtEnd <= sizeOfStr);
                memcpy(str, (const uint8_t*)pRd, noBytesAtEnd);
                if(noBytesAtEnd < noBytesToCopy)
                {
                    memcpy( str+noBytesAtEnd
                          , (const uint8_t*)&_serialInRingBuf[0]
                          , noBytesToCopy-noBytesAtEnd
                          );
                }
                
                /* The client code expects a zero terminated C string. */
                str[sizeOfStr] = '\0';

                /* Adjust read pointer such that it represents the empty buffer. */
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
                /// @todo else Count and report lost characters at least in DEBUG compilation
                
                /* Cyclically advance read pointer. */
                pRd = pRd == _pEndSerialInRingBuf? &_serialInRingBuf[0]: pRd + 1;
                
            } /* End while(All characters to consume from ring buffer) */
            
            /* Write the terminating zero byte to make the text line a C string. */
            *pWrApp = '\0';
        }
    }
    rtos_osLeaveCriticalSection(msr);

    return result;

} /* End of sio_osGetLine */


