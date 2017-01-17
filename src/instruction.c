#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <assert.h>
#include <string.h>

#include "bus.h"
#include "65c02isa.h"
#include "instruction.h"

instruction_t
instruction_fetch(bus_t *b, uint16_t addr)
{
	instruction_t i;
	uint8_t op;

	op = bus_read_1(b, addr);
	i.def = instrdef_get(op);

	assert(i.def.opcode != OP_UNIMPL);

	/* handle operands */		
	switch (i.def.mode) {
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

void
instruction_print(instruction_t *i)
{
	switch (i->def.mode) {
	case IMPLIED:
		printf("%s", i->def.mnemonic);
		break;
	case IMMEDIATE:
		printf("%s #%X", i->def.mnemonic, i->op1);
		break;
	case ZP:
		printf("%s %X", i->def.mnemonic, i->op1);
		break;
	case ZPX:
		printf("%s %X,X", i->def.mnemonic, i->op1);
		break;
	case ZPY:
		printf("%s %X,Y", i->def.mnemonic, i->op1);
		break;
	case IZP:
		printf("%s (%X)", i->def.mnemonic, i->op1);
		break;
	case IZPX:
		printf("%s (%X,X)", i->def.mnemonic, i->op1);
		break;
	case IZPY:
		printf("%s (%X),Y", i->def.mnemonic, i->op1);
		break;
	case ABSOLUTE:
		printf("%s %02X%02X", i->def.mnemonic, i->op2, i->op1);
		break;
	case ABSOLUTEX:
		printf("%s %02X%02X,X", i->def.mnemonic, i->op2, i->op1);
		break;
	case ABSOLUTEY:
		printf("%s %02X%02X,Y", i->def.mnemonic, i->op2, i->op1);
		break;
	case IABSOLUTE:
		printf("%s (%02X%02X)", i->def.mnemonic, i->op2, i->op1);
		break;
	case IABSOLUTEX:
		printf("%s (%02X%02X,X)", i->def.mnemonic, i->op2, i->op1);
		break;
	case RELATIVE:
		printf("%s %02X%02X", i->def.mnemonic, i->op2, i->op1);
		break;
	}
}

void
disassemble(bus_t *b, uint16_t addr) 
{
	instruction_t i;

	i = instruction_fetch(b, addr);

	printf("%X:\t", addr);
	instruction_print(&i);
	printf("\t\t// %X", i.def.opcode);
	printf("\n");
}

