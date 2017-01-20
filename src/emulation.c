#include <stdio.h>

#include "emulation.h"

/* AND - logical AND */
void
emul_and(rk65c02emu_t *e, instruction_t *i)
{
	instrdef_t id;
	uint8_t rv;

	id = instruction_decode(i->opcode);

	rv = e->regs.A & (instruction_data_read_1(e, &id, i));
	e->regs.A = rv;

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

/* LDA - load to accumulator */
void
emul_lda(rk65c02emu_t *e, instruction_t *i)
{
	instrdef_t id;

	id = instruction_decode(i->opcode);

	e->regs.A = instruction_data_read_1(e, &id, i);

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

/* NOP - do nothing */
void
emul_nop(rk65c02emu_t *e, instruction_t *i)
{
	/* printf("nop!\n"); */
}

/* PHA - push accumulator to stack */
void
emul_pha(rk6502emu_t *e, instruction_t *i)
{
	stack_push(e, e->regs.A);
}

/* PLA - pull from stack to accumulator */
void
emul_pla(rk65c02emu_t *e, instruciton_t *i)
{
	e->regs.A = stack_pop(e);

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

/* STP - stop the processor */
void
emul_stp(rk65c02emu_t *e, instruction_t *i)
{
	e->state = STOPPED;
}

