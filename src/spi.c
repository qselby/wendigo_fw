/**
 * @file spi.c
 * @brief Low-level SPI0 driver for the ATtiny3227 (tinyAVR 2-series).
 *
 * Target board : EV58A83A ATtiny3227 Curiosity Nano
 *
 * See spi.h for full pin mapping and design notes.
 */

#include "spi.h"
#include <avr/io.h>

/* =========================================================================
 * Internal pin definitions
 *
 * All five signals live on PORTA, so every GPIO operation is a single
 * atomic register write (OUTSET / OUTCLR / DIRSET / DIRCLR).
 * ========================================================================= */
#define SPI_MOSI_PIN  PIN1_bm   /* PA1 — MOSI, hardware SPI0 */
#define SPI_MISO_PIN  PIN2_bm   /* PA2 — MISO, hardware SPI0 */
#define SPI_SCK_PIN   PIN3_bm   /* PA3 — SCK,  hardware SPI0 */
#define SPI_CS0_PIN   PIN4_bm   /* PA4 — #CS0, active-low GPIO */
#define SPI_CS1_PIN   PIN7_bm   /* PA7 — #CS1, active-low GPIO */

/* =========================================================================
 * spi_init
 * ========================================================================= */
void spi_init(spi_mode_t mode, spi_clk_t clk)
{
    /*
     * 1. Pre-drive CS lines HIGH before enabling outputs.
     *    This prevents a spurious assertion glitch during pin direction
     *    setup, as OUT is latched before DIR takes effect on the pin.
     */
    PORTA.OUTSET = SPI_CS0_PIN | SPI_CS1_PIN;

    /*
     * 2. Configure pin directions.
     *    MOSI, SCK, CS0, CS1 → outputs
     *    MISO                 → input  (DIR bit stays cleared)
     */
    PORTA.DIRSET = SPI_MOSI_PIN | SPI_SCK_PIN | SPI_CS0_PIN | SPI_CS1_PIN;
    PORTA.DIRCLR = SPI_MISO_PIN;

    /*
     * 3. PORTMUX — no change needed.
     *    PORTMUX.SPIROUTEA reset value is 0x00, which selects the default
     *    SPI0 pin position (PA1/PA2/PA3/PA4). Writing it explicitly here
     *    makes the intent clear and survives any earlier misconfiguration.
     */
    PORTMUX.SPIROUTEA = PORTMUX_SPI0_DEFAULT_gc;

    /*
     * 4. SPI Control Register B (configure before CTRLA.ENABLE).
     *
     *    SPI_SSD_bm  — Slave Select Disable.
     *                  Mandatory when CS is managed as GPIO. Without this,
     *                  a low level on PA4 (the hardware SS pin) would force
     *                  the peripheral back into slave mode mid-transaction.
     *
     *    mode        — CPOL/CPHA bits from caller.
     *
     *    BUFEN = 0   — Normal (unbuffered) mode. The polling loop in
     *                  spi_transfer() uses the IF flag (bit 7 of INTFLAGS),
     *                  which is simpler and has no FIFO edge cases.
     */
    SPI0.CTRLB = SPI_SSD_bm | (uint8_t)mode;

    /*
     * 5. SPI Control Register A — enable last.
     *
     *    SPI_MASTER_bm — Host/master mode.
     *    clk           — Prescaler and optional CLK2X bit from caller.
     *    DORD = 0      — MSB transmitted first (change to SPI_DORD_bm for
     *                    LSB-first peripherals).
     *    SPI_ENABLE_bm — Power on the peripheral.
     */
    SPI0.CTRLA = SPI_MASTER_bm | (uint8_t)clk | SPI_ENABLE_bm;
}

/* =========================================================================
 * Chip-select control
 * ========================================================================= */
void spi_select(spi_cs_t cs)
{
    if (cs == SPI_CS0)
        PORTA.OUTCLR = SPI_CS0_PIN;
    else
        PORTA.OUTCLR = SPI_CS1_PIN;
}

void spi_deselect(spi_cs_t cs)
{
    if (cs == SPI_CS0)
        PORTA.OUTSET = SPI_CS0_PIN;
    else
        PORTA.OUTSET = SPI_CS1_PIN;
}

/* =========================================================================
 * Core transfer — polled, blocking
 *
 * Writing SPI0.DATA starts the 8-clock shift.  The IF flag (INTFLAGS
 * bit 7, also named SPI_RXCIF_bm in the device headers for the 2-series)
 * is set when the transfer completes.  Reading SPI0.DATA clears the flag
 * atomically, so no explicit flag-clear write is needed.
 * ========================================================================= */
uint8_t spi_transfer(uint8_t data)
{
    SPI0.DATA = data;
    while (!(SPI0.INTFLAGS & SPI_IF_bm))
        ;
    return SPI0.DATA;
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
