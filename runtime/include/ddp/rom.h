/**
 *    Name:        rom.h
 *    Description: the player's ddonpach ROM set, loaded from a .zip or a folder and checked by CRC32
 *                 and SHA-1 before any region is built
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    No ROM bytes are part of this repository. The executable reads the archive the player supplies and
 *    keeps the regions in private buffers for the lifetime of the process.
 */
#ifndef DDP_ROM_H
#define DDP_ROM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DDP_PROGRAM_SIZE  0x100000u
#define DDP_SPRITE_SIZE   0x800000u
#define DDP_LAYER_SIZE    0x200000u
#define DDP_SAMPLE_SIZE   0x400000u
#define DDP_EEPROM_SIZE   0x80u

typedef struct
{
	uint8_t *program;              /* 1 MiB, big endian as the 68000 sees it */
	uint8_t *sprites;              /* 8 MiB, file byte order */
	uint8_t *layers[3];            /* 2 MiB each, file byte order */
	uint8_t *samples;              /* 4 MiB, YMZ280B sample data */
	uint8_t eeprom[DDP_EEPROM_SIZE];
} ddp_rom_set;

/* Load and verify a .zip or a folder. Returns 0 on success; on failure writes a human-readable
   reason to 'error' and leaves 'set' empty. */
int ddp_rom_load(const char *path, ddp_rom_set *set, char *error, size_t error_size);
void ddp_rom_free(ddp_rom_set *set);

/* The reset vector and the program image the analysis and runtime use. */
uint32_t ddp_rom_reset_pc(const ddp_rom_set *set);
uint32_t ddp_rom_reset_sp(const ddp_rom_set *set);

#ifdef __cplusplus
}
#endif

#endif
