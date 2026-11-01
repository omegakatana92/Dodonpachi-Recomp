/**
 *    Name:        ymz280b.c
 *    Description: the YMZ280B eight-voice PCM/ADPCM playback chip
 *    Version:     0.2.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    Follows MAME's devices/sound/ymz280b.cpp (BSD-3-Clause, Aaron Giles): the register map,
 *    the four-bit ADPCM decoder, the 8/16-bit PCM modes, loop points, pitch, level and pan.
 *    The chip renders at 88.2 kHz internally and is resampled to the host output rate here.
 *    No MAME code is linked into the game.
 */
#include "cave/ymz280b.h"

#include <stdlib.h>
#include <string.h>

#define YMZ_INTERNAL_RATE 88200.0

static const int index_scale[8] = { 0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266 };
static int diff_lookup[16];
static int tables_ready;

static void compute_tables(void)
{
	int nibble;
	if (tables_ready)
		return;
	for (nibble = 0; nibble < 16; ++nibble)
	{
		int value = (nibble & 0x07) * 2 + 1;
		diff_lookup[nibble] = (nibble & 0x08) ? -value : value;
	}
	tables_ready = 1;
}

static uint8_t ymz_read_byte(const cave_ymz280b *chip, uint32_t address)
{
	if (!chip->rom || chip->rom_size == 0)
		return 0xFF;
	return chip->rom[address % chip->rom_size];
}

static void update_step(ymz280b_voice *voice)
{
	uint32_t frequency = (voice->mode == 1) ? (voice->fnum & 0x0FFu) : (voice->fnum & 0x1FFu);
	voice->output_step = frequency + 1u;
}

static void update_volumes(ymz280b_voice *voice)
{
	if (voice->pan == 8)
	{
		voice->output_left = voice->level;
		voice->output_right = voice->level;
	}
	else if (voice->pan < 8)
	{
		voice->output_left = voice->level;
		voice->output_right = (voice->pan == 0) ? 0 : voice->level * (voice->pan - 1) / 7;
	}
	else
	{
		voice->output_left = voice->level * (15 - voice->pan) / 7;
		voice->output_right = voice->level;
	}
}

static int32_t clamp16(int32_t value)
{
	if (value > 32767)
		return 32767;
	if (value < -32768)
		return -32768;
	return value;
}

/* Decode the next source sample for a voice. Returns 0 at the end. */
static int32_t next_sample(cave_ymz280b *chip, ymz280b_voice *voice, int index)
{
	int32_t value = 0;

	if (voice->mode == 1)
	{
		uint32_t nibble = ymz_read_byte(chip, voice->position / 2u);
		nibble >>= ((~voice->position & 1u) << 2);
		voice->signal += (voice->step * diff_lookup[nibble & 15]) / 8;
		voice->signal = clamp16(voice->signal);

		voice->step = (voice->step * index_scale[nibble & 7]) >> 8;
		if (voice->step > 0x6000)
			voice->step = 0x6000;
		else if (voice->step < 0x7f)
			voice->step = 0x7f;

		value = voice->signal;
		voice->position++;

		if (voice->looping)
		{
			if (voice->position == voice->loop_start && voice->loop_count == 0)
			{
				voice->loop_signal = voice->signal;
				voice->loop_step = voice->step;
			}
			if (voice->position >= voice->loop_end && voice->keyon)
			{
				voice->position = voice->loop_start;
				voice->signal = voice->loop_signal;
				voice->step = voice->loop_step;
				voice->loop_count++;
			}
		}
	}
	else if (voice->mode == 2)
	{
		value = (int8_t)ymz_read_byte(chip, voice->position / 2u) * 256;
		voice->position += 2;
		if (voice->looping && voice->position >= voice->loop_end && voice->keyon)
			voice->position = voice->loop_start;
	}
	else if (voice->mode == 3)
	{
		uint32_t low = ymz_read_byte(chip, voice->position / 2u);
		uint32_t high = ymz_read_byte(chip, voice->position / 2u + 1u);
		value = (int16_t)((high << 8) | low);
		voice->position += 4;
		if (voice->looping && voice->position >= voice->loop_end && voice->keyon)
			voice->position = voice->loop_start;
	}
	else
	{
		voice->playing = 0;
	}

	if (voice->position >= voice->stop)
	{
		voice->playing = 0;
		voice->ended = 1;
		chip->status |= (uint8_t)(1u << index);
		value = 0;
	}
	return value;
}

static void write_register(cave_ymz280b *chip, uint8_t data)
{
	if (chip->address < 0x80)
	{
		unsigned index = (chip->address >> 2) & 7;
		ymz280b_voice *voice = &chip->voice[index];

		switch (chip->address & 0xE3)
		{
		case 0x00:
			voice->fnum = (voice->fnum & 0x100u) | (data & 0xFFu);
			update_step(voice);
			break;
		case 0x01:
			voice->fnum = (voice->fnum & 0xFFu) | ((uint32_t)(data & 1u) << 8);
			voice->looping = (data & 0x10) >> 4;
			if ((data & 0x60) == 0)
				data &= 0x7F;
			else
				voice->mode = (data & 0x60) >> 5;
			if (!voice->keyon && (data & 0x80) && chip->keyon_enable)
			{
				voice->playing = 1;
				voice->position = voice->start;
				voice->signal = voice->loop_signal = 0;
				voice->step = voice->loop_step = 0x7F;
				voice->loop_count = 0;
				voice->output_pos = 0;
				voice->last_sample = voice->curr_sample = 0;
				voice->ended = 0;
			}
			else if (voice->keyon && !(data & 0x80))
			{
				voice->playing = 0;
			}
			voice->keyon = (data & 0x80) >> 7;
			update_step(voice);
			break;
		case 0x02:
			voice->level = data;
			update_volumes(voice);
			break;
		case 0x03:
			voice->pan = data & 0x0F;
			update_volumes(voice);
			break;
		case 0x20:
			voice->start = (voice->start & 0x1FFFEu) | ((uint32_t)data << 17);
			break;
		case 0x21:
			voice->loop_start = (voice->loop_start & 0x1FFFEu) | ((uint32_t)data << 17);
			break;
		case 0x22:
			voice->loop_end = (voice->loop_end & 0x1FFFEu) | ((uint32_t)data << 17);
			break;
		case 0x23:
			voice->stop = (voice->stop & 0x1FFFEu) | ((uint32_t)data << 17);
			break;
		case 0x40:
			voice->start = (voice->start & 0x1FE01FEu) | ((uint32_t)data << 9);
			break;
		case 0x41:
			voice->loop_start = (voice->loop_start & 0x1FE01FEu) | ((uint32_t)data << 9);
			break;
		case 0x42:
			voice->loop_end = (voice->loop_end & 0x1FE01FEu) | ((uint32_t)data << 9);
			break;
		case 0x43:
			voice->stop = (voice->stop & 0x1FE01FEu) | ((uint32_t)data << 9);
			break;
		case 0x60:
			voice->start = (voice->start & 0x1FFFE00u) | ((uint32_t)data << 1);
			break;
		case 0x61:
			voice->loop_start = (voice->loop_start & 0x1FFFE00u) | ((uint32_t)data << 1);
			break;
		case 0x62:
			voice->loop_end = (voice->loop_end & 0x1FFFE00u) | ((uint32_t)data << 1);
			break;
		case 0x63:
			voice->stop = (voice->stop & 0x1FFFE00u) | ((uint32_t)data << 1);
			break;
		default:
			break;
		}
	}
	else
	{
		switch (chip->address)
		{
		case 0x84:
			chip->ext_mem_address_hi = (uint32_t)data << 16;
			break;
		case 0x85:
			chip->ext_mem_address_mid = (uint32_t)data << 8;
			break;
		case 0x86:
			chip->ext_mem_address = chip->ext_mem_address_hi |
			                        chip->ext_mem_address_mid | data;
			if (chip->ext_mem_enable)
				chip->ext_readlatch = ymz_read_byte(chip, chip->ext_mem_address);
			break;
		case 0x87:
			if (chip->ext_mem_enable)
				chip->ext_mem_address = (chip->ext_mem_address + 1) & 0xFFFFFF;
			break;
		case 0xFE:
			chip->irq_mask = data;
			break;
		case 0xFF:
		{
			int old_keyon = chip->keyon_enable;
			chip->ext_mem_enable = (data & 0x40) >> 6;
			chip->irq_enable = (data & 0x10) >> 4;
			chip->keyon_enable = (data & 0x80) >> 7;
			if (old_keyon && !chip->keyon_enable)
			{
				int i;
				for (i = 0; i < YMZ280B_VOICES; ++i)
					chip->voice[i].playing = 0;
			}
			else if (!old_keyon && chip->keyon_enable)
			{
				int i;
				for (i = 0; i < YMZ280B_VOICES; ++i)
					if (chip->voice[i].keyon && chip->voice[i].looping)
						chip->voice[i].playing = 1;
			}
			break;
		}
		default:
			break;
		}
	}
}

void cave_ymz280b_init(cave_ymz280b *chip, const uint8_t *rom, size_t rom_size, int sample_rate)
{
	int i;
	compute_tables();
	memset(chip, 0, sizeof(*chip));
	chip->rom = rom;
	chip->rom_size = rom_size;
	chip->sample_rate = sample_rate > 0 ? sample_rate : 44100;
	chip->clock = YMZ280B_CLOCK;
	for (i = 0; i < YMZ280B_VOICES; ++i)
	{
		chip->voice[i].output_pos = YMZ280B_FRAC_ONE;
		chip->voice[i].output_step = 1;
	}
}

uint8_t cave_ymz280b_read(cave_ymz280b *chip, unsigned offset)
{
	if ((offset & 1u) == 0)
	{
		uint8_t result;
		if (!chip->ext_mem_enable)
			return 0xFF;
		result = chip->ext_readlatch;
		chip->ext_readlatch = ymz_read_byte(chip, chip->ext_mem_address);
		chip->ext_mem_address = (chip->ext_mem_address + 1) & 0xFFFFFF;
		return result;
	}
	else
	{
		uint8_t result = chip->status;
		chip->status = 0;
		return result;
	}
}

void cave_ymz280b_write(cave_ymz280b *chip, unsigned offset, uint8_t value)
{
	if ((offset & 1u) == 0)
		chip->address = value;
	else
	{
		chip->regs[chip->address] = value;
		write_register(chip, value);
	}
}

static void render_internal(cave_ymz280b *chip, int16_t *output, int frames)
{
	int32_t *mix;
	int i;
	int v;

	if (frames <= 0)
		return;
	mix = (int32_t *)calloc((size_t)frames * 2u, sizeof(int32_t));
	if (!mix)
	{
		memset(output, 0, (size_t)frames * 2u * sizeof(int16_t));
		return;
	}

	for (v = 0; v < YMZ280B_VOICES; ++v)
	{
		ymz280b_voice *voice = &chip->voice[v];
		int16_t prev = voice->last_sample;
		int16_t curr = voice->curr_sample;
		uint32_t pos = voice->output_pos;
		int left_volume = voice->output_left;
		int right_volume = voice->output_right;
		int sample;

		if (!voice->playing && curr == 0 && prev == 0)
		{
			voice->output_pos = YMZ280B_FRAC_ONE;
			continue;
		}

		for (sample = 0; sample < frames; ++sample)
		{
			int32_t interpolated;
			if (!voice->playing && curr == 0 && prev == 0)
				break;

			interpolated = (((int32_t)prev * (YMZ280B_FRAC_ONE - (int32_t)pos)) +
			                ((int32_t)curr * (int32_t)pos)) >> YMZ280B_FRAC_BITS;
			mix[sample * 2] += interpolated * left_volume / 512;
			mix[sample * 2 + 1] += interpolated * right_volume / 512;

			pos += voice->output_step;
			while (pos >= YMZ280B_FRAC_ONE)
			{
				pos -= YMZ280B_FRAC_ONE;
				prev = curr;
				curr = voice->playing ? (int16_t)next_sample(chip, voice, v) : 0;
			}
		}

		voice->last_sample = prev;
		voice->curr_sample = curr;
		voice->output_pos = pos;
	}

	for (i = 0; i < frames * 2; ++i)
		output[i] = (int16_t)clamp16(mix[i]);
	free(mix);
}

void cave_ymz280b_render(cave_ymz280b *chip, int16_t *samples, size_t frames)
{
	double ratio;
	double internal_frames;
	int16_t *internal;
	size_t internal_count;
	size_t i;

	if (frames == 0)
		return;
	ratio = YMZ_INTERNAL_RATE / (double)(chip->sample_rate > 0 ? chip->sample_rate : 44100);
	internal_frames = (double)frames * ratio + 2.0;
	internal_count = (size_t)internal_frames;
	internal = (int16_t *)calloc(internal_count * 2u, sizeof(int16_t));
	if (!internal)
	{
		memset(samples, 0, frames * 2u * sizeof(int16_t));
		return;
	}

	render_internal(chip, internal, (int)internal_count);

	for (i = 0; i < frames; ++i)
	{
		double position = (double)i * ratio;
		size_t index = (size_t)position;
		double fraction = position - (double)index;
		size_t next = index + 1 < internal_count ? index + 1 : index;
		double left = (double)internal[index * 2] * (1.0 - fraction) +
		              (double)internal[next * 2] * fraction;
		double right = (double)internal[index * 2 + 1] * (1.0 - fraction) +
		               (double)internal[next * 2 + 1] * fraction;
		samples[i * 2] = (int16_t)clamp16((int32_t)left);
		samples[i * 2 + 1] = (int16_t)clamp16((int32_t)right);
	}
	free(internal);
}
