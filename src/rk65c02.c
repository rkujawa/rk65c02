#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <assert.h>
#include <string.h>

#include "rk65c02.h"
#include "bus.h"

static bool run = false;

struct reg_state {
	uint8_t A;	/* accumulator */
	uint8_t X;	/* index X */
	uint8_t Y;	/* index Y */

	uint16_t PC;	/* program counter */
	uint8_t SP;	/* stack pointer */
	uint8_t P;	/* status */
};

typedef struct reg_state reg_state_t;

instruction_t
instruction_fetch(bus_t *b, uint16_t addr)
{
	instruction_t i;

	i.opcode = bus_read_1(b, addr);
	i.def = instrs[i.opcode];

	assert(i.def.op != OP_UNIMPL);

	/* handle operands */		
	switch (i.def.mode) {
	case ADDR_IMMEDIATE:
	case ADDR_ZP:
	case ADDR_ZPX:
	case ADDR_ZPY:
	case ADDR_IZP:
	case ADDR_IZPX:
	case ADDR_IZPY:
	case ADDR_RELATIVE:
		i.op1 = bus_read_1(b, addr+1);
		break;
	case ADDR_ABSOLUTE:
	case ADDR_ABSOLUTEX:
	case ADDR_ABSOLUTEY:
	case ADDR_IABSOLUTE:
	case ADDR_IABSOLUTEX:
		i.op1 = bus_read_1(b, addr+1);
		i.op2 = bus_read_1(b, addr+2);
		break;
	case ADDR_IMPLIED:
	default:
		break;
	}

	return i;
}

void
instruction_print(instruction_t *i)
{
	switch (i->def.mode) {
	case ADDR_IMPLIED:
		printf("%s", i->def.mnemonic);
		break;
	case ADDR_IMMEDIATE:
		printf("%s #%X", i->def.mnemonic, i->op1);
		break;
	case ADDR_ZP:
		printf("%s %X", i->def.mnemonic, i->op1);
		break;

	case ADDR_ABSOLUTE:
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
