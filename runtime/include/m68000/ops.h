/**
 *    Name:        ops.h
 *    Description: the 68000's arithmetic, shifts, division, BCD, memory access and register helpers,
 *                 as the translated program's C calls them
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    The semantics follow tools/core/m68000/interpreter.py (the lift's oracle), which follows MAME's
 *    68000 core (Olivier Galibert). No 68000 instruction decoder is present: the generated C already
 *    spells each instruction out, and these helpers only compute the ALU results it asks for.
 */
#ifndef M68000_OPS_H
#define M68000_OPS_H

#include <stdint.h>

#include "m68000/cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The board's bus, as the 68000 sees it. A byte is one byte of the 16-bit bus; a long is its high
   word then its low one. */
uint32_t mem_read8(uint32_t address);
uint32_t mem_read16(uint32_t address);
uint32_t mem_read32(uint32_t address);
void mem_write8(uint32_t address, uint32_t value);
void mem_write16(uint32_t address, uint32_t value);
void mem_write32(uint32_t address, uint32_t value);

/* The C the translator writes shifts 32-bit values with these. */
static inline uint32_t shl32(uint32_t value, int count)
{
	return value << (count & 31);
}

static inline uint32_t shr32(uint32_t value, int count)
{
	return value >> (count & 31);
}

/* A shift of 8, 16 or 32 bits: M68K_ASL, M68K_ASR, M68K_LSL, M68K_LSR, M68K_ROL, M68K_ROR,
   M68K_ROXL, M68K_ROXR. The value is the result, the carry flag the last bit shifted out, and
   the overflow flag is set for an ASL whose sign bit changed. */
enum
{
	M68K_ASL = 0,
	M68K_ASR,
	M68K_LSL,
	M68K_LSR,
	M68K_ROL,
	M68K_ROR,
	M68K_ROXL,
	M68K_ROXR
};

static inline uint32_t m68k_shift(int kind, uint32_t value, uint32_t count, uint32_t extend, int bits)
{
	uint32_t top = 1u << (bits - 1);
	uint32_t full = bits == 32 ? 0xFFFFFFFFu : ((1u << bits) - 1u);
	uint32_t carry = (kind == M68K_ROXL || kind == M68K_ROXR) ? (extend & 1u) : 0u;
	uint32_t overflow = 0;
	unsigned i;

	value &= full;
	count &= 63;
	for (i = 0; i < count; ++i)
	{
		uint32_t out, shifted;
		if (kind == M68K_ASL || kind == M68K_LSL || kind == M68K_ROL || kind == M68K_ROXL)
		{
			out = (value & top) ? 1u : 0u;
			shifted = (value << 1) & full;
			if (kind == M68K_ROL)
				shifted |= out;
			else if (kind == M68K_ROXL)
			{
				shifted |= carry & 1u;
				carry = out;
			}
			if (kind == M68K_ASL && ((shifted ^ value) & top))
				overflow = 1;
		}
		else
		{
			out = value & 1u;
			shifted = value >> 1;
			if (kind == M68K_ASR)
				shifted |= value & top;
			else if (kind == M68K_ROR)
				shifted |= out << (bits - 1);
			else if (kind == M68K_ROXR)
			{
				shifted |= (carry & 1u) << (bits - 1);
				carry = out;
			}
		}
		value = shifted;
		carry = out;
	}
	return value & full;
}

static inline uint32_t m68k_shift_carry(int kind, uint32_t value, uint32_t count, uint32_t extend, int bits)
{
	uint32_t top = 1u << (bits - 1);
	uint32_t full = bits == 32 ? 0xFFFFFFFFu : ((1u << bits) - 1u);
	uint32_t carry = (kind == M68K_ROXL || kind == M68K_ROXR) ? (extend & 1u) : 0u;
	unsigned i;

	value &= full;
	count &= 63;
	for (i = 0; i < count; ++i)
	{
		uint32_t out;
		if (kind == M68K_ASL || kind == M68K_LSL || kind == M68K_ROL || kind == M68K_ROXL)
		{
			out = (value & top) ? 1u : 0u;
			value = (value << 1) & full;
			if (kind == M68K_ROL)
				value |= out;
			else if (kind == M68K_ROXL)
			{
				value |= carry & 1u;
				carry = out;
			}
		}
		else
		{
			uint32_t sign = value & top;
			out = value & 1u;
			value >>= 1;
			if (kind == M68K_ASR)
				value |= sign;
			else if (kind == M68K_ROR)
				value |= out << (bits - 1);
			else if (kind == M68K_ROXR)
			{
				value |= (carry & 1u) << (bits - 1);
				carry = out;
			}
		}
		carry = out;
	}
	return carry & 1u;
}

static inline uint32_t m68k_shift_overflow(int kind, uint32_t value, uint32_t count, uint32_t extend, int bits)
{
	uint32_t top = 1u << (bits - 1);
	uint32_t full = bits == 32 ? 0xFFFFFFFFu : ((1u << bits) - 1u);
	uint32_t overflow = 0;
	unsigned i;

	if (kind != M68K_ASL)
		return 0;
	value &= full;
	count &= 63;
	for (i = 0; i < count; ++i)
	{
		uint32_t shifted = (value << 1) & full;
		if ((shifted ^ value) & top)
			overflow = 1;
		value = shifted;
	}
	return overflow;
}

/* The carry and overflow flags of ADD/SUB, as the core computes them. */
static inline uint32_t m68k_carry_add8(uint32_t a, uint32_t b, uint32_t carry)
{
	return (((a & 0xFFu) + (b & 0xFFu) + carry) > 0xFFu) ? 1u : 0u;
}

static inline uint32_t m68k_overflow_add8(uint32_t a, uint32_t b, uint32_t carry)
{
	uint32_t result = (a + b + carry) & 0xFFu;
	return (((a ^ result) & (b ^ result) & 0x80u) != 0u) ? 1u : 0u;
}

static inline uint32_t m68k_carry_add16(uint32_t a, uint32_t b, uint32_t carry)
{
	return (((a & 0xFFFFu) + (b & 0xFFFFu) + carry) > 0xFFFFu) ? 1u : 0u;
}

static inline uint32_t m68k_overflow_add16(uint32_t a, uint32_t b, uint32_t carry)
{
	uint32_t result = (a + b + carry) & 0xFFFFu;
	return (((a ^ result) & (b ^ result) & 0x8000u) != 0u) ? 1u : 0u;
}

static inline uint32_t m68k_carry_add32(uint32_t a, uint32_t b, uint32_t carry)
{
	return ((((uint64_t)a + (uint64_t)b + carry) >> 32) != 0u) ? 1u : 0u;
}

static inline uint32_t m68k_overflow_add32(uint32_t a, uint32_t b, uint32_t carry)
{
	uint32_t result = a + b + carry;
	return ((((a ^ result) & (b ^ result)) & 0x80000000u) != 0u) ? 1u : 0u;
}

static inline uint32_t m68k_carry_sub8(uint32_t a, uint32_t b, uint32_t borrow)
{
	return ((a & 0xFFu) < ((b & 0xFFu) + borrow)) ? 1u : 0u;
}

static inline uint32_t m68k_overflow_sub8(uint32_t a, uint32_t b, uint32_t borrow)
{
	uint32_t result = (a - b - borrow) & 0xFFu;
	return ((((a ^ b) & (a ^ result)) & 0x80u) != 0u) ? 1u : 0u;
}

static inline uint32_t m68k_carry_sub16(uint32_t a, uint32_t b, uint32_t borrow)
{
	return ((a & 0xFFFFu) < ((b & 0xFFFFu) + borrow)) ? 1u : 0u;
}

static inline uint32_t m68k_overflow_sub16(uint32_t a, uint32_t b, uint32_t borrow)
{
	uint32_t result = (a - b - borrow) & 0xFFFFu;
	return ((((a ^ b) & (a ^ result)) & 0x8000u) != 0u) ? 1u : 0u;
}

static inline uint32_t m68k_carry_sub32(uint32_t a, uint32_t b, uint32_t borrow)
{
	return ((uint64_t)a < ((uint64_t)b + borrow)) ? 1u : 0u;
}

static inline uint32_t m68k_overflow_sub32(uint32_t a, uint32_t b, uint32_t borrow)
{
	uint32_t result = a - b - borrow;
	return ((((a ^ b) & (a ^ result)) & 0x80000000u) != 0u) ? 1u : 0u;
}

/* DIVU.W and DIVS.W: the remainder in the high word, the quotient in the low one. The dividend is
   unchanged when the quotient does not fit; a division by zero leaves it too. */
uint32_t m68k_divu(uint32_t dividend, uint32_t divisor);
uint32_t m68k_divu_s(uint32_t dividend, uint32_t divisor);
uint32_t m68k_divu_z(uint32_t dividend, uint32_t divisor);
uint32_t m68k_divu_ov(uint32_t dividend, uint32_t divisor);

uint32_t m68k_divs(uint32_t dividend, uint32_t divisor);
uint32_t m68k_divs_s(uint32_t dividend, uint32_t divisor);
uint32_t m68k_divs_z(uint32_t dividend, uint32_t divisor);
uint32_t m68k_divs_ov(uint32_t dividend, uint32_t divisor);

/* ABCD/SBCD: the BCD result, its carry and its overflow. */
uint32_t m68k_abcd(uint32_t destination, uint32_t source, uint32_t extend);
uint32_t m68k_abcd_cy(uint32_t destination, uint32_t source, uint32_t extend);
uint32_t m68k_abcd_ov(uint32_t destination, uint32_t source, uint32_t extend);

uint32_t m68k_sbcd(uint32_t destination, uint32_t source, uint32_t extend);
uint32_t m68k_sbcd_cy(uint32_t destination, uint32_t source, uint32_t extend);
uint32_t m68k_sbcd_ov(uint32_t destination, uint32_t source, uint32_t extend);

#ifdef __cplusplus
}
#endif

#endif
