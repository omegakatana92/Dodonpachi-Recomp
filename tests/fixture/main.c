/**
 *    Name:        main.c (fixture)
 *    Description: ROM-free CI runner for the board runtime and frame-wait contract
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/video.h"
#include "ddp/rom.h"

static uint32_t last_hash;

static void frame_callback(cave_machine *machine)
{
	uint32_t hash = 2166136261u;
	size_t i;
	for (i = 0; i < sizeof(machine->work_ram); ++i)
	{
		hash ^= machine->work_ram[i];
		hash *= 16777619u;
	}
	last_hash = hash;
}

int main(void)
{
	ddp_rom_set rom;
	int frames;
	int result;

	memset(&rom, 0, sizeof(rom));
	rom.program = (uint8_t *)calloc(1, DDP_PROGRAM_SIZE);
	if (!rom.program)
		return 2;
	/* The fixture reset vectors: SSP 0x110000, PC 0x001000. */
	rom.program[0] = 0x00; rom.program[1] = 0x11; rom.program[2] = 0x00; rom.program[3] = 0x00;
	rom.program[4] = 0x00; rom.program[5] = 0x00; rom.program[6] = 0x10; rom.program[7] = 0x00;

	cave_machine_init(&ddp_machine, &rom);
	ddp_program_set_frame_callback(frame_callback);
	frames = ddp_program_run(&ddp_machine, 60);

	printf("fixture frames: %d\n", frames);
	printf("fixture hash:   %08X\n", last_hash);
	printf("unmapped reads: %u writes: %u\n", ddp_machine.invalid_reads, ddp_machine.invalid_writes);

	result = (!ddp_program_faulted() && frames == 60 &&
	          ddp_machine.invalid_reads == 0 && ddp_machine.invalid_writes == 0) ? 0 : 1;
	free(rom.program);
	return result;
}
