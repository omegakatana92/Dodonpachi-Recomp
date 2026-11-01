/**
 *    Name:        main.c
 *    Description: deterministic headless frame runner and CI gate
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    Usage: ddonpach_headless --rom rom/ddonpach.zip --frames 600 --hashes build/boot.hashes
 *    Exit code 0 only when the requested frames ran, no translated code was missing and the bus saw
 *    no unmapped access.
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/video.h"
#include "ddp/rom.h"
#include "m68000/cpu.h"

static FILE *hash_file;
static const char *png_path;
static const char *ram_path;
static const char *state_dir;
static const char *render_png;
static FILE *wav_file;
static uint32_t wav_frames;
static int dump_state;
static int auto_play;
static int stats;
static double render_seconds;
static double hash_seconds;
static int16_t audio_buffer[4096 * 2];
static uint64_t hash_state;

static void hash_bytes(const void *data, size_t size)
{
	const uint8_t *bytes = (const uint8_t *)data;
	size_t i;
	for (i = 0; i < size; ++i)
	{
		hash_state ^= bytes[i];
		hash_state *= UINT64_C(1099511628211);
	}
}

static void write_wav_header(FILE *file, uint32_t frames)
{
	uint32_t rate = 44100;
	uint32_t data_size = frames * 2u * 2u;
	uint32_t riff_size = 36u + data_size;
	uint8_t header[44];
	uint32_t value;

	memcpy(header + 0, "RIFF", 4);
	value = riff_size; memcpy(header + 4, &value, 4);
	memcpy(header + 8, "WAVEfmt ", 8);
	value = 16; memcpy(header + 16, &value, 4);
	value = 1; memcpy(header + 20, &value, 2);
	value = 2; memcpy(header + 22, &value, 2);
	memcpy(header + 24, &rate, 4);
	value = rate * 4u; memcpy(header + 28, &value, 4);
	value = 4; memcpy(header + 32, &value, 2);
	value = 16; memcpy(header + 34, &value, 2);
	memcpy(header + 36, "data", 4);
	memcpy(header + 40, &data_size, 4);
	fseek(file, 0, SEEK_SET);
	fwrite(header, 1, sizeof(header), file);
}

static void frame_callback(cave_machine *machine)
{
	clock_t start;
	clock_t middle;

	if (auto_play)
	{
		uint32_t frame = machine->frames_run;
		uint16_t in0 = 0xFFFFu;
		if (frame >= 300 && frame < 307) in0 &= (uint16_t)~0x0100u; /* coin pulse */
		if (frame >= 360 && frame < 370) in0 &= (uint16_t)~0x0080u; /* start pulse */
		if (frame >= 420) in0 &= (uint16_t)~0x0010u;               /* shot */
		if (((frame / 90) & 1u) != 0) in0 &= (uint16_t)~0x0004u;    /* left */
		else in0 &= (uint16_t)~0x0008u;                             /* right */
		cave_machine_set_inputs(machine, in0, 0xFFFFu);
	}

	start = clock();
	cave_video_render(machine, machine->framebuffer);
	middle = clock();
	render_seconds += (double)(middle - start) / CLOCKS_PER_SEC;

	hash_state = UINT64_C(1469598103934665603);
	hash_bytes(machine->work_ram, sizeof(machine->work_ram));
	hash_bytes(machine->sprite_ram, sizeof(machine->sprite_ram));
	hash_bytes(machine->layer_ram, sizeof(machine->layer_ram));
	hash_bytes(machine->palette, sizeof(machine->palette));
	hash_bytes(machine->video_regs, sizeof(machine->video_regs));
	hash_bytes(machine->layer_regs, sizeof(machine->layer_regs));
	hash_bytes(machine->framebuffer, sizeof(machine->framebuffer));
	hash_seconds += (double)(clock() - middle) / CLOCKS_PER_SEC;

	if (hash_file)
		fprintf(hash_file, "%u %016" PRIx64 "\n", machine->frames_run, hash_state);

	if (wav_file && machine->frames_run > 3000)
	{
		/* One video frame's worth at 44.1 kHz. */
		cave_ymz280b_render(&machine->ymz, audio_buffer, 766);
		fwrite(audio_buffer, sizeof(int16_t), 766 * 2, wav_file);
		wav_frames += 766;
	}
}

static void save_png_at_stop(cave_machine *machine)
{
	if (png_path)
		cave_video_save_png(machine->framebuffer, png_path);
}

static int load_bytes(const char *directory, const char *name, void *destination, size_t size)
{
	char path[1024];
	FILE *file;
	size_t got;
	snprintf(path, sizeof(path), "%s/%s", directory, name);
	file = fopen(path, "rb");
	if (!file)
		return -1;
	got = fread(destination, 1, size, file);
	fclose(file);
	return got == size ? 0 : -1;
}

static int load_words(const char *directory, const char *name, uint16_t *destination, size_t words)
{
	uint8_t *bytes = (uint8_t *)malloc(words * 2u);
	size_t i;
	int result;
	if (!bytes)
		return -1;
	result = load_bytes(directory, name, bytes, words * 2u);
	if (result == 0)
	{
		for (i = 0; i < words; ++i)
			destination[i] = (uint16_t)((bytes[i * 2] << 8) | bytes[i * 2 + 1]);
	}
	free(bytes);
	return result;
}

static int render_saved_state(cave_machine *machine, const char *directory, const char *png)
{
	if (load_bytes(directory, "work.bin", machine->work_ram, sizeof(machine->work_ram)) != 0 ||
	    load_bytes(directory, "sprite.bin", machine->sprite_ram, sizeof(machine->sprite_ram)) != 0 ||
	    load_bytes(directory, "layer0.bin", machine->layer_ram[0], DDP_LAYER0_RAM_SIZE) != 0 ||
	    load_bytes(directory, "layer1.bin", machine->layer_ram[1], DDP_LAYER1_RAM_SIZE) != 0 ||
	    load_bytes(directory, "layer2.bin", machine->layer_ram[2], DDP_LAYER2_RAM_SIZE) != 0 ||
	    load_words(directory, "palette.bin", machine->palette, DDP_PALETTE_ENTRIES) != 0 ||
	    load_words(directory, "videoregs.bin", machine->video_regs, DDP_VIDEO_REG_WORDS) != 0 ||
	    load_words(directory, "layerregs.bin", &machine->layer_regs[0][0], 9) != 0)
		return -1;
	cave_video_render(machine, machine->framebuffer);
	printf("state drawn pixels: %u\n", machine->video_drawn_pixels);
	printf("state layer regs: %04X %04X %04X / %04X %04X %04X / %04X %04X %04X\n",
	       machine->layer_regs[0][0], machine->layer_regs[0][1], machine->layer_regs[0][2],
	       machine->layer_regs[1][0], machine->layer_regs[1][1], machine->layer_regs[1][2],
	       machine->layer_regs[2][0], machine->layer_regs[2][1], machine->layer_regs[2][2]);
	{
		char raw_path[1200];
		FILE *raw;
		size_t i;
		snprintf(raw_path, sizeof(raw_path), "%s.rgba", png);
		raw = fopen(raw_path, "wb");
		if (raw)
		{
			for (i = 0; i < DDP_FRAMEBUFFER_PIXELS; ++i)
			{
				uint8_t pixel[4];
				pixel[0] = (uint8_t)(machine->framebuffer[i] >> 16);
				pixel[1] = (uint8_t)(machine->framebuffer[i] >> 8);
				pixel[2] = (uint8_t)machine->framebuffer[i];
				pixel[3] = 0xFF;
				fwrite(pixel, 1, 4, raw);
			}
			fclose(raw);
		}
	}
	if (cave_video_save_png(machine->framebuffer, png) != 0)
		return -1;
	return 0;
}

static void usage(const char *program)
{
	fprintf(stderr,
	        "usage: %s [--rom PATH] [--frames N] [--hashes FILE] [--quiet]\n"
	        "  --rom      the player's ddonpach .zip or folder (default rom/ddonpach.zip)\n"
	        "  --frames   VBLANK frame waits to run (default 600)\n"
	        "  --hashes   write 'frame hash' lines to this file\n"
	        "  --png      write the last rendered frame as a PNG\n"
	        "  --dump     print board registers and memory statistics at the end\n"
	        "  --dump-ram write work RAM to a binary file at the end\n"
	        "  --wav      write the YMZ280B output from frame 3001 onward as a WAV\n"
	        "  --load-state DIR --render-state PNG  render a saved MAME state once\n",
	        program);
}

int main(int argc, char **argv)
{
	const char *rom_path = "rom/ddonpach.zip";
	const char *hash_path = NULL;
	int frames = 600;
	int quiet = 0;
	int i;
	int completed;
	char error[512];
	ddp_rom_set rom;

	for (i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc)
			rom_path = argv[++i];
		else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
			frames = atoi(argv[++i]);
		else if (strcmp(argv[i], "--hashes") == 0 && i + 1 < argc)
			hash_path = argv[++i];
		else if (strcmp(argv[i], "--png") == 0 && i + 1 < argc)
			png_path = argv[++i];
		else if (strcmp(argv[i], "--dump") == 0)
			dump_state = 1;
		else if (strcmp(argv[i], "--auto-play") == 0)
			auto_play = 1;
		else if (strcmp(argv[i], "--stats") == 0)
			stats = 1;
		else if (strcmp(argv[i], "--dump-ram") == 0 && i + 1 < argc)
			ram_path = argv[++i];
		else if (strcmp(argv[i], "--load-state") == 0 && i + 1 < argc)
			state_dir = argv[++i];
		else if (strcmp(argv[i], "--render-state") == 0 && i + 1 < argc)
			render_png = argv[++i];
		else if (strcmp(argv[i], "--wav") == 0 && i + 1 < argc)
		{
			uint8_t placeholder[44] = { 0 };
			wav_file = fopen(argv[++i], "wb");
			if (wav_file)
				fwrite(placeholder, 1, sizeof(placeholder), wav_file);
		}
		else if (strcmp(argv[i], "--quiet") == 0)
			quiet = 1;
		else
		{
			usage(argv[0]);
			return 2;
		}
	}

	if (frames <= 0)
	{
		fprintf(stderr, "ddonpach_headless: --frames must be positive\n");
		return 2;
	}

	memset(&rom, 0, sizeof(rom));
	if (ddp_rom_load(rom_path, &rom, error, sizeof(error)) != 0)
	{
		fprintf(stderr, "ddonpach_headless: %s\n", error);
		return 3;
	}

	cave_machine_init(&ddp_machine, &rom);
	cave_machine_set_inputs(&ddp_machine, 0xFFFFu, 0xFFFFu);

	if (state_dir || render_png)
	{
		if (!state_dir || !render_png ||
		    render_saved_state(&ddp_machine, state_dir, render_png) != 0)
		{
			fprintf(stderr, "ddonpach_headless: cannot load/render the saved state\n");
			ddp_rom_free(&rom);
			return 3;
		}
		printf("rendered saved state -> %s\n", render_png);
		ddp_rom_free(&rom);
		return 0;
	}

	if (hash_path)
	{
		hash_file = fopen(hash_path, "w");
		if (!hash_file)
		{
			fprintf(stderr, "ddonpach_headless: cannot write %s\n", hash_path);
			ddp_rom_free(&rom);
			return 3;
		}
	}

	ddp_program_set_frame_callback(frame_callback);
	completed = ddp_program_run(&ddp_machine, frames);
	save_png_at_stop(&ddp_machine);
	if (wav_file)
	{
		write_wav_header(wav_file, wav_frames);
		fclose(wav_file);
		wav_file = NULL;
	}
	if (ram_path)
	{
		FILE *ram = fopen(ram_path, "wb");
		if (ram)
		{
			fwrite(ddp_machine.work_ram, 1, sizeof(ddp_machine.work_ram), ram);
			fclose(ram);
		}
	}

	if (hash_file)
		fclose(hash_file);

	if (dump_state)
	{
		int layer, index;
		unsigned nonzero = 0;
		printf("cpu: pc=%08X sr=%04X D0=%08X A7=%08X\n",
		       cpu.pc, m68k_read_sr(), cpu.r[0], cpu.r[15]);
		printf("video regs:");
		for (index = 0; index < 16; ++index)
			printf(" %04X", ddp_machine.video_regs[index]);
		printf("\n");
		for (layer = 0; layer < 3; ++layer)
		{
			printf("layer%d regs: %04X %04X %04X\n", layer,
			       ddp_machine.layer_regs[layer][0],
			       ddp_machine.layer_regs[layer][1],
			       ddp_machine.layer_regs[layer][2]);
		}
		for (index = 0; index < (int)sizeof(ddp_machine.layer_ram); ++index)
			if (((const uint8_t *)ddp_machine.layer_ram)[index])
				nonzero++;
		printf("layer RAM nonzero bytes: %u\n", nonzero);
		printf("video drawn pixels: %u\n", ddp_machine.video_drawn_pixels);
		printf("writes: palette=%u layer=%u sprite=%u\n",
		       ddp_machine.palette_writes, ddp_machine.layer_writes, ddp_machine.sprite_writes);
		printf("palette 0000=%04X 0001=%04X 4000=%04X 4001=%04X 7F00=%04X 7FFF=%04X\n",
		       ddp_machine.palette[0x0000], ddp_machine.palette[0x0001],
		       ddp_machine.palette[0x4000], ddp_machine.palette[0x4001],
		       ddp_machine.palette[0x7F00], ddp_machine.palette[0x7FFF]);
		{
			unsigned first = 0;
			while (first < DDP_PALETTE_ENTRIES && ddp_machine.palette[first] == 0)
				first++;
			{
				unsigned nonzero_palette = 0;
				unsigned last = 0;
				for (index = 0; index < (int)DDP_PALETTE_ENTRIES; ++index)
					if (ddp_machine.palette[index]) { nonzero_palette++; last = (unsigned)index; }
				printf("palette nonzero: %u, last %u\n", nonzero_palette, last);
			}
			printf("palette 4900..490F:");
			for (index = 0; index < 16; ++index)
				printf(" %04X", ddp_machine.palette[0x4900 + index]);
			printf("\n");
			printf("palette 4A00=%04X 4B00=%04X 4C00=%04X\n",
			       ddp_machine.palette[0x4A00], ddp_machine.palette[0x4B00],
			       ddp_machine.palette[0x4C00]);
			printf("palette first nonzero: %u\n", first);
			printf("ram 10E8F2:");
			for (index = 0; index < 8; ++index)
				printf(" %04X", (ddp_machine.work_ram[0xE8F2 + index * 2] << 8) |
				                 ddp_machine.work_ram[0xE8F3 + index * 2]);
			printf("\n");
		}
		printf("eeprom:");
		for (index = 0; index < 16; ++index)
			printf(" %04X", ddp_machine.eeprom.words[index]);
		printf("\n");
		printf("ram 100F44=%04X 100F82=%04X 10125C=%04X 10126E=%04X 101270=%04X 101272=%04X\n",
		       (ddp_machine.work_ram[0x0F44] << 8) | ddp_machine.work_ram[0x0F45],
		       (ddp_machine.work_ram[0x0F82] << 8) | ddp_machine.work_ram[0x0F83],
		       (ddp_machine.work_ram[0x125C] << 8) | ddp_machine.work_ram[0x125D],
		       (ddp_machine.work_ram[0x126E] << 8) | ddp_machine.work_ram[0x126F],
		       (ddp_machine.work_ram[0x1270] << 8) | ddp_machine.work_ram[0x1271],
		       (ddp_machine.work_ram[0x1272] << 8) | ddp_machine.work_ram[0x1273]);
		printf("ram 1013A8=%04X 1013AA=%04X 1013AC=%04X 101612=%04X 1017A4=%04X 1017A6=%04X\n",
		       (ddp_machine.work_ram[0x13A8] << 8) | ddp_machine.work_ram[0x13A9],
		       (ddp_machine.work_ram[0x13AA] << 8) | ddp_machine.work_ram[0x13AB],
		       (ddp_machine.work_ram[0x13AC] << 8) | ddp_machine.work_ram[0x13AD],
		       (ddp_machine.work_ram[0x1612] << 8) | ddp_machine.work_ram[0x1613],
		       (ddp_machine.work_ram[0x17A4] << 8) | ddp_machine.work_ram[0x17A5],
		       (ddp_machine.work_ram[0x17A6] << 8) | ddp_machine.work_ram[0x17A7]);
		printf("layer0 16x16 entries:");
		for (index = 0; index < 8; ++index)
		{
			const uint8_t *e = ddp_machine.layer_ram[0] + 0x0000 + index * 4;
			printf(" %02X%02X%02X%02X", e[0], e[1], e[2], e[3]);
		}
		printf("\n");
		printf("layer0 8x8 entries:");
		for (index = 0; index < 8; ++index)
		{
			const uint8_t *e = ddp_machine.layer_ram[0] + 0x4000 + index * 4;
			printf(" %02X%02X%02X%02X", e[0], e[1], e[2], e[3]);
		}
		printf("\n");
		printf("layer2 8x8 entries:");
		for (index = 0; index < 8; ++index)
		{
			const uint8_t *e = ddp_machine.layer_ram[2] + index * 4;
			printf(" %02X%02X%02X%02X", e[0], e[1], e[2], e[3]);
		}
		printf("\n");
	}

	if (stats)
	{
		printf("render time:        %.3f s (%.3f ms/frame)\n",
		       render_seconds, 1000.0 * render_seconds / (completed > 0 ? completed : 1));
		printf("hash time:          %.3f s (%.3f ms/frame)\n",
		       hash_seconds, 1000.0 * hash_seconds / (completed > 0 ? completed : 1));
	}

	if (!quiet)
	{
		printf("frames:             %d of %d\n", completed, frames);
		printf("translated fault:   %s\n", ddp_program_faulted() ? ddp_program_fault_message() : "none");
		printf("unmapped reads:     %u\n", ddp_machine.invalid_reads);
		printf("unmapped writes:    %u (open-bus, ignored)\n", ddp_machine.ignored_writes);
		printf("last work RAM hash: %016" PRIx64 "\n", hash_state);
	}

	{
		int ok = !ddp_program_faulted() && completed >= frames &&
		         ddp_machine.invalid_reads == 0 && ddp_machine.invalid_writes == 0;
		ddp_rom_free(&rom);
		return ok ? 0 : 1;
	}
}
