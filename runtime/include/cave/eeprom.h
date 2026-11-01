/**
 *    Name:        eeprom.h
 *    Description: the 93C46 serial EEPROM on the Cave board (64 x 16-bit words, 128 bytes)
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    Follows MAME's eeprom_serial_93cxx in 16-bit mode: chip select, clock and data in on the
 *    write latch at 0xe00000; data out on IN1 bit 11.
 */
#ifndef CAVE_EEPROM_H
#define CAVE_EEPROM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	uint16_t words[64];
	uint8_t image[128];

	int cs;
	int clk;
	int di;
	int do_out;

	/* instruction receiver */
	int state;
	int opcode;
	int bits;
	int address;
	uint16_t value;
	int read_bits;
} cave_eeprom;

void cave_eeprom_init(cave_eeprom *eeprom, const uint8_t image[128]);
void cave_eeprom_write(cave_eeprom *eeprom, uint8_t value);
int cave_eeprom_do(const cave_eeprom *eeprom);
void cave_eeprom_save_image(const cave_eeprom *eeprom, uint8_t image[128]);

#ifdef __cplusplus
}
#endif

#endif
