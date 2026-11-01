/**
 *    Name:        eeprom.c
 *    Description: 93C46 serial EEPROM (16-bit organisation)
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    MAME's eeprom_serial_93cxx_device: a start bit, a two-bit opcode, a six-bit address, then the
 *    data. Data out changes after a clock edge and is read on IN1 bit 11 (0x0800).
 */
#include "cave/eeprom.h"

#include <string.h>

enum
{
	EE_IDLE,
	EE_OPCODE,
	EE_ADDRESS,
	EE_READ,
	EE_WRITE,
	EE_WRITE_DONE
};

enum
{
	EE_OP_READ = 2,
	EE_OP_WRITE = 1,
	EE_OP_ERASE = 3,
	EE_OP_ERASE_ALL = 0
};

static uint16_t word_at(const cave_eeprom *eeprom, int address)
{
	if (address < 0 || address >= 64)
		return 0xFFFF;
	return eeprom->words[address];
}

void cave_eeprom_init(cave_eeprom *eeprom, const uint8_t image[128])
{
	int i;
	memset(eeprom, 0, sizeof(*eeprom));
	for (i = 0; i < 64; ++i)
		eeprom->words[i] = (uint16_t)((image[i * 2] << 8) | image[i * 2 + 1]);
	memcpy(eeprom->image, image, 128);
	eeprom->do_out = 1;
	eeprom->state = EE_IDLE;
}

static void eeprom_begin(cave_eeprom *eeprom)
{
	eeprom->state = EE_IDLE;
	eeprom->opcode = 0;
	eeprom->bits = 0;
	eeprom->address = 0;
	eeprom->value = 0;
	eeprom->read_bits = -1;
	eeprom->do_out = 1;
}

static void eeprom_execute(cave_eeprom *eeprom)
{
	switch (eeprom->opcode & 3)
	{
	case EE_OP_READ:
		eeprom->state = EE_READ;
		eeprom->read_bits = -1;
		eeprom->do_out = 0;             /* the dummy zero before D15 */
		break;
	case EE_OP_WRITE:
		eeprom->state = EE_WRITE;
		eeprom->bits = 0;
		eeprom->value = 0;
		break;
	case EE_OP_ERASE:
		if (eeprom->address < 64)
			eeprom->words[eeprom->address] = 0xFFFF;
		eeprom->state = EE_IDLE;
		eeprom->do_out = 1;
		break;
	default:
		/* 00 11xxxx enables writes, 00 00xxxx disables them; the game never locks the array,
		   so an all-zero address is treated as erase-all only when it follows a real command. */
		if ((eeprom->address & 0x30) == 0x30)
		{
			eeprom->state = EE_IDLE;
		}
		else if (eeprom->address == 0)
		{
			int i;
			for (i = 0; i < 64; ++i)
				eeprom->words[i] = 0xFFFF;
			eeprom->state = EE_IDLE;
		}
		else
		{
			eeprom->state = EE_IDLE;
		}
		eeprom->do_out = 1;
		break;
	}
}

static void eeprom_clock(cave_eeprom *eeprom, int di)
{
	switch (eeprom->state)
	{
	case EE_IDLE:
		if (di)
		{
			eeprom->state = EE_OPCODE;
			eeprom->opcode = 0;
			eeprom->bits = 0;
		}
		break;

	case EE_OPCODE:
		eeprom->opcode = (eeprom->opcode << 1) | (di & 1);
		if (++eeprom->bits == 2)
		{
			eeprom->state = EE_ADDRESS;
			eeprom->bits = 0;
			eeprom->address = 0;
		}
		break;

	case EE_ADDRESS:
		eeprom->address = (eeprom->address << 1) | (di & 1);
		if (++eeprom->bits == 6)
			eeprom_execute(eeprom);
		break;

	case EE_READ:
		if (eeprom->read_bits < 0)
		{
			eeprom->do_out = 0;
			eeprom->read_bits = 0;
		}
		else if (eeprom->read_bits < 16)
		{
			eeprom->do_out = (word_at(eeprom, eeprom->address) >> (15 - eeprom->read_bits)) & 1;
			eeprom->read_bits++;
		}
		else
		{
			eeprom->do_out = 0;
		}
		break;

	case EE_WRITE:
		eeprom->value = (uint16_t)((eeprom->value << 1) | (di & 1));
		if (++eeprom->bits == 16)
			eeprom->state = EE_WRITE_DONE;
		break;

	default:
		break;
	}
}

void cave_eeprom_write(cave_eeprom *eeprom, uint8_t value)
{
	int cs = (value >> 1) & 1;
	int clk = (value >> 2) & 1;
	int di = (value >> 3) & 1;

	if (cs && !eeprom->cs)
		eeprom_begin(eeprom);

	if (cs && clk && !eeprom->clk)
		eeprom_clock(eeprom, di);

	if (!cs && eeprom->cs)
	{
		if (eeprom->state == EE_WRITE_DONE && eeprom->address < 64)
			eeprom->words[eeprom->address] = eeprom->value;
		eeprom->state = EE_IDLE;
		eeprom->do_out = 1;
	}

	eeprom->cs = cs;
	eeprom->clk = clk;
	eeprom->di = di;
}

int cave_eeprom_do(const cave_eeprom *eeprom)
{
	return eeprom->do_out & 1;
}

void cave_eeprom_save_image(const cave_eeprom *eeprom, uint8_t image[128])
{
	int i;
	for (i = 0; i < 64; ++i)
	{
		image[i * 2] = (uint8_t)(eeprom->words[i] >> 8);
		image[i * 2 + 1] = (uint8_t)(eeprom->words[i] & 0xFF);
	}
}
