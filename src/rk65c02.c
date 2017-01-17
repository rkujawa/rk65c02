#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <assert.h>
#include <string.h>

#include "bus.h"
#include "instruction.h"
#include "rk65c02.h"

static bool run = false;

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

		if (i.def.opcode == 0xDB) // STP
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
