#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <assert.h>
#include <string.h>

#include "bus.h"
#include "instruction.h"
#include "rk65c02.h"

void rk65c02_exec(rk65c02emu_t *);

/*
 * Prepare the emulator for use, set initial CPU state.
 */
rk65c02emu_t
rk65c02_init(bus_t *b)
{
	rk65c02emu_t e;

	e.bus = b;
	e.state = STOPPED;
	e.regs.P = P_UNDEFINED|P_IRQ_DISABLE;

	return e;
}

/*
 * Execute a single instruction within emulator.
 */
void
rk65c02_exec(rk65c02emu_t *e)
{
	instruction_t i;
	instrdef_t id;

	/* XXX: handle breakpoints and watch points */

	/* if disassembly-when-running enabled */
	disassemble(e->bus, e->regs.PC);

	i = instruction_fetch(e->bus, e->regs.PC);
	id = instruction_decode(i.opcode);

	if (id.emul != NULL) {
		id.emul(e, &id, &i);
		if (!instruction_modify_pc(&id)) 
			program_counter_increment(e, &id);
	} else {
		printf("unimplemented opcode %X @ %X\n", i.opcode,
		    e->regs.PC);
		e->state = STOPPED;
		e->stopreason = EMUERROR;
	}
}

/*
 * Start the emulator.
 */
void
rk65c02_start(rk65c02emu_t *e) {

	e->state = RUNNING;
	while (e->state == RUNNING) {
		rk65c02_exec(e);
	}
}

/*
 * Execute as many instructions as specified in steps argument.
 */
void
rk65c02_step(rk65c02emu_t *e, uint16_t steps) {

	uint16_t i = 0;

	e->state = STEPPING;
	while ((e->state == STEPPING) && (i < steps)) {
		rk65c02_exec(e);
		i++;
	}
}

void
rk65c02_dump_regs(rk65c02emu_t *e)
{
	printf("A: %X X: %X Y: %X SP: %X P: ", e->regs.A, e->regs.X, e->regs.Y, e->regs.SP);

	if (e->regs.P & P_NEGATIVE)
		printf("N");
	else
		printf("-");

	if (e->regs.P & P_SIGN_OVERFLOW)
		printf("O");
	else
		printf("-");

	if (e->regs.P & P_UNDEFINED)
		printf("1");
	else
		printf("-");

	if (e->regs.P & P_BREAK)
		printf("B");
	else
		printf("-");

	if (e->regs.P & P_DECIMAL)
		printf("D");
	else
		printf("-");

	if (e->regs.P & P_IRQ_DISABLE)
		printf("I");
	else
		printf("-");

	if (e->regs.P & P_ZERO)
		printf("Z");
	else
		printf("-");

	if (e->regs.P & P_CARRY)
		printf("C");
	else
		printf("-");

	printf("\n");
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
