#include <stdio.h>
#include <assert.h>

#include "emulation.h"

/* RMB, SMB, BBR, BBS are handled by these */
static void emul_rmb(rk65c02emu_t *, void *, instruction_t *, uint8_t);
static void emul_smb(rk65c02emu_t *, void *, instruction_t *, uint8_t);
static void emul_bbr(rk65c02emu_t *, void *, instruction_t *, uint8_t);
static void emul_bbs(rk65c02emu_t *, void *, instruction_t *, uint8_t);

/* Implementation of emulation of instructions follows below */

/* ADC - add with carry */
void
emul_adc(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint8_t arg;
	uint16_t res;	/* meh */

	arg = instruction_data_read_1(e, (instrdef_t *) id, i);
	res = e->regs.A + arg;

	if (e->regs.P & P_CARRY)
		res++;

	if ((e->regs.A ^ res) & (arg ^ res) & 0x80)
		e->regs.P |= P_SIGN_OVERFLOW;
	else
		e->regs.P &= ~P_SIGN_OVERFLOW;

	/* if the result does not fit into 8 bits then set carry */
	if (res > 0xFF)
		e->regs.P |= P_CARRY;
	else
		e->regs.P &= ~P_CARRY;

	/* squash the result into accumulator's 8 bits, lol */
	e->regs.A = (uint8_t) res;

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

/* AND - logical AND */
void
emul_and(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.A &= (instruction_data_read_1(e, (instrdef_t *) id, i));

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

/* ASL - shift left one bit */
void
emul_asl(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint8_t val;

	val = instruction_data_read_1(e, (instrdef_t *) id, i);

	/* carry flag value equals contents of bit 7 */
	if (val & 0x80)
		e->regs.P |= P_CARRY;
	else
		e->regs.P &= ~P_CARRY;

	/* shift left by one bit */
	val <<= 1;

	instruction_status_adjust_zero(e, val);
	instruction_status_adjust_negative(e, val);

	instruction_data_write_1(e, (instrdef_t *) id, i, val);
}

/* BBRx - branch on bit reset (handles BBR0-7) */
static void
emul_bbr(rk65c02emu_t *e, void *id, instruction_t *i, uint8_t bit)
{
	/* is bit is clear then branch */
	if (!(BIT(i->op1, bit)))
		program_counter_branch(e, (int8_t) i->op2);
	else
		program_counter_increment(e, id);

}
void
emul_bbr0(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbr(e, id, i, 0);
}
void
emul_bbr1(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbr(e, id, i, 1);
}
void
emul_bbr2(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbr(e, id, i, 2);
}
void
emul_bbr3(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbr(e, id, i, 3);
}
void
emul_bbr4(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbr(e, id, i, 4);
}
void
emul_bbr5(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbr(e, id, i, 5);
}
void
emul_bbr6(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbr(e, id, i, 6);
}
void
emul_bbr7(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbr(e, id, i, 7);
}

/* BBSx - branch on bit set (handles BBS0-7) */
static void
emul_bbs(rk65c02emu_t *e, void *id, instruction_t *i, uint8_t bit)
{
	/* is bit is set then branch */
	if (BIT(i->op1, bit))
		program_counter_branch(e, (int8_t) i->op2);
	else
		program_counter_increment(e, id);

}
void
emul_bbs0(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbs(e, id, i, 0);
}
void
emul_bbs1(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbs(e, id, i, 1);
}
void
emul_bbs2(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbs(e, id, i, 2);
}
void
emul_bbs3(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbs(e, id, i, 3);
}
void
emul_bbs4(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbs(e, id, i, 4);
}
void
emul_bbs5(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbs(e, id, i, 5);
}
void
emul_bbs6(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbs(e, id, i, 6);
}
void
emul_bbs7(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_bbs(e, id, i, 7);
}

/* BIT - check if one or more bits are set */
void
emul_bit(rk65c02emu_t *e, void *id, instruction_t *i)
{
/*	uint8_t v = instruction_data_read_1(e, (instrdef_t *) id, i);
	printf("%x\n", v);*/

	/* zero flag set if acculumator AND memory equals zero */
	if (e->regs.A & instruction_data_read_1(e, (instrdef_t *) id, i))
		e->regs.P &= ~P_ZERO;
	else
		e->regs.P |= P_ZERO;

	if (BIT(instruction_data_read_1(e, (instrdef_t *) id, i), 6))
		e->regs.P |= P_SIGN_OVERFLOW;
	else
		e->regs.P &= ~P_SIGN_OVERFLOW;

	if (BIT(instruction_data_read_1(e, (instrdef_t *) id, i), 7))
		e->regs.P |= P_NEGATIVE;
	else
		e->regs.P &= ~P_NEGATIVE;
}

/* BCC - branch on carry clear */
void
emul_bcc(rk65c02emu_t *e, void *id, instruction_t *i)
{
	if (!(e->regs.P & P_CARRY))
		program_counter_branch(e, (int8_t) i->op1);
	else
		program_counter_increment(e, id);
}

/* BCS - branch on carry set */
void
emul_bcs(rk65c02emu_t *e, void *id, instruction_t *i)
{
	if (e->regs.P & P_CARRY)
		program_counter_branch(e, (int8_t) i->op1);
	else
		program_counter_increment(e, id);
}

/* BEQ - branch on equal */
void
emul_beq(rk65c02emu_t *e, void *id, instruction_t *i)
{
	if (e->regs.P & P_ZERO)
		program_counter_branch(e, (int8_t) i->op1);
	else
		program_counter_increment(e, id);
}

/* BMI - branch on result minus */
void
emul_bmi(rk65c02emu_t *e, void *id, instruction_t *i)
{
	if (e->regs.P & P_NEGATIVE)
		program_counter_branch(e, (int8_t) i->op1);
	else
		program_counter_increment(e, id);
}

/* BNE - branch on not equal */
void
emul_bne(rk65c02emu_t *e, void *id, instruction_t *i)
{
	if (!(e->regs.P & P_ZERO))
		program_counter_branch(e, (int8_t) i->op1);
	else
		program_counter_increment(e, id);
}

/* BPL - branch on result plus */
void
emul_bpl(rk65c02emu_t *e, void *id, instruction_t *i)
{
	if (!(e->regs.P & P_NEGATIVE))
		program_counter_branch(e, (int8_t) i->op1);
	else
		program_counter_increment(e, id);
}


/* BRA - branch always */
void
emul_bra(rk65c02emu_t *e, void *id, instruction_t *i)
{
	program_counter_branch(e, (int8_t) i->op1);
}

/* BRK - break! or rather cause an IRQ in software */
void
emul_brk(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.PC += 2;
	e->regs.P |= P_BREAK;

	rk65c02_irq(e);
}

/* BVC - branch on overflow clear */
void
emul_bvc(rk65c02emu_t *e, void *id, instruction_t *i)
{
	if (!(e->regs.P & P_SIGN_OVERFLOW))
		program_counter_branch(e, (int8_t) i->op1);
	else
		program_counter_increment(e, id);
}

/* BVS - branch on overflow set */
void
emul_bvs(rk65c02emu_t *e, void *id, instruction_t *i)
{
	if (e->regs.P & P_SIGN_OVERFLOW)
		program_counter_branch(e, (int8_t) i->op1);
	else
		program_counter_increment(e, id);
}

/* CLC - clear carry flag */
void
emul_clc(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.P &= ~P_CARRY;
}

/* CLD - clear decimal flag */
void
emul_cld(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.P &= ~P_DECIMAL;
}

/* CLI - clear interrupt disable flag */
void
emul_cli(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.P &= ~P_IRQ_DISABLE;
}

/* CLV - clear overflow flag */
void
emul_clv(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.P &= ~P_SIGN_OVERFLOW;
}

/* CMP - compare accumulator and memory location */
void
emul_cmp(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint8_t val, sr;

	val = instruction_data_read_1(e, (instrdef_t *) id, i);
	sr = e->regs.A - val;

	instruction_status_adjust_zero(e, sr);
	instruction_status_adjust_negative(e, sr);
	
	if (e->regs.A < val)
		e->regs.P |= P_CARRY;
	else
		e->regs.P &= ~P_CARRY;
}

/* CPX - compare X and memory location */
void
emul_cpx(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint8_t val, sr;

	val = instruction_data_read_1(e, (instrdef_t *) id, i);
	sr = e->regs.X - val;

	instruction_status_adjust_zero(e, sr);
	instruction_status_adjust_negative(e, sr);
	
	if (e->regs.X < val)
		e->regs.P |= P_CARRY;
	else
		e->regs.P &= ~P_CARRY;
}

/* CPY - compare Y and memory location */
void
emul_cpy(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint8_t val, sr;

	val = instruction_data_read_1(e, (instrdef_t *) id, i);
	sr = e->regs.Y - val;

	instruction_status_adjust_zero(e, sr);
	instruction_status_adjust_negative(e, sr);
	
	if (e->regs.Y < val)
		e->regs.P |= P_CARRY;
	else
		e->regs.P &= ~P_CARRY;
}

/* DEC  - decrement memory location/acumulator */
void
emul_dec(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint8_t val;

	/* this is absurdly inefficient */
	val = instruction_data_read_1(e, (instrdef_t *) id, i);
	val--;
	instruction_data_write_1(e, id, i, val);

	instruction_status_adjust_zero(e, val);
	instruction_status_adjust_negative(e, val);
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

/* EOR - logical exclusive OR */
void
emul_eor(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.A ^= instruction_data_read_1(e, (instrdef_t *) id, i);

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

/* INC - increment memory location/acumulator */
void
emul_inc(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint8_t val;

	/* this is absurdly inefficient */
	val = instruction_data_read_1(e, (instrdef_t *) id, i);
	val++;
	instruction_data_write_1(e, id, i, val);

	instruction_status_adjust_zero(e, val);
	instruction_status_adjust_negative(e, val);
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

/* JMP - JUMP~ */
void
emul_jmp(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint16_t target, iaddr;

	switch (((instrdef_t *)id)->mode) {
	case ABSOLUTE:
		target = i->op1 + (i->op2 << 8);
		break;
	case IABSOLUTE:
		iaddr = i->op1 + (i->op2 << 8);
		target = bus_read_1(e->bus, iaddr);
		target |= bus_read_1(e->bus, iaddr+1) << 8;
		break;
	case IABSOLUTEX:
		iaddr = i->op1 + (i->op2 << 8) + e->regs.X;
		target = bus_read_1(e->bus, iaddr);
		target |= bus_read_1(e->bus, iaddr + 1) << 8;
		break;
	default:
		assert(false); /* should never happen, lol */
		break;
	}

	e->regs.PC = target;
}

/* JSR - jump to subroutine */
void
emul_jsr(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint16_t jumpaddr; /* addres to jump to */
	uint16_t retaddr; /* return address */

	jumpaddr = i->op1 + (i->op2 << 8);
	retaddr = e->regs.PC + 2; /* XXX */

	/* push return address to stack */
	stack_push(e, retaddr >> 8);
	stack_push(e, retaddr & 0xFF);

	/* change program counter to point to the new location */
	e->regs.PC = jumpaddr;
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

/* LSR - shift right one bit */
void
emul_lsr(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint8_t val;

	val = instruction_data_read_1(e, (instrdef_t *) id, i);

	/* carry flag value equals contents of bit 0 */
	if (val & 0x1)
		e->regs.P |= P_CARRY;
	else
		e->regs.P &= ~P_CARRY;

	/* shift right by one bit */
	val >>= 1;

	instruction_status_adjust_zero(e, val);
	/* XXX: cannot ever be negative */
	instruction_status_adjust_negative(e, val);

	instruction_data_write_1(e, (instrdef_t *) id, i, val);
}
/* NOP - do nothing */
void
emul_nop(rk65c02emu_t *e, void *id, instruction_t *i)
{
}

/* ORA - logical inclusive OR */
void
emul_ora(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.A |= instruction_data_read_1(e, (instrdef_t *) id, i);

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
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

/* PHX - push X to stack */
void
emul_phx(rk65c02emu_t *e, void *id, instruction_t *i)
{
	stack_push(e, e->regs.X);
}

/* PHY - push Y to stack */
void
emul_phy(rk65c02emu_t *e, void *id, instruction_t *i)
{
	stack_push(e, e->regs.Y);
}

/* PLA - pull from stack to accumulator */
void
emul_pla(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.A = stack_pop(e);

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}

/* PLP - pull from stack to processor flags */
void
emul_plp(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.P = stack_pop(e) | P_UNDEFINED;
}

/* PLX - pull from stack to X */
void
emul_plx(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.X = stack_pop(e);
}

/* PLY - pull from stack to X */
void
emul_ply(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.Y = stack_pop(e);
}

/* RTS - return from subroutine */
void
emul_rts(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint16_t retaddr;

	retaddr = stack_pop(e);
	retaddr|= stack_pop(e) << 8;

	e->regs.PC = retaddr;
}

/* RMBx - reset memory bit (handles RMB0-RMB7) */
static void
emul_rmb(rk65c02emu_t *e, void *id, instruction_t *i, uint8_t bit)
{
	uint8_t val;

	val = instruction_data_read_1(e, (instrdef_t *) id, i);

	val &= ~(1 << bit);

	instruction_data_write_1(e, id, i, val);
}

void
emul_rmb0(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_rmb(e, id, i, 0);
}
void
emul_rmb1(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_rmb(e, id, i, 1);
}
void
emul_rmb2(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_rmb(e, id, i, 2);
}
void
emul_rmb3(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_rmb(e, id, i, 3);
}
void
emul_rmb4(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_rmb(e, id, i, 4);
}
void
emul_rmb5(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_rmb(e, id, i, 5);
}
void
emul_rmb6(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_rmb(e, id, i, 6);
}
void
emul_rmb7(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_rmb(e, id, i, 7);
}

/* ROL - rotate left */
void
emul_rol(rk65c02emu_t *e, void *id, instruction_t *i)
{
	bool ncarry;
	uint8_t val;

	ncarry = false;

	val = instruction_data_read_1(e, (instrdef_t *) id, i);

	/* new carry flag value equals contents of bit 7 */
	if (val & 0x80)
		ncarry = true;

	/* shift left by one bit */
	val <<= 1;

	/* bit 0 is set from current value of carry flag */
	if (e->regs.P & P_CARRY)
		val |= 0x1;
	else
		val &= ~0x1;

	if (ncarry)
		e->regs.P |= P_CARRY;
	else
		e->regs.P &= ~P_CARRY;

	instruction_status_adjust_zero(e, val);
	instruction_status_adjust_negative(e, val);

	instruction_data_write_1(e, (instrdef_t *) id, i, val);
}

/* ROR - rotate right */
void
emul_ror(rk65c02emu_t *e, void *id, instruction_t *i)
{
	bool ncarry;
	uint8_t val;

	ncarry = false;

	val = instruction_data_read_1(e, (instrdef_t *) id, i);

	/* new carry flag value equals contents of bit 0 */
	if (val & 0x1)
		ncarry = true;

	/* shift right by one bit */
	val >>= 1;

	/* bit 7 is set from current value of carry flag */
	if (e->regs.P & P_CARRY)
		val |= 0x80;
	else
		val &= ~0x80;

	if (ncarry)
		e->regs.P |= P_CARRY;
	else
		e->regs.P &= ~P_CARRY;

	instruction_status_adjust_zero(e, val);
	instruction_status_adjust_negative(e, val);

	instruction_data_write_1(e, (instrdef_t *) id, i, val);
}

/* SBC - substract with carry */
void
emul_sbc(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint8_t arg;
	uint16_t res;	/* meh */

	arg = instruction_data_read_1(e, (instrdef_t *) id, i);
	res = e->regs.A - arg;

	/* if the carry flag is NOT set then "borrow" */
	if (!(e->regs.P & P_CARRY))
		res--;

	if ((e->regs.A ^ res) & ((0xFF-arg) ^ res) & 0x80)
		e->regs.P |= P_SIGN_OVERFLOW;
	else
		e->regs.P &= ~P_SIGN_OVERFLOW;

	/* if the result does not fit into 8 bits then clear carry */
	if (res & 0x8000)
		e->regs.P &= ~P_CARRY;
	else
		e->regs.P |= P_CARRY;

	/* squash the result into accumulator's 8 bits, lol */
	e->regs.A = (uint8_t) res;

	instruction_status_adjust_zero(e, e->regs.A);
	instruction_status_adjust_negative(e, e->regs.A);
}
/* SEC - set the carry flag */
void
emul_sec(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.P |= P_CARRY;
}

/* SEI - set the interrupt disable flag */
void
emul_sei(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->regs.P |= P_IRQ_DISABLE;
}

/* SMBx - set memory bit (handles SMB0-SMB7) */
static void
emul_smb(rk65c02emu_t *e, void *id, instruction_t *i, uint8_t bit)
{
	uint8_t val;

	val = instruction_data_read_1(e, (instrdef_t *) id, i);

	val |= (1 << bit);

	instruction_data_write_1(e, id, i, val);
}
void
emul_smb0(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_smb(e, id, i, 0);
}
void
emul_smb1(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_smb(e, id, i, 1);
}
void
emul_smb2(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_smb(e, id, i, 2);
}
void
emul_smb3(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_smb(e, id, i, 3);
}
void
emul_smb4(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_smb(e, id, i, 4);
}
void
emul_smb5(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_smb(e, id, i, 5);
}
void
emul_smb6(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_smb(e, id, i, 6);
}
void
emul_smb7(rk65c02emu_t *e, void *id, instruction_t *i)
{
	emul_smb(e, id, i, 7);
}

/* STP - stop the processor */
void
emul_stp(rk65c02emu_t *e, void *id, instruction_t *i)
{
	e->state = STOPPED;
	e->stopreason = STP;
}

/* STA - store accumulator */
void
emul_sta(rk65c02emu_t *e, void *id, instruction_t *i)
{
	instruction_data_write_1(e, id, i, e->regs.A);
}

/* STX - store X */
void
emul_stx(rk65c02emu_t *e, void *id, instruction_t *i)
{
	instruction_data_write_1(e, id, i, e->regs.X);
}

/* STY - store Y */
void
emul_sty(rk65c02emu_t *e, void *id, instruction_t *i)
{
	instruction_data_write_1(e, id, i, e->regs.Y);
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

/* TRB - test and reset bits */
void
emul_trb(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint8_t val;

	val = instruction_data_read_1(e, (instrdef_t *) id, i);

	if (e->regs.A & val)
		e->regs.P &= ~P_ZERO;
	else
		e->regs.P |= P_ZERO;

	instruction_data_write_1(e, (instrdef_t *) id, i,
	    val & (e->regs.A ^ 0xFF));
}

/* TSB - test and set bits */
void
emul_tsb(rk65c02emu_t *e, void *id, instruction_t *i)
{
	uint8_t val;

	val = instruction_data_read_1(e, (instrdef_t *) id, i);

	if (e->regs.A & val)
		e->regs.P &= ~P_ZERO;
	else
		e->regs.P |= P_ZERO;

	instruction_data_write_1(e, (instrdef_t *) id, i,
	    val | e->regs.A);
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

