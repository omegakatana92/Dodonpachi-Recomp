/**
 *    Name:        video.h
 *    Description: the 038 tile layers and the 013 sprite generator, drawn into a 320x240 framebuffer
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 */
#ifndef CAVE_VIDEO_H
#define CAVE_VIDEO_H

#include <stdint.h>

#include "cave/machine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Render the board's current memories. 'pixels' holds 320*240 0x00RRGGBB values, top row first. */
void cave_video_render(cave_machine *machine, uint32_t *pixels);

/* The xGRB_555 palette entry as 0x00RRGGBB. */
uint32_t cave_video_palette_rgb(uint16_t entry);

/* Write a 320x240 framebuffer as a PNG (development snapshots). */
int cave_video_save_png(const uint32_t *pixels, const char *path);

#ifdef __cplusplus
}
#endif

#endif
