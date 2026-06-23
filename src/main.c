/*
 * main.c
 * ATtiny3227 Curiosity Nano (EV58A83A)
 *
 * Demonstrates the EA DOGS164-A 4×16 LCD driver (SSD1803A controller).
 *
 * Clock: internal 20 MHz oscillator / 6 prescaler = ~3.33 MHz (factory default).
 *
 * SPI clock check:
 *   dogs164_init() uses SPI_CLK_DIV32 → 3,333,333 / 32 ≈ 104 kHz,
 *   which is well within the DOGS164-A's 1 MHz SPI maximum.
 *
 * Pin usage:
 *   PA1  MOSI  ──► SID  (display serial data in)
 *   PA3  SCK   ──► SCLK (display serial clock)
 *   PA4  CS0   ──► CS   (display chip select, active-low)
 *   PB7  LED0       active-low, blinks to show main loop is running
 */

#include <avr/io.h>
#include <util/delay.h>
#include "spi.h"
#include "dogs164.h"
#define LED_PORT  PORTB
#define LED_PIN   PIN7_bm
int main(void)
{
    LED_PORT.DIRSET = LED_PIN;
    LED_PORT.OUTSET = LED_PIN;

    dogs164_t lcd;
    dogs164_init(&lcd, SPI_CS0, DOGS164_BOTTOM_VIEW);

    dogs164_set_cursor(&lcd, 0, 0);
    dogs164_write_str(&lcd, "LINE ONE        ");

    dogs164_set_cursor(&lcd, 1, 0);
    dogs164_write_str(&lcd, "LINE TWO        ");

    dogs164_set_cursor(&lcd, 2, 0);
    dogs164_write_str(&lcd, "LINE THREE      ");

    dogs164_set_cursor(&lcd, 3, 0);
    dogs164_write_str(&lcd, "LINE FOUR       ");

    dogs164_display_ctrl(&lcd, true, false, false);

    while (1) {
        LED_PORT.OUTTGL = LED_PIN;
        _delay_ms(500);
    }
}
