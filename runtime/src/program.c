/**
 *    Name:        program.c
 *    Description: boot the translated program from the ROM vectors and run it until a frame target,
 *                 a port request or a fault
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    The translated C keeps the game's call stack in C, so a run is one continuous session: the
 *    frame wait either returns to the game (ports keep running) or leaves the session (the target
 *    was reached, the window closed, or the translated code faulted).
 */
#include "cave/program.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "m68000/cpu.h"
#include "m68000/registers.h"

static jmp_buf exit_jump;
static int exit_armed;
static int target_frames;
static int stop_requested;
static int session_started;
static int faulted;
static char fault_message[256];
static ddp_frame_callback frame_callback;

void ddp_program_set_frame_callback(ddp_frame_callback callback)
{
	frame_callback = callback;
}

void ddp_program_request_stop(void)
{
	stop_requested = 1;
}

int ddp_program_faulted(void)
{
	return faulted;
}

const char *ddp_program_fault_message(void)
{
	return fault_message;
}

void ddp_program_fault(const char *format, ...)
{
	va_list arguments;
	if (!faulted)
	{
		faulted = 1;
		va_start(arguments, format);
		vsnprintf(fault_message, sizeof(fault_message), format, arguments);
		va_end(arguments);
		fprintf(stderr, "ddonpach: %s\n", fault_message);
	}
	if (exit_armed)
		longjmp(exit_jump, 1);
}

/* Called by the generated code at the game's idle instruction (0x0571B2). */
void program_frame_wait(void)
{
	cave_machine_frame(&ddp_machine);

	if (frame_callback)
		frame_callback(&ddp_machine);

	if (stop_requested || (target_frames > 0 && (int)ddp_machine.frames_run >= target_frames))
	{
		if (exit_armed)
			longjmp(exit_jump, 1);
	}
}

int ddp_program_run(cave_machine *machine, int frames)
{
	uint32_t reset_pc;
	uint32_t reset_sp;

	if (session_started)
	{
		ddp_program_fault("ddp_program_run was called twice in one process");
		return (int)machine->frames_run;
	}
	session_started = 1;

	target_frames = frames > 0 ? frames : 0;
	stop_requested = 0;
	faulted = 0;
	fault_message[0] = 0;

	reset_pc = ddp_rom_reset_pc(machine->rom);
	reset_sp = ddp_rom_reset_sp(machine->rom);

	memset(&cpu, 0, sizeof(cpu));
	cpu.r[15] = reset_sp;                 /* A7 = initial supervisor stack pointer */
	cpu.r[M68K_SR_SYSTEM] = 0x2700;       /* supervisor, interrupt mask 7 */
	cpu.pc = reset_pc;

	exit_armed = 1;
	if (setjmp(exit_jump) != 0)
	{
		exit_armed = 0;
		return (int)machine->frames_run;
	}

	/* The reset routine sets A7 again and enters the game's main loop. */
	m68k_jump(reset_pc, reset_pc);

	/* The translated program returned to its reset entry: the game's main loop should never
	   do that. Report it instead of pretending the run completed. */
	if (!faulted)
		ddp_program_fault("the translated program returned to its reset entry at %06X",
		                  cpu.pc & 0xFFFFFFu);

	exit_armed = 0;
	return (int)machine->frames_run;
}
