/* ROM-free fixture: one translated-looking entry at the fixture reset vector. */
#include "m68000/code.h"
#include "program.h"

const m68k_code_entry m68k_code_entries[] =
{
	{ 0x001000u, fixture_program },
};

const size_t m68k_code_entry_count = 1;
