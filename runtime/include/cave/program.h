/**
 *    Name:        program.h
 *    Description: the translated program's boot and frame entry points
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 */
#ifndef DDP_PROGRAM_H
#define DDP_PROGRAM_H

#include "cave/machine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ddp_frame_callback)(cave_machine *machine);

/* A port's per-frame hook: render, poll events, feed audio. It runs where the game's main loop
   waits for VBLANK. */
void ddp_program_set_frame_callback(ddp_frame_callback callback);

/* Boot from the ROM's reset vectors and run. With frames > 0 the run stops after that many VBLANK
   frame waits (the boot's own waits included); with frames == 0 it runs until the frame callback
   calls ddp_program_request_stop() or a fault stops it. The translated C keeps its call stack
   across frames, so one run is one session. Returns the number of frame waits completed. */
int ddp_program_run(cave_machine *machine, int frames);

void ddp_program_request_stop(void);

int ddp_program_faulted(void);
const char *ddp_program_fault_message(void);

/* Called by the runtime when the translated program must stop. */
void ddp_program_fault(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
