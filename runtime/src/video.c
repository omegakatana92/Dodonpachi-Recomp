/**
 *    Name:        video.c
 *    Description: the 038 tile layers (8x8) and the 320x240 framebuffer
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    The tile format, layer registers, row effects and priority order follow MAME's
 *    devices/video/tmap038.cpp and cave_v.cpp (screen_update/tilemap_draw); the graphics layouts are
 *    gfx_8x8x4_packed_msb and layout_8x8x8 from MAME's generic.cpp / cave.cpp. 16x16 layers are
 *    decoded in a later pass; DoDonPachi's three layers are 8x8 in normal play.
 */
#include "cave/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "miniz.h"

#define LAYER_BASE_8X8  0x4000u
#define TILE_BYTES_4BPP 32u
#define TILE_BYTES_8BPP 64u

static uint16_t read_be16(const uint8_t *p)
{
	return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | p[3];
}

uint32_t cave_video_palette_rgb(uint16_t entry)
{
	/* MAME palette_device::xGRB_555: x GGGGG RRRRR BBBBB */
	uint32_t green = (entry >> 10) & 0x1Fu;
	uint32_t red = (entry >> 5) & 0x1Fu;
	uint32_t blue = entry & 0x1Fu;
	red = (red << 3) | (red >> 2);
	green = (green << 3) | (green >> 2);
	blue = (blue << 3) | (blue >> 2);
	return (red << 16) | (green << 8) | blue;
}

/* A 4bpp packed 8x8 tile: four bytes per row, high nibble first. */
static unsigned tile_pen_4bpp(const uint8_t *rom, uint32_t code, unsigned x, unsigned y)
{
	const uint8_t *tile;
	uint8_t packed;
	/* MAME's gfx_element::transpen() takes code % elements(); the 4bpp element has
	   65536 characters, so a 16x16 raw code may wrap instead of becoming blank. */
	code &= 0xFFFFu;
	tile = rom + (size_t)code * TILE_BYTES_4BPP;
	packed = tile[y * 4 + (x >> 1)];
	return (x & 1u) ? (packed & 0x0Fu) : (packed >> 4);
}

/* An 8bpp 8x8 tile in MAME's layout_8x8x8: eight planes, with the pixel's high nibble
   (planes 0-3) at bits 8-11 and its low nibble (planes 4-7) at bits 0-3. */
static unsigned tile_pen_8bpp(const uint8_t *rom, uint32_t code, unsigned x, unsigned y)
{
	static const unsigned plane_offset[8] = { 8, 9, 10, 11, 0, 1, 2, 3 };
	static const unsigned x_offset[8] = { 0, 4, 16, 20, 32, 36, 48, 52 };
	const uint8_t *tile;
	unsigned base_bit;
	unsigned pen = 0;
	unsigned plane;

	if (code >= 32768u)
		code &= 0x7FFFu;
	tile = rom + (size_t)code * TILE_BYTES_8BPP;
	base_bit = y * 64u + x_offset[x];
	for (plane = 0; plane < 8; ++plane)
	{
		unsigned bit = base_bit + plane_offset[plane];
		/* MAME's readbit() is MSB-first and decode() assigns plane 0 to pen bit 7. */
		if ((tile[bit >> 3] & (0x80u >> (bit & 7u))) != 0)
			pen |= 1u << (7u - plane);
	}
	return pen;
}

void cave_video_render(cave_machine *machine, uint32_t *pixels)
{
	static uint8_t best_priority[DDP_FRAMEBUFFER_PIXELS];
	static uint8_t best_layer_priority[DDP_FRAMEBUFFER_PIXELS];
	static uint8_t best_layer[DDP_FRAMEBUFFER_PIXELS];
	static uint8_t best_valid[DDP_FRAMEBUFFER_PIXELS];
	unsigned layer;
	size_t i;
	int only_layer = -1;

	{
		static int initialized;
		if (!initialized)
		{
			const char *selection = getenv("DDP_RENDER_LAYER");
			initialized = 1;
			if (selection)
				only_layer = atoi(selection);
		}
	}

	machine->video_drawn_pixels = 0;
	{
		uint32_t backdrop = cave_video_palette_rgb(machine->palette[0x7F00]);
		for (i = 0; i < DDP_FRAMEBUFFER_PIXELS; ++i)
		{
			pixels[i] = backdrop;
			best_priority[i] = 0;
			best_layer_priority[i] = 0;
			best_layer[i] = 0;
			best_valid[i] = 0;
		}
	}

	for (layer = 0; layer < 3; ++layer)
	{
		uint16_t vreg0 = machine->layer_regs[layer][0];
		if (only_layer >= 0 && (int)layer != only_layer)
			continue;
		uint16_t vreg1 = machine->layer_regs[layer][1];
		uint16_t vreg2 = machine->layer_regs[layer][2];
		int flipx = (vreg0 & 0x8000u) == 0;
		int flipy = (vreg1 & 0x8000u) == 0;
		int rowscroll = (vreg0 & 0x4000u) != 0 && layer < 2;
		int rowselect = (vreg1 & 0x4000u) != 0 && layer < 2;
		int sixteen = (vreg1 & 0x2000u) != 0 && layer < 2;
		unsigned layer_priority = vreg2 & 3u;
		int enabled = (vreg2 & 0x0010u) == 0;
		int offs_x = 0x13;
		int offs_y = -0x12;
		int sx, sy;
		int x, y;
		const uint8_t *rom = machine->rom->layers[layer];

		{
			static int trace = -1;
			if (trace < 0)
				trace = getenv("DDP_VIDEO_TRACE") ? 1 : 0;
			if (trace && (machine->frames_run % 600u) == 0)
				fprintf(stderr, "video %u layer %u enabled=%d sixteen=%d rom=%p vregs=%04X %04X %04X\n",
				        machine->frames_run, layer, enabled, sixteen, (const void *)rom,
				        vreg0, vreg1, vreg2);
		}
		if (!enabled || !rom)
			continue;

		/* MAME's cave_v.cpp: 16x16 layers use a 1/2/3 pixel adjustment, 8x8 layers add 8. */
		if (layer == 0)
			offs_x -= sixteen ? 1 : 9;
		else if (layer == 1)
			offs_x -= sixteen ? 2 : 10;
		else
			offs_x -= sixteen ? 3 : 11;

		sx = (int)(vreg0 & 0x1FFu) - (int)machine->video_regs[0] +
		     (flipx ? offs_x + 2 : -offs_x);
		sy = (int)(vreg1 & 0x1FFu) - (int)machine->video_regs[1] +
		     (flipy ? offs_y + 2 : -offs_y);

		/* MAME's tilemap applies its flip around the visible area, not after scrolling:
		   effective_rowscroll() returns screen_width - tilemap_width - (dx_flipped - scroll),
		   and draw_instance() flips the map inside the pixmap. Do the same per pixel. */
		if (flipx)
			sx = (DDP_SCREEN_WIDTH - 512) - (0 - sx);
		else
			sx = 0 - sx;
		if (flipy)
			sy = (DDP_SCREEN_HEIGHT - 512) - (0 - sy);
		else
			sy = 0 - sy;
		sx -= 512;                      /* the first of the repeating instances */
		sy -= 512;

		for (y = 0; y < DDP_SCREEN_HEIGHT; ++y)
		{
			int map_y;
			int sx_line = sx;
			int row_index = (sy + (flipy ? 1 : -1) + y) & 511;
			const uint8_t *line_ram = machine->layer_ram[layer] + 0x1000u;

			if (rowselect)
				map_y = read_be16(line_ram + (unsigned)row_index * 4u + 2u) & 0x1FF;
			else
				map_y = y - sy;
			if (rowscroll)
				sx_line += read_be16(line_ram + (unsigned)row_index * 4u);
			if (flipy)
				map_y = 511 - map_y;
			map_y &= 511;

			for (x = 0; x < DDP_SCREEN_WIDTH; ++x)
			{
				int map_x = x - sx_line;
				int tile_y, tile_x, py, px;
				const uint8_t *entry;
				uint32_t value, code;
				unsigned color, priority, pen, palette_index;
				size_t offset;

				if (flipx)
					map_x = 511 - map_x;
				map_x &= 511;
				tile_y = map_y >> 3;
				tile_x = map_x >> 3;
				py = map_y & 7;
				px = map_x & 7;
				if (flipy)
					py = 7 - py;
				if (flipx)
					px = 7 - px;

				if (sixteen)
				{
					/* tmap038's get_tile_info for the 16x16 map: one entry per 2x2
					   group of 8x8 cells, and the code's low two bits select the
					   quadrant. */
					unsigned tile_index = (unsigned)(tile_y * 64 + tile_x);
					unsigned entry_tile = (tile_index % 64u) / 2u +
					                      ((tile_index / 64u) / 2u) * 32u;
					entry = machine->layer_ram[layer] + (size_t)entry_tile * 4u;
					value = read_be32(entry);
					code = ((value & 0xFFFFu) << 2) ^ (tile_index & 1u) ^
					       (((tile_index / 64u) & 1u) << 1);
				}
				else
				{
					/* Layers 0/1 keep the 8x8 map at +0x4000; layer 2's whole 16 KiB VRAM
					   is the 8x8 map at offset 0 (MAME's vram_8x8_map). */
					size_t base = (layer < 2) ? LAYER_BASE_8X8 : 0u;
					entry = machine->layer_ram[layer] + base +
					        (size_t)(tile_y * 64 + tile_x) * 4u;
					value = read_be32(entry);
					code = value & 0x0003FFFFu;
				}
				priority = (value >> 30) & 3u;
				color = (value >> 24) & 0x3Fu;

				pen = (layer == 2)
					? tile_pen_8bpp(rom, code, (unsigned)px, (unsigned)py)
					: tile_pen_4bpp(rom, code, (unsigned)px, (unsigned)py);
				{
					static int trace = -1;
					if (trace < 0)
						trace = getenv("DDP_VIDEO_TRACE") ? 1 : 0;
					if (trace && x == 0 && y == 0 && (machine->frames_run % 600u) == 0)
						fprintf(stderr,
						        "  pixel layer=%u map=(%d,%d) tile=(%d,%d) value=%08X code=%06X pen=%u priority=%u\n",
						        layer, map_x, map_y, tile_x, tile_y, value, code, pen, priority);
				}
				if (pen == 0)
					continue;

				offset = (size_t)y * DDP_SCREEN_WIDTH + (size_t)x;
				if (best_valid[offset])
				{
					if (priority < best_priority[offset])
						continue;
					if (priority == best_priority[offset] &&
					    layer_priority < best_layer_priority[offset])
						continue;
					if (priority == best_priority[offset] &&
					    layer_priority == best_layer_priority[offset] &&
					    layer < best_layer[offset])
						continue;
				}

				palette_index = 0x4000u + color * 256u + pen;
				if (palette_index >= DDP_PALETTE_ENTRIES)
					continue;
				pixels[offset] = cave_video_palette_rgb(machine->palette[palette_index]);
				best_priority[offset] = (uint8_t)priority;
				best_layer_priority[offset] = (uint8_t)layer_priority;
				best_layer[offset] = (uint8_t)layer;
				best_valid[offset] = 1;
				machine->video_drawn_pixels++;
			}
		}
	}

	/* 013 sprites, ddonpach's no-zoom format: 16-byte records, 8 MiB of 4bpp tiles. */
	if (only_layer < 0 || only_layer == 3)
	{
		unsigned bank = machine->video_regs[4] & 3u;
		{
			const char *forced = getenv("DDP_SPRITE_BANK");
			if (forced)
				bank = (unsigned)atoi(forced) & 3u;
		}
		const uint8_t *sprite_ram = machine->sprite_ram + bank * 0x4000u;
		const uint8_t *sprites = machine->rom->sprites;
		int global_flipx = (machine->video_regs[0] & 0x8000u) != 0;
		int global_flipy = (machine->video_regs[1] & 0x8000u) != 0;
		unsigned index;

		if (sprites)
		{
			for (index = 0; index < 0x400u; ++index)
			{
				const uint8_t *record = sprite_ram + index * 16u;
				uint16_t attr = read_be16(record);
				uint32_t code = read_be16(record + 2) + ((attr & 3u) << 16);
				int x = read_be16(record + 4) & 0x3FF;
				int y = read_be16(record + 6) & 0x3FF;
				uint16_t size = read_be16(record + 8);
				int width = ((size >> 8) & 0x1F) * 16;
				int height = (size & 0x1F) * 16;
				int flipx = (attr >> 3) & 1;
				int flipy = (attr >> 2) & 1;
				unsigned color = (attr >> 8) & 0x3Fu;
				unsigned priority = (attr >> 4) & 3u;
				int sx, sy;

				if (!width || !height || !sprites)
					continue;
				code &= 0xFFFFu;        /* the 013's 8 MiB graphics wrap at 65536 tiles */

				x = ((x & 0x200) ? (x | ~0x3FF) : x);
				y = ((y & 0x200) ? (y | ~0x3FF) : y);

				if (global_flipx)
				{
					x = 320 - x - width;
					flipx = !flipx;
				}
				if (global_flipy)
				{
					y = 240 - y - height;
					flipy = !flipy;
				}
				if (x + width <= 0 || x >= DDP_SCREEN_WIDTH ||
				    y + height <= 0 || y >= DDP_SCREEN_HEIGHT)
					continue;

				for (sy = 0; sy < height; ++sy)
				{
					int draw_y = y + (flipy ? height - 1 - sy : sy);
					if (draw_y < 0 || draw_y >= DDP_SCREEN_HEIGHT)
						continue;
					for (sx = 0; sx < width; ++sx)
					{
						int draw_x = x + (flipx ? width - 1 - sx : sx);
						size_t offset;
						size_t rowoff;
						uint8_t packed;
						unsigned pen;
						unsigned palette_index;

						if (draw_x < 0 || draw_x >= DDP_SCREEN_WIDTH)
							continue;
						rowoff = (size_t)code * 128u + (size_t)sy * (size_t)(width / 2);
						if (rowoff + (size_t)(sx >> 1) >= DDP_SPRITE_SIZE)
							continue;
						packed = sprites[rowoff + (size_t)(sx >> 1)];
						pen = (sx & 1u) ? (packed & 0x0Fu) : (packed >> 4);
						if (pen == 0)
							continue;
						offset = (size_t)draw_y * DDP_SCREEN_WIDTH + (size_t)draw_x;
						if (best_valid[offset] && priority < best_priority[offset])
							continue;
						palette_index = color * 256u + pen;
						if (palette_index >= DDP_PALETTE_ENTRIES)
							continue;
						pixels[offset] = cave_video_palette_rgb(machine->palette[palette_index]);
						best_valid[offset] = 1;
					}
				}
			}
		}
	}

	machine->video_dirty = 0;
}

int cave_video_save_png(const uint32_t *pixels, const char *path)
{
	uint8_t *rgb;
	void *png;
	size_t png_size = 0;
	FILE *file;
	size_t i;
	int result;

	rgb = (uint8_t *)malloc(DDP_FRAMEBUFFER_PIXELS * 3);
	if (!rgb)
		return -1;
	for (i = 0; i < DDP_FRAMEBUFFER_PIXELS; ++i)
	{
		rgb[i * 3 + 0] = (uint8_t)(pixels[i] >> 16);
		rgb[i * 3 + 1] = (uint8_t)(pixels[i] >> 8);
		rgb[i * 3 + 2] = (uint8_t)pixels[i];
	}
	png = tdefl_write_image_to_png_file_in_memory_ex(rgb, DDP_SCREEN_WIDTH, DDP_SCREEN_HEIGHT,
	                                                 3, &png_size, 6, 0);
	free(rgb);
	if (!png)
		return -1;
	file = fopen(path, "wb");
	if (!file)
	{
		mz_free(png);
		return -1;
	}
	result = (fwrite(png, 1, png_size, file) == png_size) ? 0 : -1;
	fclose(file);
	mz_free(png);
	return result;
}
