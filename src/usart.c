/**
 * @file usart.c
 * @brief Low-level USART0 driver for the ATtiny3227 (tinyAVR 2-series).
 *
 * Target board : EV58A83A ATtiny3227 Curiosity Nano
 *
 * See usart.h for full pin mapping and design notes.
 */

#include "usart.h"
#include <avr/io.h>

/* =========================================================================
 * Internal pin definitions
 *
 * All five signals live on PORTA, so every GPIO operation is a single
 * atomic register write (OUTSET / OUTCLR / DIRSET / DIRCLR).
 * ========================================================================= */
#define USART_TX_PIN  PIN2_bm   /* PB2 — TX, hardware USART0 */
#define USART_RX_PIN  PIN3_bm   /* PB3 — RX, hardware USART0 */

/* =========================================================================
 * Other definitions
 * ========================================================================= */
#define BAUD_RATE 115200UL

/* =========================================================================
 * usart_init
 * ========================================================================= */
void usart_init(usart_mode_t mode, usart_baud_t baud)
{
    /*
     * 1. Configure pin directions.
     *    TX    → outputs
     *    RX    → input  (DIR bit stays cleared)
     */
    PORTB.DIRSET = USART_TX_PIN;
    PORTB.DIRCLR = USART_RX_PIN;

    /*
     * 2. PORTMUX — Change to 0x03.
     *    PORTMUX.USARTROUTEA reset value is 0x00, which selects the default
     *    USART0 and USART1 pin positions (PB0/PB1/PB2/PB3 for USART0 and
     *    PA1/PA2/PA3/PA4 for USART1). Disconnect USART1 from the pins.
     */
    PORTMUX.USARTROUTEA = PORTMUX_USART0_DEFAULT_gc | PORTMUX_USART1_NONE_gc;

    /*
     * 3. USART Control Register A (configure before CTRLB.ENABLE).
     *
     *    LBME        — Loop-Back Mode Enable
     *                  Mandatory when CS is managed as GPIO. Without this,
     *                  a low level on PA4 (the hardware SS pin) would force
     *                  the peripheral back into slave mode mid-transaction.
     */
    USART0.CTRLA = LBME;

    /*
     * 4. USART Control Register C, NORMAL MODE.
     *
     *    All registers can be left to their default values.
     *    CMODE - 0x00, ASYNCHRONOUS
     *    PMODE - 0x0, DISABLED
     *    SBMODE - 0x0, 1 STOP BIT
     *    CHSIZE - 0x03, 8 BIT
     */
    USART0.CTRLC = mode;

    /*
     * 5. USART Control Register B, NORMAL MODE — enable last.
     *
     *    Enable TX and RX, all other registers can be left to their defaults.
     *    RXEN - 0x1, RX ENABLED
     *    TXEN - 0x1, TX ENABLED
     *    SFDEN - 0x0, Start-of-Frame Detection DISABLED
     *    ODME - 0x0, Open Drain Mode DISABLED
     *    RXMODE - 0x0, RX mode, NORMAL
     *    MPCM - 0x0, Multi-Processor Communication Mode DISABLED
     */
    USART0.CTRLB = RXEN | TXEN;
}

/* =========================================================================
 * Core recieve — polled, blocking
 *
 * TODO
 * ========================================================================= */
uint8_t usart0_rx(void)
{
    while (!(USART0.RXCIF))
        ;
    return USART0.RXDATAL;
}

/* =========================================================================
 * Core transmit — polled, blocking
 *
 * TODO
 * ========================================================================= */
void usart0_tx(uint8_t data)
{
    USART0.TXDATAL = data;
    while (!(USART0.TXCIF))
        ;
}

/* =========================================================================
 * Convenience wrappers
 * ========================================================================= */
void spi_write(uint8_t data)
{
    (void)spi_transfer(data);
}

uint8_t spi_read(void)
{
    return spi_transfer(0xFF);  /* Clock out a benign idle byte */
}

void spi_write_buf(const uint8_t *buf, uint8_t len)
{
    while (len--)
        spi_write(*buf++);
}

void spi_read_buf(uint8_t *buf, uint8_t len)
{
    while (len--)
        *buf++ = spi_read();
}
