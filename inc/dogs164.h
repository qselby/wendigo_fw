/**
 * @file dogs164.h
 * @brief Driver for the EA DOGS164-A 4×16 character LCD (SSD1803A controller).
 *
 * Target board  : EV58A83A ATtiny3227 Curiosity Nano
 * Display       : Electronic Assembly EA DOGS164x-A
 * Controller    : Solomon Systech SSD1803A
 * Interface     : 4-wire SPI, Mode 3 (CPOL=1, CPHA=1), max 1 MHz
 *
 * Hardware assumptions (matching the companion spi.h driver):
 *   MOSI  PA1  │  SCK   PA3  │  CS0  PA4  ← display chip-select
 *   MISO  PA2  │  (no MISO connection needed — display is write-only)
 *
 * RST pin wiring:
 *   The display RST pin is pulled HIGH through an external resistor.
 *   The MCU does NOT drive RST.  A 50 ms power-on delay in dogs164_init()
 *   allows the internal reset circuit to complete before any SPI traffic.
 *
 * SPI clock:
 *   dogs164_init() calls spi_init() with SPI_CLK_DIV32, yielding 625 kHz
 *   at F_CPU = 20 MHz.  If you share the SPI bus with another device that
 *   requires a different clock rate, save and restore the SPI settings
 *   around each dogs164 call, or reinitialise with spi_init() as needed.
 *
 * SSD1803A serial protocol (4-wire write, 3 bytes per transfer):
 *   Byte 1 — sync  : 0xF8 (command) or 0xFA (data)
 *   Byte 2 — upper : DB7–DB4 in bits 7–4, bits 3–0 = 0
 *   Byte 3 — lower : DB3–DB0 in bits 7–4, bits 3–0 = 0
 */

#ifndef DOGS164_H
#define DOGS164_H

#include <stdint.h>
#include <stdbool.h>
#include "spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Display geometry
 * ========================================================================= */

#define DOGS164_COLS  16u   /**< Characters per row.   */
#define DOGS164_ROWS   4u   /**< Number of text rows.  */

/* =========================================================================
 * Contrast
 * ========================================================================= */

/** Contrast range: 0 (faintest) … 63 (darkest). */
#define DOGS164_CONTRAST_MIN      0u
#define DOGS164_CONTRAST_MAX     63u

/**
 * Default contrast applied by dogs164_init().
 * Value 42 (0b101010) matches the datasheet initialisation example and
 * gives a clean reading at room temperature.  Adjust for your optic.
 */
#define DOGS164_CONTRAST_DEFAULT 42u

/* =========================================================================
 * Types
 * ========================================================================= */

/**
 * @brief Viewing direction / mounting orientation.
 *
 * DOGS164_BOTTOM_VIEW (6 o'clock)  — normal upright mounting.
 * DOGS164_TOP_VIEW    (12 o'clock) — display rotated 180°, read from above.
 *
 * The two modes differ in the Entry Mode Set command sent during init and
 * in the DDRAM row base addresses used by dogs164_set_cursor():
 *
 *   Bottom view row bases: 0x00, 0x20, 0x40, 0x60
 *   Top view    row bases: 0x04, 0x24, 0x44, 0x64
 */
typedef enum {
    DOGS164_BOTTOM_VIEW = 0,   /**< 6:00  — read from below (default orientation) */
    DOGS164_TOP_VIEW    = 1    /**< 12:00 — read from above (rotated 180°)         */
} dogs164_view_t;

/**
 * @brief Built-in character set (ROM) selector.
 *
 * All three ROMs share ASCII codes 0x20–0x7F.  They differ in the
 * extended range 0xA0–0xFF:
 *   ROM_A — English / Japanese (katakana)
 *   ROM_B — Western European (Latin-1 supplement)
 *   ROM_C — Cyrillic
 *
 * ROM_B is selected by dogs164_init().
 */
typedef enum {
    DOGS164_ROM_A = 0,   /**< English / Japanese  */
    DOGS164_ROM_B = 1,   /**< Western European    */
    DOGS164_ROM_C = 2    /**< Cyrillic            */
} dogs164_rom_t;

/**
 * @brief Driver handle.  Declare one per display and pass to every API call.
 *
 * Treat the contents as opaque; initialise only through dogs164_init().
 */
typedef struct {
    spi_cs_t       cs;    /**< Chip-select line used for this display. */
    dogs164_view_t view;  /**< Mounting orientation.                   */
} dogs164_t;

/* =========================================================================
 * Initialisation
 * ========================================================================= */

/**
 * @brief Initialise the SSD1803A controller and bring the display to a
 *        known, ready state.
 *
 * Sequence performed:
 *  1. spi_init(SPI_MODE_3, SPI_CLK_DIV32) — 625 kHz @ 20 MHz F_CPU.
 *  2. 50 ms power-on delay (RST is passive pull-up; no MCU control).
 *  3. Full SSD1803A initialisation per the EA DOGS164-A datasheet p.4,
 *     adapted for 4-line top-view (12:00) mode.
 *  4. Default contrast DOGS164_CONTRAST_DEFAULT.
 *  5. Display on, cursor on, blink on.
 *  6. Screen cleared.
 *
 * @param dev   Uninitialised driver handle; filled in by this function.
 * @param cs    Chip-select line — SPI_CS0 (PA4) or SPI_CS1 (PA7).
 * @param view  DOGS164_BOTTOM_VIEW or DOGS164_TOP_VIEW.
 */
void dogs164_init(dogs164_t *dev, spi_cs_t cs, dogs164_view_t view);

/* =========================================================================
 * Display control
 * ========================================================================= */

/**
 * @brief Clear all characters and return the cursor to row 0, column 0.
 *
 * The SSD1803A requires ≈ 6.2 ms to execute the clear command.
 * A 7 ms blocking delay is inserted automatically.
 */
void dogs164_clear(dogs164_t *dev);

/**
 * @brief Return the cursor to row 0, column 0 without changing display contents.
 */
void dogs164_home(dogs164_t *dev);

/**
 * @brief Control display visibility, cursor underline, and cursor blink.
 *
 * @param dev         Driver handle.
 * @param display_on  true  = characters visible; false = blanked (contents kept).
 * @param cursor_on   true  = underline cursor shown at current position.
 * @param blink_on    true  = blinking block cursor shown at current position.
 */
void dogs164_display_ctrl(dogs164_t *dev,
                          bool display_on,
                          bool cursor_on,
                          bool blink_on);

/* =========================================================================
 * Cursor positioning
 * ========================================================================= */

/**
 * @brief Move the cursor to an absolute row/column position.
 *
 * @param dev  Driver handle.
 * @param row  Row index, 0 … (DOGS164_ROWS  - 1).
 * @param col  Column index, 0 … (DOGS164_COLS - 1).
 *
 * Out-of-range values are silently ignored.
 */
void dogs164_set_cursor(dogs164_t *dev, uint8_t row, uint8_t col);

/* =========================================================================
 * Text output
 * ========================================================================= */

/**
 * @brief Write one ASCII character at the current cursor position.
 *
 * The controller auto-increments the cursor after each write.
 *
 * @param dev  Driver handle.
 * @param c    Character to display.
 */
void dogs164_write_char(dogs164_t *dev, char c);

/**
 * @brief Write a null-terminated string starting at the current cursor position.
 *
 * No automatic line-wrapping is performed.  Text that runs past column 15
 * will wrap into adjacent DDRAM and produce incorrect output.  For multi-line
 * text, call dogs164_set_cursor() before writing each row:
 *
 * @code
 *   dogs164_set_cursor(&lcd, 0, 0);
 *   dogs164_write_str(&lcd, "Hello, world!   ");
 *   dogs164_set_cursor(&lcd, 1, 0);
 *   dogs164_write_str(&lcd, "Row 1           ");
 * @endcode
 *
 * @param dev  Driver handle.
 * @param s    Null-terminated string.
 */
void dogs164_write_str(dogs164_t *dev, const char *s);

/* =========================================================================
 * Contrast
 * ========================================================================= */

/**
 * @brief Adjust the LCD driving contrast.
 *
 * The SSD1803A splits the 6-bit contrast value across two commands:
 *   Power/Icon/Contrast (IS=1): holds C5:C4
 *   Contrast Set         (IS=1): holds C3:C0
 * This function enters and exits the IS=1 register bank transparently.
 *
 * @param dev       Driver handle.
 * @param contrast  6-bit value, 0 (faintest) – 63 (darkest).
 *                  Values > 63 are clamped to 63.
 */
void dogs164_set_contrast(dogs164_t *dev, uint8_t contrast);

/* =========================================================================
 * Character ROM selection
 * ========================================================================= */

/**
 * @brief Select one of the three built-in character ROMs.
 *
 * Takes effect immediately for all subsequent writes.  Use
 * DOGS164_ROM_B (Western European) for English text with accented
 * Latin characters.
 *
 * @param dev  Driver handle.
 * @param rom  DOGS164_ROM_A, DOGS164_ROM_B, or DOGS164_ROM_C.
 */
void dogs164_select_rom(dogs164_t *dev, dogs164_rom_t rom);

/* =========================================================================
 * Custom characters (CGRAM)
 * ========================================================================= */

/**
 * @brief Define a custom 5×8 character in CGRAM.
 *
 * The SSD1803A provides 8 user-definable character slots (0–7), available
 * as ASCII codes 0x00–0x07.  Once defined, write a custom character with:
 * @code
 *   dogs164_write_char(&lcd, '\x01');   // slot 1
 * @endcode
 *
 * @p pattern is an array of 8 bytes, one per pixel row (top to bottom).
 * Only bits 4–0 of each byte are used (5-pixel wide character cell).
 * Byte [7] is the cursor row and is typically 0x00.
 *
 * Example — solid 3×5 rectangle:
 * @code
 *   static const uint8_t box[8] = { 0x1F, 0x11, 0x11, 0x11, 0x1F,
 *                                    0x00, 0x00, 0x00 };
 *   dogs164_define_char(&lcd, 0, box);
 * @endcode
 *
 * @param dev      Driver handle.
 * @param slot     CGRAM slot, 0–7.
 * @param pattern  8-byte pixel pattern, MSB unused (only bits 4–0 matter).
 */
void dogs164_define_char(dogs164_t *dev,
                         uint8_t slot,
                         const uint8_t pattern[8]);

#ifdef __cplusplus
}
#endif

#endif /* DOGS164_H */
