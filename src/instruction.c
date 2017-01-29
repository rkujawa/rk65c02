#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <assert.h>
#include <string.h>

#include "bus.h"
#include "rk65c02.h"
#include "65c02isa.h"
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
		i.op1 = bus_read_1(b, addr+1);
		i.op2 = bus_read_1(b, addr+2);
		break;
	case IMPLIED:
	default:
		break;
	}

	return i;
}

/*void
instruction_execute(rk65c02emu_t *e, instruction_t *i)
{
	id.emul();	
	e->regs.PC += id.size;
}*/

void
instruction_print(instruction_t *i)
{
	instrdef_t id;

	id = instruction_decode(i->opcode);
	switch (id.mode) {
	case IMPLIED:
		printf("%s", id.mnemonic);
		break;
	case ACCUMULATOR:
		printf("%s A", id.mnemonic);
		break;
	case IMMEDIATE:
		printf("%s #%#02x", id.mnemonic, i->op1);
		break;
	case ZP:
		printf("%s %#02x", id.mnemonic, i->op1);
		break;
	case ZPX:
		printf("%s %#02x,X", id.mnemonic, i->op1);
		break;
	case ZPY:
		printf("%s %#02x,Y", id.mnemonic, i->op1);
		break;
	case IZP:
		printf("%s (%#02x)", id.mnemonic, i->op1);
		break;
	case IZPX:
		printf("%s (%#02x,X)", id.mnemonic, i->op1);
		break;
	case IZPY:
		printf("%s (%#02x),Y", id.mnemonic, i->op1);
		break;
	case ZPR:
		printf("%s %#02x,%#02x", id.mnemonic, i->op1, i->op2);
		break;
	case ABSOLUTE:
		printf("%s %#02x%02x", id.mnemonic, i->op2, i->op1);
		break;
	case ABSOLUTEX:
		printf("%s %#02x%02x,X", id.mnemonic, i->op2, i->op1);
		break;
	case ABSOLUTEY:
		printf("%s %#02x%02x,Y", id.mnemonic, i->op2, i->op1);
		break;
	case IABSOLUTE:
		printf("%s (%#02x%02x)", id.mnemonic, i->op2, i->op1);
		break;
	case IABSOLUTEX:
		printf("%s (%#02x%02x,X)", id.mnemonic, i->op2, i->op1);
		break;
	case RELATIVE:
		printf("%s %#02x", id.mnemonic, i->op1);
		break;
	}
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
	uint16_t iaddr;

	switch (id->mode) {
	case ZP:
		bus_write_1(e->bus, i->op1, val);
		break;
	case ZPX:
		/* XXX: wraps around zero page? */
		bus_write_1(e->bus, i->op1 + e->regs.X, val);
		break;
	case ZPY:
		bus_write_1(e->bus, i->op1 + e->regs.Y, val);
		break;
	case IZP:
		iaddr = bus_read_1(e->bus, i->op1);
		iaddr |= (bus_read_1(e->bus, i->op1 + 1) << 8);
		bus_write_1(e->bus, iaddr, val);
		break;
	case ABSOLUTE:
		bus_write_1(e->bus, i->op1 + (i->op2 << 8), val);
		break;
	case IZPX:
		/* XXX */
		iaddr = bus_read_1(e->bus, i->op1 + e->regs.X);
		iaddr |= (bus_read_1(e->bus, i->op1 + e->regs.X + 1) << 8);
		bus_write_1(e->bus, iaddr, val);
		break;
	case IZPY:
		/* XXX */
		iaddr = bus_read_1(e->bus, i->op1);
		iaddr |= (bus_read_1(e->bus, i->op1 + 1) << 8);
		bus_write_1(e->bus, iaddr, val + e->regs.Y);
		break;
	case ABSOLUTEX:
		bus_write_1(e->bus, (i->op1 + (i->op2 << 8)) + e->regs.X, val);
		break;
	case ABSOLUTEY:
		bus_write_1(e->bus, (i->op1 + (i->op2 << 8)) + e->regs.Y, val);
		break;
	case ACCUMULATOR:
		e->regs.A = val;
		break;
	case ZPR:
		/* XXX */
		break;
	case IMMEDIATE:
	case RELATIVE:
	case IABSOLUTE:
	case IABSOLUTEX:
		/* 
		 * IABSOLUTE, IABSOLUTEX, RELATIVE are only for branches
		 * and jumps. They do not read or write anything, only modify
		 * PC which is handled within emulation of a given opcode.
		 */
	default:
		printf("unhandled addressing mode for opcode %x\n",
		    i->opcode);
		break;
	}
}

uint8_t
instruction_data_read_1(rk65c02emu_t *e, instrdef_t *id, instruction_t *i)
{
	uint8_t rv;	/* data read from the bus */
	uint16_t iaddr; /* indirect address */

	rv = 0;

	switch (id->mode) {
	case ACCUMULATOR:
		rv = e->regs.A;
		break;
	case IMMEDIATE:
		rv = i->op1;
		break;
	case ZP:
		rv = bus_read_1(e->bus, i->op1);
		break;
	case ZPX:
		/* XXX: wraps around zero page? */
		rv = bus_read_1(e->bus, i->op1 + e->regs.X);
		break;
	case ZPY:
		rv = bus_read_1(e->bus, i->op1 + e->regs.Y);
		break;
	case IZP:
		iaddr = bus_read_1(e->bus, i->op1);
		iaddr |= (bus_read_1(e->bus, i->op1 + 1) << 8);
		rv = bus_read_1(e->bus, iaddr);
		break;
	case IZPX:
		/* XXX: what about page wraps / roll over */
		iaddr = bus_read_1(e->bus, i->op1 + e->regs.X);
		iaddr |= (bus_read_1(e->bus, i->op1 + e->regs.X + 1) << 8);
		rv = bus_read_1(e->bus, iaddr);
		break;
	case IZPY:
		/* XXX: what about page wraps / roll over */
		iaddr = bus_read_1(e->bus, i->op1);
		iaddr |= (bus_read_1(e->bus, i->op1 + 1) << 8);
		rv = bus_read_1(e->bus, iaddr) + e->regs.Y;
		break;
	case ABSOLUTE:
		rv = bus_read_1(e->bus, i->op1 + (i->op2 << 8));
		break;
	case ABSOLUTEX:
		rv = bus_read_1(e->bus, (i->op1 + (i->op2 << 8)) + e->regs.X);
		break;
	case ABSOLUTEY:
		rv = bus_read_1(e->bus, (i->op1 + (i->op2 << 8)) + e->regs.Y);
		break;
	case ZPR:
		/* XXX */
	case IABSOLUTE:
	case IABSOLUTEX:
	case RELATIVE:
		/* 
		 * IABSOLUTE, IABSOLUTEX, RELATIVE are only for branches
		 * and jumps. They do not read or write anything, only modify
		 * PC which is handled within emulation of a given opcode.
		 */
	default:
		printf("unhandled addressing mode for opcode %x\n",
		    i->opcode);
		break;
	}

	return rv;
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

