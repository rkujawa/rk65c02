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
		printf("%s #%X", id.mnemonic, i->op1);
		break;
	case ZP:
		printf("%s %X", id.mnemonic, i->op1);
		break;
	case ZPX:
		printf("%s %X,X", id.mnemonic, i->op1);
		break;
	case ZPY:
		printf("%s %X,Y", id.mnemonic, i->op1);
		break;
	case IZP:
		printf("%s (%X)", id.mnemonic, i->op1);
		break;
	case IZPX:
		printf("%s (%X,X)", id.mnemonic, i->op1);
		break;
	case IZPY:
		printf("%s (%X),Y", id.mnemonic, i->op1);
		break;
	case ABSOLUTE:
		printf("%s %02X%02X", id.mnemonic, i->op2, i->op1);
		break;
	case ABSOLUTEX:
		printf("%s %02X%02X,X", id.mnemonic, i->op2, i->op1);
		break;
	case ABSOLUTEY:
		printf("%s %02X%02X,Y", id.mnemonic, i->op2, i->op1);
		break;
	case IABSOLUTE:
		printf("%s (%02X%02X)", id.mnemonic, i->op2, i->op1);
		break;
	case IABSOLUTEX:
		printf("%s (%02X%02X,X)", id.mnemonic, i->op2, i->op1);
		break;
	case RELATIVE:
		printf("%s %02X%02X", id.mnemonic, i->op2, i->op1);
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
	printf("\t\t// %X", id.opcode);
	printf("\n");
}

instrdef_t
instruction_decode(uint8_t opcode)
{
	instrdef_t id;

	id = instrs[opcode];

	return id;
}

