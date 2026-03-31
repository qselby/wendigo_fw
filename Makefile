# -----------------------------------------------------------------------
# Makefile — ATtiny3227 Curiosity Nano (EV58A83A) blinky
#
# Toolchain: avr-gcc, avr-objcopy, avr-size, avrdude
#
# Install on Debian/Ubuntu:
#   sudo apt install gcc-avr binutils-avr avr-libc avrdude
#
# Usage:
#   make          — build blinky.hex
#   make flash    — build and upload via the onboard nEDBG
#   make clean    — remove build artefacts
# -----------------------------------------------------------------------

# Target device.  Change to attiny3226 if you have that chip instead.
MCU    = attiny3227

# Default factory clock: 20 MHz internal oscillator / 6 prescaler.
F_CPU  = 3333333UL

TARGET = blinky
SRC    = main.c

# --- Toolchain -----------------------------------------------------------
CC      = avr-gcc
OBJCOPY = avr-objcopy
SIZE    = avr-size
AVRDUDE = avrdude

CFLAGS  = -mmcu=$(MCU) -DF_CPU=$(F_CPU) \
          -Os -std=c11 -Wall -Wextra \
          -ffunction-sections -fdata-sections
LDFLAGS = -mmcu=$(MCU) -Wl,--gc-sections

# --- Programmer ----------------------------------------------------------
# The Curiosity Nano's onboard nEDBG appears to avrdude as pkobn_updi.
# Port "usb" lets avrdude auto-detect the USB HID device; no /dev/tty needed.
PROGRAMMER = pkobn_updi
PORT       = usb

# -------------------------------------------------------------------------
.PHONY: all flash clean

all: $(TARGET).hex

# Compile
$(TARGET).elf: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Convert to Intel HEX
$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	@echo ""
	@$(SIZE) --format=avr --mcu=$(MCU) $<

# Flash via onboard debugger
flash: $(TARGET).hex
	$(AVRDUDE) -c $(PROGRAMMER) -p $(MCU) -P $(PORT) \
	           -U flash:w:$(TARGET).hex:i

# Drag-and-drop alternative (no avrdude needed):
#   Copy blinky.hex to the CURIOSITY drive that appears when you plug in the board.

clean:
	rm -f $(TARGET).elf $(TARGET).hex
