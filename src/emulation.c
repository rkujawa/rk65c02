#include <stdio.h>

#include "emulation.h"

void
emul_lda(rk65c02emu_t *e, instruction_t *i)
{
	instrdef_t id;
	id = instruction_decode(i->opcode);

	e->regs.A = instruction_data_read_1(e, &id, i);


	/* adjust status flags */

 
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

