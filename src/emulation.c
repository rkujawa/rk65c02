#include <stdio.h>

#include "emulation.h"

void
emul_and(rk65c02emu_t *e, instruction_t *i)
{
	instrdef_t id;
	id = instruction_decode(i->opcode);
	uint8_t rv;

	rv = e->regs.A & (instruction_data_read_1(e, &id, i));
	e->regs.A = rv;

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

void
emul_lda(rk65c02emu_t *e, instruction_t *i)
{
	instrdef_t id;
	id = instruction_decode(i->opcode);

	e->regs.A = instruction_data_read_1(e, &id, i);

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

void
emul_nop(rk65c02emu_t *e, instruction_t *i)
{
	/* printf("nop!\n"); */
}

void
emul_stp(rk65c02emu_t *e, instruction_t *i)
{
	e->state = STOPPED;
}

