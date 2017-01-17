#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <assert.h>
#include <string.h>

#include "rk65c02.h"
#include "bus.h"

static bool run = false;

instruction_t
instruction_fetch(bus_t *b, uint16_t addr)
{
	instruction_t i;

	i.opcode = bus_read_1(b, addr);
	i.def = instrs[i.opcode];

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
	printf("\t\t// %X", i.opcode);
	printf("\n");
}

void
rk6502_start(bus_t *b, uint16_t addr) {
	reg_state_t r;
	instruction_t i;

	r.PC = addr;

	run = true;
	while (run) {
		disassemble(b, r.PC);
		i = instruction_fetch(b, r.PC);

		//execute(i, r);

		if (i.opcode == OP_STP)
			run = false;

		r.PC += i.def.size;
	}
}
/*
int
main(void)
{
	bus_t b;

	b = bus_init();

	bus_write_1(&b, 0, OP_INX);
	bus_write_1(&b, 1, OP_NOP);
	bus_write_1(&b, 2, OP_LDY_IMM);
	bus_write_1(&b, 3, 0x1);
	bus_write_1(&b, 4, OP_TSB_ZP);
	bus_write_1(&b, 5, 0x3);
	bus_write_1(&b, 6, OP_JSR);
	bus_write_1(&b, 7, 0x09);
	bus_write_1(&b, 8, 0x0);
	bus_write_1(&b, 9, OP_STP);

	rk6502_start(&b, 0);

	bus_finish(&b);
}
*/
