#include <stdio.h>

#include "emulation.h"

/* AND - logical AND */
void
emul_and(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint8_t rv;

	rv = e->regs.A & (instruction_data_read_1(e, (instrdef_t *) id, i));
	e->regs.A = rv;

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

/* CLC - clear carry flag */
void
emul_clc(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.P &= ~P_CARRY;
}

/* DNX - decrement X */
void
emul_dex(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.X--;

	instruction_status_adjust_zero(e, e->regs.X);
	instruction_status_adjust_negative(e, e->regs.X);
}

/* DNY - decrement Y */
void
emul_dey(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.Y--;

	instruction_status_adjust_zero(e, e->regs.Y);
	instruction_status_adjust_negative(e, e->regs.Y);
}

/* INX - increment X */
void
emul_inx(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.X++;

	instruction_status_adjust_zero(e, e->regs.X);
	instruction_status_adjust_negative(e, e->regs.X);
}

/* INY - increment Y */
void
emul_iny(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.Y++;

	instruction_status_adjust_zero(e, e->regs.Y);
	instruction_status_adjust_negative(e, e->regs.Y);
}

/* LDA - load to accumulator */
void
emul_lda(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.A = instruction_data_read_1(e, (instrdef_t *) id, i);

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

/* LDX - load to X */
void
emul_ldx(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.X = instruction_data_read_1(e, (instrdef_t *) id, i);

	instruction_status_adjust_zero(e, e->regs.X);
	instruction_status_adjust_negative(e, e->regs.X);
}

/* LDY - load to Y */
void
emul_ldy(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.Y = instruction_data_read_1(e, (instrdef_t *) id, i);

	instruction_status_adjust_zero(e, e->regs.Y);
	instruction_status_adjust_negative(e, e->regs.Y);
}

/* NOP - do nothing */
void
emul_nop(rk65c02emu_t *e, void *id, instruction_t *i)
{
}

/* PHA - push accumulator to stack */
void
emul_pha(rk65c02emu_t *e, void *id, instruction_t *i)
{
	stack_push(e, e->regs.A);
}

/* PHP - push processor flags to stack */
void
emul_php(rk65c02emu_t *e, void *id, instruction_t *i)
{
	stack_push(e, e->regs.P);
}

/* PLA - pull from stack to accumulator */
void
emul_pla(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.A = stack_pop(e);

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

/* PLA - pull from stack to processor flags */
void
emul_plp(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.P = stack_pop(e) | P_UNDEFINED;
}

/* SEC - set the carry flag */
void
emul_sec(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.P |= P_CARRY;
}

/* STP - stop the processor */
void
emul_stp(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->state = STOPPED;
}

/* STZ - store zero */
void
emul_stz(rk65c02emu_t *e, void *id, instruction_t *i)
{
	instruction_data_write_1(e, id, i, 0);
}

/* TAX - transfer accumulator to X */
void
emul_tax(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.X = e->regs.A;

	instruction_status_adjust_zero(e, e->regs.X);
	instruction_status_adjust_negative(e, e->regs.X);
}

/* TAY - transfer accumulator to Y */
void
emul_tay(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.Y = e->regs.A;

	instruction_status_adjust_zero(e, e->regs.Y);
	instruction_status_adjust_negative(e, e->regs.Y);
}

/* TSX - transfer stack pointer to X */
void
emul_tsx(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.X = e->regs.SP;

	instruction_status_adjust_zero(e, e->regs.X);
	instruction_status_adjust_negative(e, e->regs.X);
}

/* TXA - transfer X to accumulator */
void
emul_txa(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.A = e->regs.X;

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

/* TXS - transfer X to stack pointer */
void
emul_txs(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.SP = e->regs.X;
}

/* TYA - transfer Y to accumulator */
void
emul_tya(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.A = e->regs.Y;

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

