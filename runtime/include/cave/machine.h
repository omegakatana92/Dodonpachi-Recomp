/**
 *    Name:        machine.h
 *    Description: the Cave first-generation board: work RAM, video memories, palette, tile and
 *                 sprite registers, inputs, EEPROM and the YMZ280B, behind the 68000 bus
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    The map follows MAME's cave_state::ddonpach_map (src/mame/atlus/cave.cpp, BSD-3-Clause):
 *      000000-0fffff ROM       100000-10ffff work RAM   300000-300003 YMZ280B
 *      400000-40ffff sprites   500000-507fff layer 0   600000-607fff layer 1
 *      700000-703fff layer 2 (mirrored every 16 KiB)
 *      800000-80007f sprite/video registers (write) and IRQ cause (read)
 *      900000/a00000/b00000 layer controls   c00000-c0ffff palette
 *      d00000/d00002 inputs + EEPROM data out   e00000 EEPROM latch
 */
#ifndef CAVE_MACHINE_H
#define CAVE_MACHINE_H

#include <stddef.h>
#include <stdint.h>

#include "cave/eeprom.h"
#include "cave/frame.h"
#include "cave/ymz280b.h"
#include "ddp/rom.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DDP_WORK_RAM_SIZE      0x10000u
#define DDP_SPRITE_RAM_SIZE    0x10000u
#define DDP_LAYER0_RAM_SIZE    0x8000u
#define DDP_LAYER1_RAM_SIZE    0x8000u
#define DDP_LAYER2_RAM_SIZE    0x4000u
#define DDP_PALETTE_ENTRIES    0x8000u
#define DDP_VIDEO_REG_WORDS    0x40u
#define DDP_LAYER_REG_WORDS    3u
#define DDP_FRAMEBUFFER_PIXELS (DDP_SCREEN_WIDTH * DDP_SCREEN_HEIGHT)

typedef struct
{
	const ddp_rom_set *rom;

	uint8_t work_ram[DDP_WORK_RAM_SIZE];
	uint8_t sprite_ram[DDP_SPRITE_RAM_SIZE];
	uint8_t layer_ram[3][DDP_LAYER0_RAM_SIZE];
	uint16_t palette[DDP_PALETTE_ENTRIES];
	uint16_t video_regs[DDP_VIDEO_REG_WORDS];
	uint16_t layer_regs[3][DDP_LAYER_REG_WORDS];

	cave_ymz280b ymz;
	cave_eeprom eeprom;

	uint16_t in0;
	uint16_t in1;
	uint16_t in0_latch;
	uint16_t in1_latch;

	unsigned coin_counter[2];
	unsigned coin_lockout[2];

	int vblank_pending;
	int vblank_active;
	int irq_level;
	uint32_t frame;
	uint32_t frames_run;

	uint32_t palette_writes;
	uint32_t layer_writes;
	uint32_t sprite_writes;

	uint32_t invalid_reads;
	uint32_t invalid_writes;
	uint32_t ignored_writes;
	uint32_t invalid_first_address;
	int invalid_reported;

	uint32_t framebuffer[DDP_FRAMEBUFFER_PIXELS];
	uint32_t video_drawn_pixels;
	int video_dirty;
} cave_machine;

/* One board per process: the translated program and the ports share it. */
extern cave_machine ddp_machine;

void cave_machine_init(cave_machine *machine, const ddp_rom_set *rom);
void cave_machine_reset(cave_machine *machine);

uint32_t cave_bus_read8(cave_machine *machine, uint32_t address);
uint32_t cave_bus_read16(cave_machine *machine, uint32_t address);
uint32_t cave_bus_read32(cave_machine *machine, uint32_t address);
void cave_bus_write8(cave_machine *machine, uint32_t address, uint32_t value);
void cave_bus_write16(cave_machine *machine, uint32_t address, uint32_t value);
void cave_bus_write32(cave_machine *machine, uint32_t address, uint32_t value);

/* The board's VBLANK: called where the game's main loop idles. */
void cave_machine_frame(cave_machine *machine);
void cave_machine_ack_vblank(cave_machine *machine);
int cave_machine_irq_level(const cave_machine *machine);
void cave_machine_clear_irq(cave_machine *machine);

void cave_machine_set_inputs(cave_machine *machine, uint16_t in0, uint16_t in1);

#ifdef __cplusplus
}
#endif

#endif
