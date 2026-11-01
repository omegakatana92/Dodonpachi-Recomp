/**
 *    Name:        m68000.c
 *    Description: the 68000 state and the runtime services the translated C calls: SR, exceptions,
 *                 interrupts, computed calls and jumps, and memory through the board bus
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    This is not an instruction interpreter: every instruction is already C. What remains here is
 *    the architectural state and the transitions that cannot be expressed inside one translated
 *    function (an exception frame, a return through a computed address, the VBLANK).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68000/code.h"
#include "m68000/cpu.h"
#include "m68000/ops.h"
#include "m68000/registers.h"
#include "cave/machine.h"
#include "cave/program.h"

m68k_cpu cpu;

#define DDP_CALL_STACK_MAX 8192

static uint32_t call_stack[DDP_CALL_STACK_MAX];
static unsigned call_depth;
static unsigned stop_count;

static int collect_gaps(void)
{
	static int enabled = -1;
	if (enabled < 0)
		enabled = getenv("DDP_COLLECT_GAPS") ? 1 : 0;
	return enabled;
}

static void record_gap(uint32_t address, const char *kind)
{
	FILE *file = fopen("build/gaps.txt", "a");
	if (file)
	{
		fprintf(file, "%06X %s\n", address & 0xFFFFFFu, kind);
		fclose(file);
	}
}

static void (*lookup_entry(uint32_t address))(void)
{
	size_t low = 0;
	size_t high = m68k_code_entry_count;

	address &= 0xFFFFFFu;
	while (low < high)
	{
		size_t middle = low + (high - low) / 2;
		uint32_t entry = m68k_code_entries[middle].address;
		if (entry == address)
			return m68k_code_entries[middle].function;
		if (entry < address)
			low = middle + 1;
		else
			high = middle;
	}
	return NULL;
}

uint32_t m68k_read_sr(void)
{
	return (SR_SYSTEM & M68K_SR_MASK) |
	       ((uint32_t)cpu.x << 4) |
	       ((uint32_t)cpu.s << 3) |
	       ((uint32_t)cpu.z << 2) |
	       ((uint32_t)cpu.ov << 1) |
	       (uint32_t)cpu.cy;
}

void m68k_write_sr(uint32_t value)
{
	uint32_t old = SR_SYSTEM;
	uint32_t new_value = value & M68K_SR_MASK;

	if (((old ^ new_value) & M68K_SR_S) != 0)
	{
		uint32_t stack = cpu.r[15];
		cpu.r[15] = cpu.r[M68K_OTHER_SP];
		cpu.r[M68K_OTHER_SP] = stack;
	}
	SR_SYSTEM = new_value;

	cpu.x = (uint8_t)((value >> 4) & 1u);
	cpu.s = (uint8_t)((value >> 3) & 1u);
	cpu.z = (uint8_t)((value >> 2) & 1u);
	cpu.ov = (uint8_t)((value >> 1) & 1u);
	cpu.cy = (uint8_t)(value & 1u);
}

static int run_handler(uint32_t vector, uint32_t pc, int level)
{
	uint32_t old_sr = m68k_read_sr();
	uint32_t new_sr = (old_sr & ~0x8000u) | M68K_SR_S;
	uint32_t stack;
	uint32_t handler;
	void (*function)(void);

	if (level)
		new_sr = (new_sr & ~0x0700u) | ((uint32_t)level << 8);
	m68k_write_sr(new_sr);

	stack = cpu.r[15];
	mem_write16(stack - 2, pc & 0xFFFFu);
	mem_write16(stack - 6, old_sr);
	mem_write16(stack - 4, (pc >> 16) & 0xFFFFu);
	cpu.r[15] = stack - 6;

	handler = mem_read32(vector * 4u);
	function = lookup_entry(handler);
	if (!function)
	{
		ddp_program_fault("exception %u: vector points at %06X, which is not translated code",
		                  vector, handler & 0xFFFFFFu);
		return 0;
	}
	cpu.pc = pc;
	function();
	return 1;
}

bool m68k_exception(uint32_t vector, uint32_t pc, uint32_t from)
{
	(void)from;
	if (!run_handler(vector, pc, 0))
		return false;
	return cpu.pc == pc;
}

void m68k_take_interrupts(uint32_t resume)
{
	int level = cave_machine_irq_level(&ddp_machine);
	unsigned mask = (SR_SYSTEM >> 8) & 7u;

	if (level > 0 && (level == 7 || (unsigned)level > mask))
	{
		cave_machine_clear_irq(&ddp_machine);
		run_handler(24u + (uint32_t)level, resume, level);
		cpu.pc = resume;
	}
}

void m68k_stop(uint32_t value)
{
	m68k_write_sr(value);
	/* STOP waits for an interrupt. The board never sends one while the program sits in the
	   unused vectors, so a repeating STOP is a fault rather than a hang. */
	if (++stop_count > 8)
		ddp_program_fault("the translated program executed STOP repeatedly at %06X",
		                  cpu.pc & 0xFFFFFFu);
}

void m68k_reset_devices(void)
{
	/* The board connects no device to the 68000 RESET line. */
}

void m68k_calls_push(uint32_t address)
{
	if (call_depth >= DDP_CALL_STACK_MAX)
	{
		ddp_program_fault("call stack overflow at %06X", cpu.pc & 0xFFFFFFu);
		call_depth = 0;
		return;
	}
	call_stack[call_depth++] = address & 0xFFFFFFu;
}

bool m68k_returned_to(uint32_t expected, uint32_t from)
{
	(void)from;
	expected &= 0xFFFFFFu;
	if (cpu.pc == expected)
	{
		if (call_depth > 0 && call_stack[call_depth - 1] == expected)
			call_depth--;
		return true;
	}
	/* The callee went back somewhere else: this C frame is abandoned too. */
	if (call_depth == 0)
		fprintf(stderr, "ddonpach: top-level return mismatch: pc=%06X expected=%06X from=%06X\n",
		        cpu.pc & 0xFFFFFFu, expected, from & 0xFFFFFFu);
	if (call_depth > 0 && call_stack[call_depth - 1] == expected)
		call_depth--;
	return false;
}

void m68k_call(uint32_t address, uint32_t from)
{
	void (*function)(void);
	(void)from;

	address &= 0xFFFFFFu;
	function = lookup_entry(address);
	if (!function)
	{
		if (collect_gaps())
		{
			record_gap(address, "call");
			/* Continue as if the missing function had returned: the return address is on
			   the shadow stack (m68k_calls_push ran just before this call). */
			if (call_depth > 0)
				cpu.pc = call_stack[call_depth - 1];
			return;
		}
		ddp_program_fault("call to %06X, which is not translated code", address);
		return;
	}
	function();
}

void m68k_jump(uint32_t address, uint32_t from)
{
	void (*function)(void);
	int index;
	(void)from;

	address &= 0xFFFFFFu;

	/* A jump to the return address a call waits for is that call returning. */
	for (index = (int)call_depth - 1; index >= 0; --index)
	{
		if (call_stack[index] == address)
		{
			call_depth = (unsigned)index;
			cpu.pc = address;
			return;
		}
	}

	function = lookup_entry(address);
	if (!function)
	{
		if (collect_gaps())
		{
			record_gap(address, "jump");
			if (call_depth > 0)
				cpu.pc = call_stack[call_depth - 1];
			return;
		}
		ddp_program_fault("jump to %06X, which is not translated code", address);
		return;
	}
	function();
}

uint32_t mem_read8(uint32_t address)
{
	return cave_bus_read8(&ddp_machine, address);
}

uint32_t mem_read16(uint32_t address)
{
	return cave_bus_read16(&ddp_machine, address);
}

uint32_t mem_read32(uint32_t address)
{
	return cave_bus_read32(&ddp_machine, address);
}

void mem_write8(uint32_t address, uint32_t value)
{
	cave_bus_write8(&ddp_machine, address, value);
}

void mem_write16(uint32_t address, uint32_t value)
{
	cave_bus_write16(&ddp_machine, address, value);
}

void mem_write32(uint32_t address, uint32_t value)
{
	cave_bus_write32(&ddp_machine, address, value);
}

/* ---- the ALU helpers (the same results as tools/core/m68000/interpreter.py) ---- */

uint32_t m68k_divu(uint32_t dividend, uint32_t divisor)
{
	uint32_t quotient, remainder;

	divisor &= 0xFFFFu;
	if (!divisor)
		return dividend;
	quotient = dividend / divisor;
	remainder = dividend % divisor;
	if (quotient > 0xFFFFu)
		return dividend;
	return (remainder << 16) | quotient;
}

uint32_t m68k_divu_s(uint32_t dividend, uint32_t divisor)
{
	uint32_t quotient;

	divisor &= 0xFFFFu;
	if (!divisor)
		return (dividend >> 31) & 1u;
	quotient = dividend / divisor;
	if (quotient > 0xFFFFu)
		return 1u;
	return (quotient >> 15) & 1u;
}

uint32_t m68k_divu_z(uint32_t dividend, uint32_t divisor)
{
	uint32_t quotient;

	divisor &= 0xFFFFu;
	if (!divisor)
		return (dividend >> 16) == 0 ? 1u : 0u;
	quotient = dividend / divisor;
	if (quotient > 0xFFFFu)
		return 0u;
	return quotient == 0 ? 1u : 0u;
}

uint32_t m68k_divu_ov(uint32_t dividend, uint32_t divisor)
{
	divisor &= 0xFFFFu;
	if (!divisor)
		return 0u;
	return (dividend / divisor) > 0xFFFFu ? 1u : 0u;
}

static int32_t signed32(uint32_t value)
{
	return (int32_t)value;
}

static int32_t signed16(uint32_t value)
{
	return (int16_t)(value & 0xFFFFu);
}

uint32_t m68k_divs(uint32_t dividend, uint32_t divisor)
{
	int32_t a = signed32(dividend);
	int32_t b = signed16(divisor);
	int32_t quotient, remainder;
	uint32_t q;

	if (!b)
		return dividend;
	quotient = a / b;
	remainder = a % b;
	if (quotient < -0x8000 || quotient > 0x7FFF)
		return dividend;
	q = (uint32_t)quotient & 0xFFFFu;
	return (((uint32_t)remainder & 0xFFFFu) << 16) | q;
}

uint32_t m68k_divs_s(uint32_t dividend, uint32_t divisor)
{
	int32_t a = signed32(dividend);
	int32_t b = signed16(divisor);
	int32_t quotient;

	if (!b)
		return 0u;
	quotient = a / b;
	if (quotient < -0x8000 || quotient > 0x7FFF)
		return 1u;
	return ((uint32_t)quotient >> 15) & 1u;
}

uint32_t m68k_divs_z(uint32_t dividend, uint32_t divisor)
{
	int32_t a = signed32(dividend);
	int32_t b = signed16(divisor);
	int32_t quotient;

	if (!b)
		return 1u;
	quotient = a / b;
	if (quotient < -0x8000 || quotient > 0x7FFF)
		return 0u;
	return quotient == 0 ? 1u : 0u;
}

uint32_t m68k_divs_ov(uint32_t dividend, uint32_t divisor)
{
	int32_t a = signed32(dividend);
	int32_t b = signed16(divisor);
	int32_t quotient;

	if (!b)
		return 0u;
	quotient = a / b;
	return (quotient < -0x8000 || quotient > 0x7FFF) ? 1u : 0u;
}

static uint32_t bcd_add(uint32_t destination, uint32_t source, uint32_t extend,
                        uint32_t *carry, uint32_t *overflow)
{
	uint32_t low = ((destination & 0xF) + (source & 0xF) + (extend & 1)) & 0xFF;
	uint32_t first = (destination + source + (extend & 1)) & 0xFFFF;
	uint32_t result = (first + (low > 9 ? 6 : 0)) & 0xFFFF;
	if (result > 0x9F)
		result = (result + 0x60) & 0xFFFF;
	*carry = (result & 0x300) != 0 ? 1u : 0u;
	*overflow = ((result & 0x80) && !(first & 0x80)) ? 1u : 0u;
	return result & 0xFF;
}

static uint32_t bcd_sub(uint32_t destination, uint32_t source, uint32_t extend,
                        uint32_t *carry, uint32_t *overflow)
{
	uint32_t low = ((destination & 0xF) - (source & 0xF) - (extend & 1)) & 0xFF;
	uint32_t first = (destination - source - (extend & 1)) & 0xFFFF;
	uint32_t result = (first - ((low & 0x10) ? 6 : 0)) & 0xFFFF;
	if (first & 0x100)
		result = (result - 0x60) & 0xFFFF;
	*carry = (result & 0x300) != 0 ? 1u : 0u;
	*overflow = ((result & 0x80) == 0 && (first & 0x80) != 0) ? 1u : 0u;
	return result & 0xFF;
}

uint32_t m68k_abcd(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t carry, overflow;
	return bcd_add(destination, source, extend, &carry, &overflow);
}

uint32_t m68k_abcd_cy(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t carry, overflow;
	(void)bcd_add(destination, source, extend, &carry, &overflow);
	return carry;
}

uint32_t m68k_abcd_ov(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t carry, overflow;
	(void)bcd_add(destination, source, extend, &carry, &overflow);
	return overflow;
}

uint32_t m68k_sbcd(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t carry, overflow;
	return bcd_sub(destination, source, extend, &carry, &overflow);
}

uint32_t m68k_sbcd_cy(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t carry, overflow;
	(void)bcd_sub(destination, source, extend, &carry, &overflow);
	return carry;
}

uint32_t m68k_sbcd_ov(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t carry, overflow;
	(void)bcd_sub(destination, source, extend, &carry, &overflow);
	return overflow;
}

