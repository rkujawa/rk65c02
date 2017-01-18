#include <stdio.h>

#include "emulation.h"

void
emul_lda(rk65c02emu_t *e, instruction_t *i)
{
	instrdef_t id;

	id = instruction_decode(i->opcode);

	printf("A: %x", e->regs.A);

	e->regs.A = i->op1;

	printf("A: %x", e->regs.A);

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

