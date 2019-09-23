/*
 *      SPDX-License-Identifier: GPL-3.0-only
 *
 *      rk65c02
 *      Copyright (C) 2017-2019  Radoslaw Kujawa
 *
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, version 3 of the License.
 * 
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include <assert.h>
#include <string.h>

#include <gc/gc.h>

#include "bus.h"
#include "rk65c02.h"
#include "65c02isa.h"
#include "log.h"
#include "instruction.h"

instruction_t
instruction_fetch(bus_t *b, uint16_t addr)
{
	instruction_t i;
	instrdef_t id;

	i.opcode = bus_read_1(b, addr);
	id = instruction_decode(i.opcode);

	//assert(i.def.opcode != OP_UNIMPL);

	/* handle operands */		
	switch (id.mode) {
	case IMMEDIATE:
	case ZP:
	case ZPX:
	case ZPY:
	case IZP:
	case IZPX:
	case IZPY:
	case RELATIVE:
		i.op1 = bus_read_1(b, addr+1);
		break;
	case ABSOLUTE:
	case ABSOLUTEX:
	case ABSOLUTEY:
	case IABSOLUTE:
	case IABSOLUTEX:
	case ZPR:
		i.op1 = bus_read_1(b, addr+1);
		i.op2 = bus_read_1(b, addr+2);
		break;
	case IMPLIED:
	default:
		break;
	}

	return i;
}

void
instruction_print(instruction_t *i)
{
	char *str;

	str = instruction_string_get(i);

	printf("%s", str);
}

char *
instruction_string_get(instruction_t *i)
{
#define INSTR_STR_LEN	16
	instrdef_t id;
	char *str;

	str = GC_MALLOC(INSTR_STR_LEN);
	assert(str != NULL);
	memset(str, 0, INSTR_STR_LEN);

	id = instruction_decode(i->opcode);
	switch (id.mode) {
	case IMPLIED:
		snprintf(str, INSTR_STR_LEN, "%s", id.mnemonic);
		break;
	case ACCUMULATOR:
		snprintf(str, INSTR_STR_LEN, "%s A", id.mnemonic);
		break;
	case IMMEDIATE:
		snprintf(str, INSTR_STR_LEN, "%s #%#02x", id.mnemonic, i->op1);
		break;
	case ZP:
		snprintf(str, INSTR_STR_LEN, "%s %#02x", id.mnemonic, i->op1);
		break;
	case ZPX:
		snprintf(str, INSTR_STR_LEN, "%s %#02x,X", id.mnemonic, i->op1);
		break;
	case ZPY:
		snprintf(str, INSTR_STR_LEN, "%s %#02x,Y", id.mnemonic, i->op1);
		break;
	case IZP:
		snprintf(str, INSTR_STR_LEN, "%s (%#02x)", id.mnemonic, i->op1);
		break;
	case IZPX:
		snprintf(str, INSTR_STR_LEN, "%s (%#02x,X)", id.mnemonic, i->op1);
		break;
	case IZPY:
		snprintf(str, INSTR_STR_LEN, "%s (%#02x),Y", id.mnemonic, i->op1);
		break;
	case ZPR:
		snprintf(str, INSTR_STR_LEN, "%s %#02x,%#02x", id.mnemonic, i->op1, i->op2);
		break;
	case ABSOLUTE:
		snprintf(str, INSTR_STR_LEN, "%s %#02x%02x", id.mnemonic, i->op2, i->op1);
		break;
	case ABSOLUTEX:
		snprintf(str, INSTR_STR_LEN, "%s %#02x%02x,X", id.mnemonic, i->op2, i->op1);
		break;
	case ABSOLUTEY:
		snprintf(str, INSTR_STR_LEN, "%s %#02x%02x,Y", id.mnemonic, i->op2, i->op1);
		break;
	case IABSOLUTE:
		snprintf(str, INSTR_STR_LEN, "%s (%#02x%02x)", id.mnemonic, i->op2, i->op1);
		break;
	case IABSOLUTEX:
		snprintf(str, INSTR_STR_LEN, "%s (%#02x%02x,X)", id.mnemonic, i->op2, i->op1);
		break;
	case RELATIVE:
		snprintf(str, INSTR_STR_LEN, "%s %#02x", id.mnemonic, i->op1);
		break;
	}

	return str;
}

void
disassemble(bus_t *b, uint16_t addr) 
{
	instruction_t i;
	instrdef_t id;

	i = instruction_fetch(b, addr);
	id = instruction_decode(i.opcode);

	printf("%X:\t", addr);
	instruction_print(&i);
	printf("\t\t// ");

	if (id.size == 1)
		printf("%X", id.opcode);
	else if (id.size == 2)
		printf("%X %X", id.opcode, i.op1);
	else if (id.size == 3)
		printf("%X %X %X", id.opcode, i.op1, i.op2);
	printf("\n");
}

instrdef_t
instruction_decode(uint8_t opcode)
{
	instrdef_t id;

	id = instrs[opcode];

	return id;
}

void
instruction_status_adjust_zero(rk65c02emu_t *e, uint8_t regval)
{
	if (regval == 0)
		e->regs.P |= P_ZERO;
	else
		e->regs.P &= ~P_ZERO;
}

void
instruction_status_adjust_negative(rk65c02emu_t *e, uint8_t regval)
{
	if (regval & NEGATIVE)
		e->regs.P |= P_NEGATIVE;
	else    
		e->regs.P &= ~P_NEGATIVE;
}

void
instruction_data_write_1(rk65c02emu_t *e, instrdef_t *id, instruction_t *i, uint8_t val)
{
	if (id->mode == ACCUMULATOR) {
		e->regs.A = val;
		return;
	}

	if (id->mode == IMMEDIATE) {
		rk65c02_panic(e,
		    "invalid IMMEDIATE addressing mode for opcode %x\n",
		    i->opcode);
		return;
	}

	bus_write_1(e->bus, instruction_data_address(e, id, i), val);
}

uint8_t
instruction_data_read_1(rk65c02emu_t *e, instrdef_t *id, instruction_t *i)
{
	if (id->mode == ACCUMULATOR)
		return e->regs.A;
	else if (id->mode == IMMEDIATE)
		return i->op1;

	return bus_read_1(e->bus, instruction_data_address(e, id, i));
}

uint16_t
instruction_data_address(rk65c02emu_t *e, instrdef_t *id, instruction_t *i)
{
	uint16_t addr;

	addr = 0;

	switch (id->mode) {
	case ZP:
	case ZPR:
		addr = i->op1;
		break;
	case ZPX:
		addr = ((uint8_t) (i->op1 + e->regs.X));
		break;
	case ZPY:
		addr = i->op1 + e->regs.Y;
		break;
	case IZP:
		addr = bus_read_1(e->bus, i->op1);
		addr |= (bus_read_1(e->bus, i->op1 + 1) << 8);
		break;
	case IZPX: /* Zero Page Indexed Indirect with X */
		addr = bus_read_1(e->bus, (uint8_t) (i->op1 + e->regs.X));
		addr |= (bus_read_1(e->bus, (uint8_t) (i->op1 + e->regs.X + 1)) << 8);
		break;
	case IZPY: /* Zero Page Indirect Indexed with Y */
		addr = bus_read_1(e->bus, i->op1);
		addr |= (bus_read_1(e->bus, i->op1 + 1) << 8);
		addr += e->regs.Y;
		break;
	case ABSOLUTE:
		addr = i->op1 + (i->op2 << 8);
		break;
	case ABSOLUTEX:
		addr = i->op1 + (i->op2 << 8) + e->regs.X;
		break;
	case ABSOLUTEY:
		addr = i->op1 + (i->op2 << 8) + e->regs.Y;
		break;
	case IABSOLUTE:
	case IABSOLUTEX:
	case RELATIVE:
		/* 
		 * IABSOLUTE, IABSOLUTEX, RELATIVE are only for branches
		 * and jumps. They do not read or write anything, only modify
		 * PC which is handled within emulation of a given opcode.
		 */
	default:
		rk65c02_panic(e, "unhandled addressing mode for opcode %x\n",
		    i->opcode);
		break;
	}

	return addr;
}

/* put value onto the stack */
void
stack_push(rk65c02emu_t *e, uint8_t val)
{
	bus_write_1(e->bus, STACK_START+e->regs.SP, val);
	e->regs.SP--;
}

/* pull/pop value from the stack */
uint8_t
stack_pop(rk65c02emu_t *e)
{
	uint8_t val;

	e->regs.SP++;
	val = bus_read_1(e->bus, STACK_START+e->regs.SP);

	return val;
}

/* increment program counter based on instruction size (opcode + operands) */ 
void
program_counter_increment(rk65c02emu_t *e, instrdef_t *id)
{
	e->regs.PC += id->size;
}	

void
program_counter_branch(rk65c02emu_t *e, int8_t boffset)
{
	e->regs.PC += boffset + 2;
}

/* check whether given instruction modify program counter */
bool
instruction_modify_pc(instrdef_t *id)
{
	return id->modify_pc;
}

/* find instr definition (and opcode) searching by mnemonic and addr mode */
bool
instruction_opcode_by_mnemonic(char *mnemonic, addressing_t mode, uint8_t *opcode, instrdef_t *id)
{
	bool found;

	found = false;

	while ((*opcode) <= 0xFF)  { /* this is stupid */
		*id = instruction_decode(*opcode);
		if ((strcmp(mnemonic, id->mnemonic) == 0) && (id->mode == mode)) {
			found = true;
			break;
		}
		(*opcode)++;
	}

	return found;
}

