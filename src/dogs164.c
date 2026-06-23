/**
 * @file dogs164.c
 * @brief Driver for the EA DOGS164-A 4×16 LCD (SSD1803A controller).
 *
 * See dogs164.h for full hardware and wiring notes.
 */

#include "dogs164.h"
#include <util/delay.h>

/* =========================================================================
 * SSD1803A 4-wire SPI sync bytes
 *
 * Every transfer opens with a sync byte whose format is:
 *   bits 7:3 = 11111  (synchronisation pattern)
 *   bit  2   = RS     (0 = instruction register, 1 = data register)
 *   bit  1   = RW     (always 0 for write)
 *   bit  0   = 0      (always 0)
 * ========================================================================= */

#define SYNC_CMD   0xF8u   /**< RS=0, RW=0 — write to instruction register */
#define SYNC_DATA  0xFAu   /**< RS=1, RW=0 — write to data register        */

/* =========================================================================
 * DDRAM row base addresses
 *
 * The SSD1803A maps the 4 text rows to non-contiguous DDRAM regions.
 * The starting address depends on viewing direction (see datasheet p.5).
 * ========================================================================= */

static const uint8_t ROW_ADDR_BOTTOM[DOGS164_ROWS] = { 0x00u, 0x20u, 0x40u, 0x60u };
static const uint8_t ROW_ADDR_TOP[DOGS164_ROWS]    = { 0x04u, 0x24u, 0x44u, 0x64u };

/* ROM selection byte lookup (datasheet p.6):
 *   ROM A → 0x00,  ROM B → 0x04,  ROM C → 0x0C              */
static const uint8_t ROM_SEL_BYTE[3] = { 0x00u, 0x04u, 0x0Cu };

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

/**
 * @brief Transmit one payload byte using the SSD1803A 3-byte serial protocol.
 *
 * CS must already be asserted by the caller; it is not touched here.
 * The controller samples MOSI on the falling SCK edge (Mode 3), which the
 * SPI peripheral handles automatically.
 *
 * @param sync  SYNC_CMD (0xF8) or SYNC_DATA (0xFA).
 * @param byte  8-bit payload (command opcode or character code).
 */
static inline void ssd1803_send(uint8_t sync, uint8_t byte)
{
    spi_write(sync);
    spi_write(byte & 0xF0u);           /* upper nibble → bits 7:4, bits 3:0 = 0 */
    spi_write((uint8_t)(byte << 4u));  /* lower nibble → bits 7:4, bits 3:0 = 0 */
}

/** @brief Assert CS, send a command byte, deassert CS. */
static void write_cmd(dogs164_t *dev, uint8_t cmd)
{
    spi_select(dev->cs);
    ssd1803_send(SYNC_CMD, cmd);
    spi_deselect(dev->cs);
}

/** @brief Assert CS, send a data byte, deassert CS. */
static void write_data(dogs164_t *dev, uint8_t data)
{
    spi_select(dev->cs);
    ssd1803_send(SYNC_DATA, data);
    spi_deselect(dev->cs);
}

/* =========================================================================
 * Public API
 * ========================================================================= */

void dogs164_init(dogs164_t *dev, spi_cs_t cs, dogs164_view_t view)
{
    dev->cs   = cs;
    dev->view = view;

    /*
     * Configure the SPI peripheral.
     *   Mode 3 (CPOL=1, CPHA=1): SSD1803A clocks data on the falling SCK edge,
     *   with SCK idle HIGH — this is the mode the DOGS164-A datasheet specifies.
     *   DIV32 @ F_CPU 20 MHz → 625 kHz, comfortably below the 1 MHz SPI maximum.
     */
    spi_init(SPI_MODE_3, SPI_CLK_DIV32);

    /*
     * Power-on reset delay.
     * RST is pulled HIGH passively; no MCU control.  The SSD1803A internal
     * reset circuit needs at least ~1 ms after VDD reaches operating voltage,
     * but 50 ms is used here to cover slow power-rail rise times and to allow
     * the internal oscillator and voltage pump to stabilise.
     */
    _delay_ms(50);

    /* ------------------------------------------------------------------
     * Initialisation sequence — EA DOGS164-A datasheet, page 4.
     * All commands are single-byte unless noted otherwise.
     * ------------------------------------------------------------------ */

    /* Step 1: Enter extended instruction set RE=1.
     *   0x3A = Function Set: DL=1 (8-bit), N=1, DH=0, RE=1, REV=0         */
    write_cmd(dev, 0x3Au);

    /* Step 2: Configure 4-line mode and cursor movement direction.
     *   0x09 = Extended Function Set (RE=1): FW=0, B/W=0, NW=1 → 4 lines
     *   0x05 = Entry Mode Set: BDC=0, BDS=1 → top view (12:00, 180° rotation)
     *   0x06 = Entry Mode Set: BDC=1, BDS=0 → bottom view (6:00, default)   */
    write_cmd(dev, 0x09u);
    write_cmd(dev, (view == DOGS164_TOP_VIEW) ? 0x05u : 0x06u);

    /* Step 3: Bias and internal oscillator setup (IS=1 register bank).
     *   0x1E = Bias Setting:   BS1=1 (bias 1/6, used with BS0 below)
     *   0x39 = Function Set:   RE=0, IS=1  (switch to IS=1 commands)
     *   0x1B = Internal OSC:   BS0=1 → 1/6 bias; F2:F0=011 → 183 Hz frame rate */
    write_cmd(dev, 0x1Eu);
    write_cmd(dev, 0x39u);
    write_cmd(dev, 0x1Bu);

    /* Step 4: Enable the internal voltage follower.
     *   0x6C = Follower Control: FON=1 (follower on), RAB=100
     *   The follower requires up to 200 ms to reach steady-state output.
     *   Powering the booster before this delay causes contrast instability. */
    write_cmd(dev, 0x6Eu);
    _delay_ms(200);

    /* Step 5: Enable booster and set default contrast (C5:C0 = 42 = 0b101010).
     *   0x56 = Power/Icon/Contrast: Bon=1, Ion=0, C5=1, C4=0
     *   0x7A = Contrast Set:        C3=1,  C2=0,  C1=1, C0=0               */
    write_cmd(dev, 0x57u);
    write_cmd(dev, 0x72u);

    /* Step 6: Return to base instruction set and turn the display on.
     *   0x38 = Function Set: RE=0, IS=0
     *   0x0F = Display On/Off: D=1 (on), C=1 (cursor), B=1 (blink)         */
    write_cmd(dev, 0x38u);
    write_cmd(dev, 0x0Fu);

    /* Step 7: Select Western European character ROM (ROM B — most useful for
     *          English text with accented characters).                       */
    dogs164_select_rom(dev, DOGS164_ROM_B);

    /* Step 8: Clear screen and return cursor to (0, 0). */
    dogs164_clear(dev);
}

/* ------------------------------------------------------------------------- */

void dogs164_clear(dogs164_t *dev)
{
    write_cmd(dev, 0x01u);  /* Clear Display — fills DDRAM with 0x20 (space) */
    _delay_ms(7);           /* SSD1803A execution time ≈ 6.2 ms; add margin  */
}

/* ------------------------------------------------------------------------- */

void dogs164_home(dogs164_t *dev)
{
    write_cmd(dev, 0x02u);  /* Return Home — cursor to (0,0), no DDRAM change */
    _delay_us(100);
}

/* ------------------------------------------------------------------------- */
void dogs164_set_cursor(dogs164_t *dev, uint8_t row, uint8_t col)
{
    if (row >= DOGS164_ROWS || col >= DOGS164_COLS) {
        return;
    }

    const uint8_t *base = (dev->view == DOGS164_TOP_VIEW)
                          ? ROW_ADDR_TOP
                          : ROW_ADDR_BOTTOM;

    /* TOP_VIEW: col 0 is the physical left edge, which is the HIGH end of the
     * DDRAM range (0x13 for row 0).  BDC=0 then decrements through the visible
     * window (0x13 → 0x04) as characters are written left-to-right.           */
    uint8_t addr = (dev->view == DOGS164_TOP_VIEW)
                   ? base[row] + (DOGS164_COLS - 1u - col)
                   : base[row] + col;

    write_cmd(dev, 0x80u | addr);
}

/* ------------------------------------------------------------------------- */

void dogs164_write_char(dogs164_t *dev, char c)
{
    write_data(dev, (uint8_t)c);
}

/* ------------------------------------------------------------------------- */

void dogs164_write_str(dogs164_t *dev, const char *s)
{
    while (*s) {
        write_data(dev, (uint8_t)*s);
        s++;
    }
}

/* ------------------------------------------------------------------------- */

void dogs164_display_ctrl(dogs164_t *dev,
                          bool display_on,
                          bool cursor_on,
                          bool blink_on)
{
    uint8_t cmd = 0x08u;                     /* Display On/Off base opcode   */
    if (display_on) cmd |= 0x04u;            /* D bit                        */
    if (cursor_on)  cmd |= 0x02u;            /* C bit                        */
    if (blink_on)   cmd |= 0x01u;            /* B bit                        */
    write_cmd(dev, cmd);
}

/* ------------------------------------------------------------------------- */

void dogs164_set_contrast(dogs164_t *dev, uint8_t contrast)
{
    if (contrast > DOGS164_CONTRAST_MAX) {
        contrast = DOGS164_CONTRAST_MAX;
    }

    /*
     * The SSD1803A distributes the 6-bit contrast value C5:C0 across two
     * commands in the IS=1 register bank:
     *
     *   Power/Icon/Contrast (0x54 base):
     *     bit 1 = C5,  bit 0 = C4
     *     bit 2 = Bon (booster enable — keep ON)
     *     → byte = 0x54 | (C5:C4)
     *
     *   Contrast Set (0x70 base):
     *     bits 3:0 = C3:C0
     *     → byte = 0x70 | (C3:C0)
     */
    write_cmd(dev, 0x39u);                                  /* IS=1           */
    write_cmd(dev, 0x54u | ((contrast >> 4u) & 0x03u));    /* Bon, C5:C4     */
    write_cmd(dev, 0x70u | (contrast & 0x0Fu));             /* C3:C0          */
    write_cmd(dev, 0x38u);                                  /* IS=0           */
}

/* ------------------------------------------------------------------------- */

void dogs164_select_rom(dogs164_t *dev, dogs164_rom_t rom)
{
    if ((uint8_t)rom > 2u) {
        rom = DOGS164_ROM_A;
    }

    /*
     * ROM selection is a two-byte extended command (SSD1803A datasheet p.50).
     * It must be issued with RE=1.  The second byte is sent as data (RS=1).
     *
     *   Byte 1 (command): 0x72 — ROM Selection command
     *   Byte 2 (data):    ROM select byte
     *     ROM A → 0x00,  ROM B → 0x04,  ROM C → 0x0C
     */
    write_cmd(dev,  0x3Au);                  /* Function Set: RE=1            */
    write_cmd(dev,  0x72u);                  /* ROM Selection (first byte)    */
    write_data(dev, ROM_SEL_BYTE[(uint8_t)rom]); /* ROM select (second byte)  */
    write_cmd(dev,  0x38u);                  /* Function Set: RE=0            */
}

/* ------------------------------------------------------------------------- */

void dogs164_define_char(dogs164_t *dev,
                         uint8_t slot,
                         const uint8_t pattern[8])
{
    if (slot > 7u) {
        return;
    }

    /*
     * CGRAM addressing:
     *   Set CGRAM Address command = 0x40 | (slot << 3 | row)
     *   Writing to CGRAM with the address set to the start of a slot and then
     *   issuing 8 sequential data writes fills all 8 pixel rows automatically
     *   (the address counter auto-increments through CGRAM).
     *
     * Requires: IS=0, RE=0 (base instruction set — already active).
     */
    write_cmd(dev, 0x40u | ((slot & 0x07u) << 3u));  /* Set CGRAM address    */

    for (uint8_t row = 0u; row < 8u; row++) {
        write_data(dev, pattern[row] & 0x1Fu);        /* Only bits 4:0 used   */
    }

    /*
     * Return to DDRAM mode by re-issuing the last DDRAM address.
     * A simple home command is used here; the caller should call
     * dogs164_set_cursor() to reposition after defining characters.
     */
    write_cmd(dev, 0x80u);  /* Set DDRAM address = 0 (top-left)               */
}
