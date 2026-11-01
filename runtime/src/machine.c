/**
 *    Name:        machine.c
 *    Description: the Cave first-generation board model and the 68000 bus
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    Address decoding follows MAME's cave_state::ddonpach_map. The board is 16 bits wide; byte
 *    accesses use mem_mask lanes (0xff00 even, 0x00ff odd), which is how the generated C issues
 *    them. Unmapped accesses are counted and reported once; they are not silently invented.
 */
#include "cave/machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cave/program.h"
#include "m68000/cpu.h"

cave_machine ddp_machine;

static void invalid_read(cave_machine *machine, uint32_t address)
{
	machine->invalid_reads++;
	if (!machine->invalid_reported)
	{
		machine->invalid_first_address = address;
		machine->invalid_reported = 1;
		fprintf(stderr, "ddonpach: unmapped read at %06X (pc %06X)\n",
		        address & 0xFFFFFFu, cpu.pc);
	}
}

static void invalid_write(cave_machine *machine, uint32_t address, uint32_t value)
{
	/* The 68000 bus has no write fault: an address that decodes to no device is simply
	   ignored by the board. Keep a count and an optional debug line, but do not fail the
	   run (MAME also ignores these writes). */
	static int trace = -1;
	machine->ignored_writes++;
	if (trace < 0)
		trace = getenv("DDP_BUS_TRACE") ? 1 : 0;
	if (trace && !machine->invalid_reported)
	{
		machine->invalid_reported = 1;
		machine->invalid_first_address = address;
		fprintf(stderr, "ddonpach: open-bus write at %06X = %04X (pc %06X, ignored)\n",
		        address & 0xFFFFFFu, value & 0xFFFFu, cpu.pc);
	}
}

static void eeprom_latch(cave_machine *machine, uint8_t value)
{
	static int trace = -1;
	if (trace < 0)
		trace = getenv("DDP_EEPROM_TRACE") ? 1 : 0;
	if (trace)
		fprintf(stderr, "EE W %02X pc=%06X\n", value, cpu.pc);
	cave_eeprom_write(&machine->eeprom, value);
}

static uint16_t irq_cause_read(cave_machine *machine, unsigned word_offset)
{
	uint16_t result = 0x0003;

	if (machine->vblank_pending)
		result ^= 0x0001;
	/* MAME's agallet bit: zero while the VBLANK interval is active */
	if (machine->vblank_active)
		result &= (uint16_t)~0x0004u;
	else
		result |= 0x0004;

	if (word_offset == 2 || word_offset == 3)
	{
		machine->vblank_pending = 0;
		machine->irq_level = 0;
	}
	return result;
}

void cave_machine_init(cave_machine *machine, const ddp_rom_set *rom)
{
	memset(machine, 0, sizeof(*machine));
	machine->rom = rom;
	cave_ymz280b_init(&machine->ymz, rom ? rom->samples : NULL,
	                  rom ? DDP_SAMPLE_SIZE : 0, 44100);
	if (rom)
		cave_eeprom_init(&machine->eeprom, rom->eeprom);
	else
	{
		uint8_t blank[128];
		memset(blank, 0xFF, sizeof(blank));
		cave_eeprom_init(&machine->eeprom, blank);
	}
	cave_machine_reset(machine);
}

void cave_machine_reset(cave_machine *machine)
{
	memset(machine->work_ram, 0, sizeof(machine->work_ram));
	memset(machine->sprite_ram, 0, sizeof(machine->sprite_ram));
	memset(machine->layer_ram, 0, sizeof(machine->layer_ram));
	memset(machine->palette, 0, sizeof(machine->palette));
	memset(machine->video_regs, 0, sizeof(machine->video_regs));
	memset(machine->layer_regs, 0, sizeof(machine->layer_regs));
	memset(machine->framebuffer, 0, sizeof(machine->framebuffer));

	machine->in0 = 0xFFFF;
	machine->in1 = 0xFFFF;
	machine->in0_latch = 0xFFFF;
	machine->in1_latch = 0xFFFF;
	machine->coin_counter[0] = machine->coin_counter[1] = 0;
	machine->coin_lockout[0] = machine->coin_lockout[1] = 0;

	machine->vblank_pending = 0;
	machine->vblank_active = 0;
	machine->irq_level = 0;
	machine->frame = 0;
	machine->frames_run = 0;
	machine->palette_writes = 0;
	machine->layer_writes = 0;
	machine->sprite_writes = 0;
	machine->invalid_reads = 0;
	machine->invalid_writes = 0;
	machine->ignored_writes = 0;
	machine->invalid_first_address = 0;
	machine->invalid_reported = 0;
	machine->video_dirty = 1;

	memset(machine->ymz.regs, 0, sizeof(machine->ymz.regs));
	machine->ymz.address = 0;
}

uint32_t cave_bus_read8(cave_machine *machine, uint32_t address)
{
	address &= 0xFFFFFFu;

	if (address < DDP_PROGRAM_SIZE)
		return machine->rom->program[address];
	if (address >= 0x100000u && address < 0x110000u)
		return machine->work_ram[address - 0x100000u];
	if (address >= 0x400000u && address < 0x410000u)
		return machine->sprite_ram[address - 0x400000u];
	if (address >= 0x500000u && address < 0x508000u)
		return machine->layer_ram[0][address - 0x500000u];
	if (address >= 0x600000u && address < 0x608000u)
		return machine->layer_ram[1][address - 0x600000u];
	if (address >= 0x700000u && address < 0x710000u)
		return machine->layer_ram[2][(address - 0x700000u) & 0x3FFFu];

	/* Everything else is word-addressed: take the requested lane. */
	{
		uint32_t word = cave_bus_read16(machine, address & ~1u);
		return (address & 1u) ? (word & 0xFFu) : ((word >> 8) & 0xFFu);
	}
}

uint32_t cave_bus_read16(cave_machine *machine, uint32_t address)
{
	address &= 0xFFFFFEu;

	if (address < DDP_PROGRAM_SIZE)
		return ((uint32_t)machine->rom->program[address] << 8) |
		       machine->rom->program[address + 1];
	if (address >= 0x100000u && address < 0x110000u)
		return ((uint32_t)machine->work_ram[address - 0x100000u] << 8) |
		       machine->work_ram[address - 0x100000u + 1];
	if (address >= 0x400000u && address < 0x410000u)
		return ((uint32_t)machine->sprite_ram[address - 0x400000u] << 8) |
		       machine->sprite_ram[address - 0x400000u + 1];
	if (address >= 0x500000u && address < 0x508000u)
		return ((uint32_t)machine->layer_ram[0][address - 0x500000u] << 8) |
		       machine->layer_ram[0][address - 0x500000u + 1];
	if (address >= 0x600000u && address < 0x608000u)
		return ((uint32_t)machine->layer_ram[1][address - 0x600000u] << 8) |
		       machine->layer_ram[1][address - 0x600000u + 1];
	if (address >= 0x700000u && address < 0x710000u)
	{
		uint32_t offset = (address - 0x700000u) & 0x3FFFu;
		return ((uint32_t)machine->layer_ram[2][offset] << 8) |
		       machine->layer_ram[2][offset + 1];
	}

	if (address >= 0x300000u && address < 0x300004u)
		return cave_ymz280b_read(&machine->ymz, (address - 0x300000u) >> 1);
	if (address >= 0x800000u && address < 0x800008u)
		return irq_cause_read(machine, (address - 0x800000u) >> 1);
	if (address >= 0x800008u && address < 0x800080u)
		return 0;
	if (address >= 0x900000u && address < 0x900006u)
		return machine->layer_regs[0][(address - 0x900000u) >> 1];
	if (address >= 0xA00000u && address < 0xA00006u)
		return machine->layer_regs[1][(address - 0xA00000u) >> 1];
	if (address >= 0xB00000u && address < 0xB00006u)
		return machine->layer_regs[2][(address - 0xB00000u) >> 1];
	if (address >= 0xC00000u && address < 0xC10000u)
		return machine->palette[(address - 0xC00000u) >> 1];
	if (address >= 0xD00000u && address < 0xD00002u)
		return machine->in0;
	if (address >= 0xD00002u && address < 0xD00004u)
	{
		uint16_t value = (uint16_t)(machine->in1 & (uint16_t)~0x0800u);
		if (cave_eeprom_do(&machine->eeprom))
			value |= 0x0800u;
		return value;
	}
	if (address >= 0xE00000u && address < 0xE00002u)
		return 0x00FFu;

	invalid_read(machine, address);
	return 0xFFFFu;
}

uint32_t cave_bus_read32(cave_machine *machine, uint32_t address)
{
	uint32_t high = cave_bus_read16(machine, address);
	uint32_t low = cave_bus_read16(machine, address + 2);
	return (high << 16) | low;
}

void cave_bus_write8(cave_machine *machine, uint32_t address, uint32_t value)
{
	address &= 0xFFFFFFu;
	value &= 0xFFu;

	if (address < DDP_PROGRAM_SIZE)
		return;                         /* ROM: writes are ignored by the board */
	if (address >= 0x100000u && address < 0x110000u)
	{
		machine->work_ram[address - 0x100000u] = (uint8_t)value;
		return;
	}
	if (address >= 0x400000u && address < 0x410000u)
	{
		machine->sprite_ram[address - 0x400000u] = (uint8_t)value;
		return;
	}
	if (address >= 0x500000u && address < 0x508000u)
	{
		machine->layer_ram[0][address - 0x500000u] = (uint8_t)value;
		return;
	}
	if (address >= 0x600000u && address < 0x608000u)
	{
		machine->layer_ram[1][address - 0x600000u] = (uint8_t)value;
		return;
	}
	if (address >= 0x700000u && address < 0x710000u)
	{
		machine->layer_ram[2][(address - 0x700000u) & 0x3FFFu] = (uint8_t)value;
		return;
	}
	if (address >= 0x300000u && address < 0x300004u)
	{
		if (address & 1u)
			cave_ymz280b_write(&machine->ymz, (address - 0x300000u) >> 1, (uint8_t)value);
		return;
	}
	if (address >= 0xE00000u && address < 0xE00002u)
	{
		if ((address & 1u) == 0)
			eeprom_latch(machine, (uint8_t)value);
		return;
	}
	if (address >= 0x800000u && address < 0x800080u)
	{
		unsigned index = (address - 0x800000u) >> 1;
		uint16_t old = machine->video_regs[index];
		machine->video_regs[index] = (address & 1u)
			? (uint16_t)((old & 0xFF00u) | value)
			: (uint16_t)((old & 0x00FFu) | (value << 8));
		machine->video_dirty = 1;
		return;
	}
	if ((address >= 0x900000u && address < 0x900006u) ||
	    (address >= 0xA00000u && address < 0xA00006u) ||
	    (address >= 0xB00000u && address < 0xB00006u))
	{
		unsigned layer = address >= 0xB00000u ? 2u : (address >= 0xA00000u ? 1u : 0u);
		unsigned index = (address - (0x900000u + layer * 0x100000u)) >> 1;
		uint16_t old = machine->layer_regs[layer][index];
		machine->layer_regs[layer][index] = (address & 1u)
			? (uint16_t)((old & 0xFF00u) | value)
			: (uint16_t)((old & 0x00FFu) | (value << 8));
		machine->video_dirty = 1;
		return;
	}
	if (address >= 0xC00000u && address < 0xC10000u)
	{
		unsigned index = (address - 0xC00000u) >> 1;
		uint16_t old = machine->palette[index];
		machine->palette[index] = (address & 1u)
			? (uint16_t)((old & 0xFF00u) | value)
			: (uint16_t)((old & 0x00FFu) | (value << 8));
		machine->video_dirty = 1;
		return;
	}
	if (address >= 0xD00000u && address < 0xD00004u)
		return;                         /* inputs are read-only on this board */

	invalid_write(machine, address, value);
}

void cave_bus_write16(cave_machine *machine, uint32_t address, uint32_t value)
{
	address &= 0xFFFFFEu;
	value &= 0xFFFFu;

	if (address < DDP_PROGRAM_SIZE)
		return;
	if (address >= 0x100000u && address < 0x110000u)
	{
		machine->work_ram[address - 0x100000u] = (uint8_t)(value >> 8);
		machine->work_ram[address - 0x100000u + 1] = (uint8_t)value;
		return;
	}
	if (address >= 0x400000u && address < 0x410000u)
	{
		machine->sprite_ram[address - 0x400000u] = (uint8_t)(value >> 8);
		machine->sprite_ram[address - 0x400000u + 1] = (uint8_t)value;
		machine->sprite_writes++;
		return;
	}
	if (address >= 0x500000u && address < 0x508000u)
	{
		machine->layer_ram[0][address - 0x500000u] = (uint8_t)(value >> 8);
		machine->layer_ram[0][address - 0x500000u + 1] = (uint8_t)value;
		machine->layer_writes++;
		return;
	}
	if (address >= 0x600000u && address < 0x608000u)
	{
		machine->layer_ram[1][address - 0x600000u] = (uint8_t)(value >> 8);
		machine->layer_ram[1][address - 0x600000u + 1] = (uint8_t)value;
		machine->layer_writes++;
		return;
	}
	if (address >= 0x700000u && address < 0x710000u)
	{
		uint32_t offset = (address - 0x700000u) & 0x3FFFu;
		machine->layer_ram[2][offset] = (uint8_t)(value >> 8);
		machine->layer_ram[2][offset + 1] = (uint8_t)value;
		machine->layer_writes++;
		return;
	}
	if (address >= 0x300000u && address < 0x300004u)
	{
		cave_ymz280b_write(&machine->ymz, (address - 0x300000u) >> 1, (uint8_t)value);
		return;
	}
	if (address >= 0x800000u && address < 0x800080u)
	{
		machine->video_regs[(address - 0x800000u) >> 1] = (uint16_t)value;
		machine->video_dirty = 1;
		return;
	}
	if ((address >= 0x900000u && address < 0x900006u) ||
	    (address >= 0xA00000u && address < 0xA00006u) ||
	    (address >= 0xB00000u && address < 0xB00006u))
	{
		unsigned layer = address >= 0xB00000u ? 2u : (address >= 0xA00000u ? 1u : 0u);
		machine->layer_regs[layer][(address - (0x900000u + layer * 0x100000u)) >> 1] = (uint16_t)value;
		machine->video_dirty = 1;
		return;
	}
	if (address >= 0xC00000u && address < 0xC10000u)
	{
		machine->palette[(address - 0xC00000u) >> 1] = (uint16_t)value;
		machine->palette_writes++;
		machine->video_dirty = 1;
		return;
	}
	if (address >= 0xE00000u && address < 0xE00002u)
	{
		/* The 93C46 latch sits on the high byte of the 16-bit write; MAME's eeprom_w(u8)
		   receives that byte (the game writes 0x0200/0x0600/0x0a00/0x0e00 to clock it). */
		if ((address & 1u) == 0)
			eeprom_latch(machine, (uint8_t)(value >> 8));
		return;
	}
	if (address >= 0xD00000u && address < 0xD00004u)
		return;

	invalid_write(machine, address, value);
}

void cave_bus_write32(cave_machine *machine, uint32_t address, uint32_t value)
{
	cave_bus_write16(machine, address, value >> 16);
	cave_bus_write16(machine, address + 2, value);
}

void cave_machine_frame(cave_machine *machine)
{
	machine->frame++;
	machine->frames_run++;
	machine->vblank_active = 1;
	machine->vblank_pending = 1;
	machine->irq_level = 1;
	machine->video_dirty = 1;
}

void cave_machine_ack_vblank(cave_machine *machine)
{
	machine->vblank_pending = 0;
	machine->irq_level = 0;
}

int cave_machine_irq_level(const cave_machine *machine)
{
	return machine->irq_level;
}

void cave_machine_clear_irq(cave_machine *machine)
{
	machine->irq_level = 0;
}

void cave_machine_set_inputs(cave_machine *machine, uint16_t in0, uint16_t in1)
{
	machine->in0 = in0;
	machine->in1 = in1;
}
