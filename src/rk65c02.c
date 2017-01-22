#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <assert.h>
#include <string.h>

#include "bus.h"
#include "instruction.h"
#include "rk65c02.h"

rk65c02emu_t
rk65c02_init(bus_t *b)
{
	rk65c02emu_t e;

	e.bus = b;
	e.state = STOPPED;

	return e;
}

void
rk65c02_start(rk65c02emu_t *e) {
	instruction_t i;
	instrdef_t id;

	e->state = RUNNING;
	while (e->state == RUNNING) {
		disassemble(e->bus, e->regs.PC);
		i = instruction_fetch(e->bus, e->regs.PC);
		id = instruction_decode(i.opcode);

		if (id.emul != NULL) {
			id.emul(e, &id, &i);
			/* if (!instruction_modify_pc) */
			program_counter_increment(e, &id);
		} else {
			printf("unimplemented opcode %X @ %X\n", i.opcode,
			    e->regs.PC);
			e->state = STOPPED;
		}
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
