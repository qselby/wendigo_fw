/**
 * @file spi.h
 * @brief Low-level SPI0 driver for the ATtiny3227 (tinyAVR 2-series).
 *
 * Target board : EV58A83A ATtiny3227 Curiosity Nano
 * Peripheral   : SPI0 — normal (unbuffered) master mode
 *
 * Pin mapping (SPI0 default position — no PORTMUX change required):
 * ┌─────────┬──────┬───────────────────────────────────────────────┐
 * │ Signal  │ Pin  │ Notes                                         │
 * ├─────────┼──────┼───────────────────────────────────────────────┤
 * │ MOSI    │ PA1  │ Hardware SPI0, output                         │
 * │ MISO    │ PA2  │ Hardware SPI0, input                          │
 * │ SCK     │ PA3  │ Hardware SPI0, output (also EXTCLK — see §1)  │
 * │ #CS0    │ PA4  │ GPIO active-low (hardware SS pin, see §2)     │
 * │ #CS1    │ PA7  │ GPIO active-low, free pin                     │
 * └─────────┴──────┴───────────────────────────────────────────────┘
 *
 * §1  PA3 doubles as the external clock input (EXTCLK). This only
 *     matters if CLKCTRL.MCLKCTRLA is configured to use an external
 *     clock source. The factory default uses the internal oscillator,
 *     so PA3 is free for SCK under normal conditions.
 *
 * §2  PA4 is the hardware SS pin. SPI0.CTRLB.SSD is set to 1 in
 *     spi_init() so the peripheral ignores the pin state and cannot
 *     inadvertently drop out of master mode.
 */

#ifndef SPI_H
#define SPI_H

#include <avr/io.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Types
 * ========================================================================= */

/**
 * @brief Chip-select channel selector.
 */
typedef enum {
    SPI_CS0 = 0,    /**< #CS0 on PA4 */
    SPI_CS1 = 1     /**< #CS1 on PA7 */
} spi_cs_t;

/**
 * @brief SPI clock rate relative to F_CPU.
 *
 * The CLK2X variants (DIV2, DIV8, DIV32) set the CLK2X bit in CTRLA
 * alongside the prescaler field, doubling the effective clock rate.
 *
 * Example at F_CPU = 20 MHz:
 *   SPI_CLK_DIV4   →  5.00 MHz
 *   SPI_CLK_DIV16  →  1.25 MHz
 *   SPI_CLK_DIV2   → 10.00 MHz
 */
typedef enum {
    SPI_CLK_DIV4   = (SPI_PRESC_DIV4_gc),
    SPI_CLK_DIV16  = (SPI_PRESC_DIV16_gc),
    SPI_CLK_DIV64  = (SPI_PRESC_DIV64_gc),
    SPI_CLK_DIV128 = (SPI_PRESC_DIV128_gc),
    /* CLK2X variants — same prescalers but with the doubler engaged: */
    SPI_CLK_DIV2   = (SPI_PRESC_DIV4_gc  | SPI_CLK2X_bm),
    SPI_CLK_DIV8   = (SPI_PRESC_DIV16_gc | SPI_CLK2X_bm),
    SPI_CLK_DIV32  = (SPI_PRESC_DIV64_gc | SPI_CLK2X_bm)
} spi_clk_t;

/**
 * @brief SPI bus mode (clock polarity / clock phase).
 */
typedef enum {
    SPI_MODE_0 = SPI_MODE_0_gc,     /**< CPOL=0, CPHA=0 — idle LOW,  sample rising  */
    SPI_MODE_1 = SPI_MODE_1_gc,     /**< CPOL=0, CPHA=1 — idle LOW,  sample falling */
    SPI_MODE_2 = SPI_MODE_2_gc,     /**< CPOL=1, CPHA=0 — idle HIGH, sample falling */
    SPI_MODE_3 = SPI_MODE_3_gc      /**< CPOL=1, CPHA=1 — idle HIGH, sample rising  */
} spi_mode_t;

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Initialise SPI0 in master mode.
 *
 * - Configures PA1 (MOSI), PA2 (MISO), PA3 (SCK) for hardware SPI0.
 * - Configures PA4 (#CS0) and PA7 (#CS1) as GPIO outputs, driven HIGH
 *   (deasserted) before the peripheral is enabled.
 * - Sets CTRLB.SSD so the hardware ignores the SS pin; both chip-selects
 *   are managed entirely in software.
 * - Does not touch PORTMUX; the default SPI0 position already maps to
 *   PA1–PA4.
 *
 * @param mode  Clock polarity and phase (SPI_MODE_0 … SPI_MODE_3).
 * @param clk   Clock prescaler (SPI_CLK_DIV2 … SPI_CLK_DIV128).
 */
void spi_init(spi_mode_t mode, spi_clk_t clk);

/**
 * @brief Assert (drive LOW) the specified chip-select line.
 *
 * Call this before the first byte of a transaction.
 *
 * @param cs  SPI_CS0 or SPI_CS1.
 */
void spi_select(spi_cs_t cs);

/**
 * @brief Deassert (drive HIGH) the specified chip-select line.
 *
 * Call this after the last byte of a transaction.
 *
 * @param cs  SPI_CS0 or SPI_CS1.
 */
void spi_deselect(spi_cs_t cs);

/**
 * @brief Full-duplex single-byte transfer.
 *
 * Blocks until the 8-clock cycle completes.
 *
 * @param data  Byte to transmit on MOSI.
 * @return      Byte received on MISO.
 */
uint8_t spi_transfer(uint8_t data);

/**
 * @brief Write one byte, discarding the received byte.
 * @param data  Byte to transmit.
 */
void spi_write(uint8_t data);

/**
 * @brief Read one byte by clocking out a dummy 0xFF.
 * @return  Byte received from the peripheral.
 */
uint8_t spi_read(void);

/**
 * @brief Write a buffer, discarding all received bytes.
 *
 * @param buf  Pointer to data to transmit.
 * @param len  Number of bytes.
 */
void spi_write_buf(const uint8_t *buf, uint8_t len);

/**
 * @brief Read a buffer, clocking out 0xFF for each byte.
 *
 * @param buf  Destination buffer (must be at least @p len bytes).
 * @param len  Number of bytes to receive.
 */
void spi_read_buf(uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* SPI_H */
