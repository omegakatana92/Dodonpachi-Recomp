/**
 *    Name:        frame.h
 *    Description: Cave first-generation video timing, as MAME's cave.cpp configures it
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    16 MHz 68000, 15.625 kHz line rate, 271.5 lines per frame. MAME:
 *    m_screen[0]->set_refresh_hz(15625/271.5), set_size(320, 240).
 */
#ifndef CAVE_FRAME_H
#define CAVE_FRAME_H

#define DDP_CPU_CLOCK_HZ       16000000u
#define DDP_LINE_CLOCK_HZ      15625u
#define DDP_LINES_PER_FRAME    271.5
#define DDP_CLOCKS_PER_FRAME   278016u   /* 16 MHz / (15625 / 271.5) */
#define DDP_FRAMES_PER_SECOND  (DDP_CPU_CLOCK_HZ / DDP_CLOCKS_PER_FRAME)  /* 57.55 */

#define DDP_SCREEN_WIDTH       320
#define DDP_SCREEN_HEIGHT      240

#endif
