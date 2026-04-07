/*
 * blinky/main.c
 * ATtiny3227 Curiosity Nano (EV58A83A)
 *
 * Blinks LED0 (PB7, active-low) at 1 Hz.
 *
 * Default clock: internal 20 MHz oscillator / 6 prescaler = ~3.33 MHz.
 * No fuse changes needed — this is factory default.
 */

#define F_CPU 3333333UL

#include <avr/io.h>
#include <util/delay.h>

/* LED0 on the Curiosity Nano is wired to PB7, driven active-low. */
#define LED_PORT  PORTB
#define LED_PIN   PIN7_bm

int main(void)
{
    /* Configure PB7 as output. */
    LED_PORT.DIRSET = LED_PIN;

    while (1) {
        LED_PORT.OUTCLR = LED_PIN;   /* drive low  → LED on  */
        _delay_ms(500);
        LED_PORT.OUTSET = LED_PIN;   /* drive high → LED off */
        _delay_ms(500);
    }
}
