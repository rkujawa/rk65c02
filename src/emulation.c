#include <stdio.h>

#include "emulation.h"

void
emul_lda(rk65c02emu_t *e, instruction_t *i)
{
	instrdef_t id;
	id = instruction_decode(i->opcode);

	e->regs.A = instruction_data_read_1(e, &id, i);

	/* adjust status flags */
	if (e->regs.A & NEGATIVE)
		e->regs.P |= P_NEGATIVE;
	else
		e->regs.P &= ~P_NEGATIVE;

	if (e->regs.A == 0)
		e->regs.P |= P_ZERO;
	else
		e->regs.P &= ~P_ZERO;
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

