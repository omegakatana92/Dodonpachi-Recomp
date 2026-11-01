/* ROM-free fixture: exercises the board bus and the frame-wait path. No game data. */
#include "m68000/ops.h"
#include "program.h"

void fixture_program(void)
{
	uint32_t count = 0;
	for (;;)
	{
		count++;
		mem_write32(0x100000u, count);
		mem_write16(0x100004u, (uint16_t)(count >> 16));
		program_frame_wait();
	}
}
