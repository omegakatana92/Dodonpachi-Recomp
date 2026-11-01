/**
 *    Name:        ymz280b.h
 *    Description: the Yamaha YMZ280B sample playback chip: eight PCM/ADPCM voices
 *    Version:     0.2.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    The chip is an 8-bit device on the low byte of the 68000 bus at 0x300000-0x300003, as MAME's
 *    ddonpach_map maps it with umask16(0x00ff). Sample ROMs u6/u7 are the player's.
 */
#ifndef CAVE_YMZ280B_H
#define CAVE_YMZ280B_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YMZ280B_CLOCK 16934400u
#define YMZ280B_VOICES 8
#define YMZ280B_FRAC_BITS 9
#define YMZ280B_FRAC_ONE (1 << YMZ280B_FRAC_BITS)

typedef struct
{
	int playing;
	int ended;
	int keyon;
	int looping;
	int mode;                       /* 1 = 4-bit ADPCM, 2 = 8-bit PCM, 3 = 16-bit PCM */

	uint32_t fnum;
	uint32_t start;
	uint32_t stop;
	uint32_t loop_start;
	uint32_t loop_end;
	uint32_t position;

	int32_t signal;
	int32_t step;
	int32_t loop_signal;
	int32_t loop_step;
	int32_t loop_count;

	int level;
	int pan;
	int output_left;
	int output_right;

	uint32_t output_step;
	uint32_t output_pos;
	int16_t last_sample;
	int16_t curr_sample;
} ymz280b_voice;

typedef struct
{
	uint8_t regs[256];
	uint8_t address;
	uint8_t status;

	const uint8_t *rom;
	size_t rom_size;
	ymz280b_voice voice[YMZ280B_VOICES];

	int keyon_enable;
	int ext_mem_enable;
	int irq_enable;
	int irq_mask;
	int irq_state;
	uint8_t ext_readlatch;
	uint32_t ext_mem_address;
	uint32_t ext_mem_address_hi;
	uint32_t ext_mem_address_mid;

	int sample_rate;                /* the host output rate (the chip renders at 88200 Hz internally) */
	uint32_t clock;
} cave_ymz280b;

void cave_ymz280b_init(cave_ymz280b *chip, const uint8_t *rom, size_t rom_size, int sample_rate);
uint8_t cave_ymz280b_read(cave_ymz280b *chip, unsigned offset);
void cave_ymz280b_write(cave_ymz280b *chip, unsigned offset, uint8_t value);

/* Mix the chip's next samples (stereo interleaved, signed 16-bit). 'frames' is in sample pairs at
   chip->sample_rate. */
void cave_ymz280b_render(cave_ymz280b *chip, int16_t *samples, size_t frames);

#ifdef __cplusplus
}
#endif

#endif
