/*
 *      SPDX-License-Identifier: GPL-3.0-only
 *
 *      rk65c02 - GNU lightning based JIT backend
 *
 *      Compiles basic blocks of 65C02 instructions into native code.
 *      See doc/jit-design.md for architecture and opcode matrix.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

#ifdef HAVE_LIGHTNING
#include <lightning/lightning.h>
static jit_state_t *_jit;
#endif

#include <gc/gc.h>

#include "rk65c02.h"
#include "jit.h"
#include "instruction.h"
#include "bus.h"
#include "log.h"

/* Opcode bytes for JIT-native instructions (avoid pulling in 65c02isa.h). */
#define JIT_OP_CLC      0x18
#define JIT_OP_SEC      0x38
#define JIT_OP_CLI      0x58
#define JIT_OP_SEI      0x78
#define JIT_OP_DEY      0x88
#define JIT_OP_TXA      0x8A
#define JIT_OP_TYA      0x98
#define JIT_OP_TXS      0x9A
#define JIT_OP_LDY_IMM  0xA0
#define JIT_OP_LDX_IMM  0xA2
#define JIT_OP_TAY      0xA8
#define JIT_OP_LDA_IMM  0xA9
#define JIT_OP_TAX      0xAA
#define JIT_OP_CLV      0xB8
#define JIT_OP_TSX      0xBA
#define JIT_OP_INY      0xC8
#define JIT_OP_DEX      0xCA
#define JIT_OP_NOP      0xEA
#define JIT_OP_INX      0xE8

#define JIT_CACHE_SIZE 65536
#define JIT_BLOCK_MAX_INSNS 64
#define JIT_MAGIC 0x4a495431u  /* "JIT1" */

/* Offsets into emulator state for generated code (no magic numbers). */
#define OFFSET_E_STATE  offsetof(struct rk65c02emu, state)
#define OFFSET_E_REGS   offsetof(struct rk65c02emu, regs)
#define OFFSET_E_BUS    offsetof(struct rk65c02emu, bus)
#define OFFSET_REGS_A   offsetof(struct reg_state, A)
#define OFFSET_REGS_X   offsetof(struct reg_state, X)
#define OFFSET_REGS_Y   offsetof(struct reg_state, Y)
#define OFFSET_REGS_PC  offsetof(struct reg_state, PC)
#define OFFSET_REGS_SP  offsetof(struct reg_state, SP)
#define OFFSET_REGS_P   offsetof(struct reg_state, P)
#define OFFSET_E_PC     (OFFSET_E_REGS + OFFSET_REGS_PC)
#define OFFSET_E_P      (OFFSET_E_REGS + OFFSET_REGS_P)
#define OFFSET_E_A      (OFFSET_E_REGS + OFFSET_REGS_A)
#define OFFSET_E_X      (OFFSET_E_REGS + OFFSET_REGS_X)
#define OFFSET_E_Y      (OFFSET_E_REGS + OFFSET_REGS_Y)
#define OFFSET_E_SP     (OFFSET_E_REGS + OFFSET_REGS_SP)

struct rk65c02_jit_block {
	uint16_t start_pc;
	void (*fn)(rk65c02emu_t *);
#ifdef HAVE_LIGHTNING
	jit_state_t *lightning_state;  /* one state per block: stable fn, no buffer realloc */
#endif
};

struct rk65c02_jit {
	unsigned int magic;
	struct rk65c02_jit_block *blocks[JIT_CACHE_SIZE];
};

/* One decoded instruction in a block. */
struct jit_block_insn {
	instruction_t i;
	instrdef_t id;
};

/* Forward declaration from rk65c02.c */
void rk65c02_exec(rk65c02emu_t *e);

/*
 * Build a block of instructions starting at start_pc. Stops at the first
 * instruction that modifies PC, at max_insns, or on PC wrap. Reuses
 * instruction_fetch and instruction_decode for consistency with the interpreter.
 */
static void
jit_build_block_insns(bus_t *bus, uint16_t start_pc,
    struct jit_block_insn *insns, size_t max_insns, size_t *num_insns)
{
	uint16_t pc;
	size_t n;
	instrdef_t id;

	pc = start_pc;
	n = 0;

	while (n < max_insns) {
		insns[n].i = instruction_fetch(bus, pc);
		id = instruction_decode(insns[n].i.opcode);
		insns[n].id = id;

		n++;

		/*
		 * End the block at any instruction that modifies PC.
		 * RTS has modify_pc=false in the ISA because the interpreter
		 * relies on program_counter_increment to add 1 to the popped
		 * return address. However, RTS still changes PC to a completely
		 * different location, so it must end the block.
		 */
		if (instruction_modify_pc(&id) ||
		    insns[n-1].i.opcode == 0x60 /* RTS */)
			break;

		pc += id.size;
	}

	*num_insns = n;
}

static bool jit_initialized;

static struct rk65c02_jit *
jit_backend_create(void)
{
	struct rk65c02_jit *j;

#ifdef HAVE_LIGHTNING
	if (!jit_initialized) {
		init_jit(NULL);
		jit_initialized = true;
	}
#endif

	j = (struct rk65c02_jit *)GC_MALLOC(sizeof(struct rk65c02_jit));
	assert(j != NULL);
	j->magic = JIT_MAGIC;
	memset(j->blocks, 0, sizeof(j->blocks));

	return j;
}

static void
jit_backend_flush(struct rk65c02_jit *j)
{
	if (j == NULL)
		return;

	memset(j->blocks, 0, sizeof(j->blocks));
}

static struct rk65c02_jit_block *
jit_find_block(struct rk65c02_jit *j, uint16_t pc)
{
	if (j == NULL || j->magic != JIT_MAGIC)
		return NULL;

	return j->blocks[pc];
}

#ifdef HAVE_LIGHTNING
/*
 * Emit code to advance e->regs.PC by size bytes. JIT_R0 must hold e.
 */
static void
jit_emit_advance_pc(unsigned int size)
{
	jit_ldxi_us(JIT_R1, JIT_R0, OFFSET_E_PC);
	jit_addi(JIT_R1, JIT_R1, size);
	jit_stxi_s(OFFSET_E_PC, JIT_R0, JIT_R1);
}

/*
 * Update Z and N flags in P from 8-bit result in JIT_R1. JIT_R0 = e.
 * Uses JIT_R2 as scratch.
 */
static void
jit_emit_update_zn(void)
{
	jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
	jit_andi(JIT_R2, JIT_R2, ~(P_ZERO | P_NEGATIVE));

	jit_node_t *not_zero = jit_bnei(JIT_R1, 0);
	jit_ori(JIT_R2, JIT_R2, P_ZERO);
	jit_patch(not_zero);

	jit_node_t *not_neg = jit_bmci(JIT_R1, 0x80);
	jit_ori(JIT_R2, JIT_R2, P_NEGATIVE);
	jit_patch(not_neg);

	jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R2);
}

/*
 * Emit bus_read_1(e->bus, addr) where addr is in JIT_R1.
 * JIT_R0 must hold e. After call, JIT_R1 holds the byte read.
 * JIT_R0 is reloaded from arg_node after the call.
 */
static void
jit_emit_bus_read(jit_node_t *arg_node)
{
	jit_ldxi(JIT_R2, JIT_R0, OFFSET_E_BUS);
	jit_prepare();
	jit_pushargr(JIT_R2);
	jit_pushargr(JIT_R1);
	jit_finishi((void *)bus_read_1);
	jit_retval(JIT_R1);
	jit_andi(JIT_R1, JIT_R1, 0xFF);
	jit_movr(JIT_R0, JIT_V0);
}

/*
 * Emit bus_write_1(e->bus, addr, val) where addr is in JIT_R1, val in JIT_R2.
 * JIT_R0 must hold e. JIT_R0 is reloaded from arg_node after the call.
 */
static void
jit_emit_bus_write(jit_node_t *arg_node)
{
	jit_ldxi(JIT_V1, JIT_R0, OFFSET_E_BUS);
	jit_prepare();
	jit_pushargr(JIT_V1);
	jit_pushargr(JIT_R1);
	jit_pushargr(JIT_R2);
	jit_finishi((void *)bus_write_1);
	jit_movr(JIT_R0, JIT_V0);
}

/*
 * Compute effective address into JIT_R1 based on addressing mode and operands.
 * JIT_R0 must hold e. For modes requiring bus reads (IZP, IZPX, IZPY),
 * this calls bus_read_1 and reloads JIT_R0 from arg_node.
 * Returns true if the mode is handled, false for modes that should use fallback.
 */
static bool
jit_emit_effaddr(struct jit_block_insn *bi, jit_node_t *arg_node)
{
	addressing_t mode = bi->id.mode;
	uint8_t op1 = bi->i.op1;
	uint8_t op2 = bi->i.op2;

	switch (mode) {
	case IMMEDIATE:
		jit_movi(JIT_R1, op1);
		return true;
	case ZP:
		jit_movi(JIT_R1, op1);
		return true;
	case ZPX:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_X);
		jit_addi(JIT_R1, JIT_R1, op1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		return true;
	case ZPY:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_Y);
		jit_addi(JIT_R1, JIT_R1, op1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		return true;
	case ABSOLUTE:
		jit_movi(JIT_R1, op1 | (op2 << 8));
		return true;
	case ABSOLUTEX:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_X);
		jit_addi(JIT_R1, JIT_R1, op1 | (op2 << 8));
		return true;
	case ABSOLUTEY:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_Y);
		jit_addi(JIT_R1, JIT_R1, op1 | (op2 << 8));
		return true;
	case IZP:
		/* addr = bus_read(op1) | (bus_read(op1+1) << 8) */
		jit_movi(JIT_R1, op1);
		jit_emit_bus_read(arg_node);
		jit_movr(JIT_V1, JIT_R1);
		jit_movi(JIT_R1, (uint8_t)(op1 + 1));
		jit_emit_bus_read(arg_node);
		jit_lshi(JIT_R1, JIT_R1, 8);
		jit_orr(JIT_R1, JIT_R1, JIT_V1);
		return true;
	case IZPX:
		/* addr = bus_read((op1+X)&0xFF) | (bus_read((op1+X+1)&0xFF) << 8) */
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_X);
		jit_addi(JIT_R2, JIT_R2, op1);
		jit_andi(JIT_R2, JIT_R2, 0xFF);
		jit_movr(JIT_R1, JIT_R2);
		jit_emit_bus_read(arg_node);
		jit_movr(JIT_V1, JIT_R1);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_X);
		jit_addi(JIT_R2, JIT_R2, op1 + 1);
		jit_andi(JIT_R1, JIT_R2, 0xFF);
		jit_emit_bus_read(arg_node);
		jit_lshi(JIT_R1, JIT_R1, 8);
		jit_orr(JIT_R1, JIT_R1, JIT_V1);
		return true;
	case IZPY:
		/* addr = (bus_read(op1) | (bus_read(op1+1) << 8)) + Y */
		jit_movi(JIT_R1, op1);
		jit_emit_bus_read(arg_node);
		jit_movr(JIT_V1, JIT_R1);
		jit_movi(JIT_R1, (uint8_t)(op1 + 1));
		jit_emit_bus_read(arg_node);
		jit_lshi(JIT_R1, JIT_R1, 8);
		jit_orr(JIT_R1, JIT_R1, JIT_V1);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_Y);
		jit_addr(JIT_R1, JIT_R1, JIT_R2);
		return true;
	case ACCUMULATOR:
		return true;
	default:
		return false;
	}
}

/*
 * Load operand into JIT_R1. For IMMEDIATE mode, the value is op1 directly.
 * For memory modes, computes the effective address and reads from the bus.
 * Returns false if the addressing mode is not handled.
 */
static bool
jit_emit_load_operand(struct jit_block_insn *bi, jit_node_t *arg_node)
{
	if (!jit_emit_effaddr(bi, arg_node))
		return false;
	if (bi->id.mode != IMMEDIATE)
		jit_emit_bus_read(arg_node);
	return true;
}

/*
 * Emit native code for one instruction or fall back to rk65c02_exec.
 * JIT_R0 must hold e; it may be clobbered by C calls so we reload from arg.
 * Returns a forward-branch node that needs patching to the block's return
 * point (for fallback bail-out on state change), or NULL for native insns.
 */
static jit_node_t *
jit_emit_insn(struct jit_block_insn *bi, jit_node_t *arg_node)
{
	jit_node_t *bail;
	uint8_t op = bi->i.opcode;
	uint8_t size = bi->id.size;

	switch (op) {

	/* --- NOP --- */
	case JIT_OP_NOP:
		jit_emit_advance_pc(size);
		return NULL;

	/* --- Flag modifiers --- */
	case JIT_OP_CLC:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R1, JIT_R1, ~P_CARRY);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;
	case JIT_OP_SEC:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_P);
		jit_ori(JIT_R1, JIT_R1, P_CARRY);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;
	case JIT_OP_CLI:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R1, JIT_R1, ~P_IRQ_DISABLE);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;
	case JIT_OP_SEI:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_P);
		jit_ori(JIT_R1, JIT_R1, P_IRQ_DISABLE);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;
	case JIT_OP_CLV:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R1, JIT_R1, ~P_SIGN_OVERFLOW);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;

	/* --- Transfer --- */
	case JIT_OP_TAX:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_A);
		jit_stxi_c(OFFSET_E_X, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case JIT_OP_TAY:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_A);
		jit_stxi_c(OFFSET_E_Y, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case JIT_OP_TXA:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_X);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case JIT_OP_TYA:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_Y);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case JIT_OP_TSX:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_SP);
		jit_stxi_c(OFFSET_E_X, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case JIT_OP_TXS:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_X);
		jit_stxi_c(OFFSET_E_SP, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;

	/* --- INX/DEX/INY/DEY --- */
	case JIT_OP_INX:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_X);
		jit_addi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_X, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case JIT_OP_DEX:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_X);
		jit_subi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_X, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case JIT_OP_INY:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_Y);
		jit_addi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_Y, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case JIT_OP_DEY:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_Y);
		jit_subi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_Y, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;

	/* --- INC/DEC accumulator --- */
	case 0x1A: /* INC A */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_A);
		jit_addi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case 0x3A: /* DEC A */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_A);
		jit_subi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;

	/* --- LDA (all modes) --- */
	case 0xA9: case 0xA5: case 0xB5: case 0xAD: case 0xBD: case 0xB9:
	case 0xA1: case 0xB1: case 0xB2:
		if (!jit_emit_load_operand(bi, arg_node))
			break;
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;

	/* --- LDX (all modes) --- */
	case 0xA2: case 0xA6: case 0xB6: case 0xAE: case 0xBE:
		if (!jit_emit_load_operand(bi, arg_node))
			break;
		jit_stxi_c(OFFSET_E_X, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;

	/* --- LDY (all modes) --- */
	case 0xA0: case 0xA4: case 0xB4: case 0xAC: case 0xBC:
		if (!jit_emit_load_operand(bi, arg_node))
			break;
		jit_stxi_c(OFFSET_E_Y, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;

	/* --- STA (all modes) --- */
	case 0x85: case 0x95: case 0x8D: case 0x9D: case 0x99:
	case 0x81: case 0x91: case 0x92:
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_A);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return NULL;

	/* --- STX (all modes) --- */
	case 0x86: case 0x96: case 0x8E:
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_X);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return NULL;

	/* --- STY (all modes) --- */
	case 0x84: case 0x94: case 0x8C:
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_Y);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return NULL;

	/* --- STZ (all modes) --- */
	case 0x64: case 0x74: case 0x9C: case 0x9E:
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_movi(JIT_R2, 0);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return NULL;

	/* --- AND (all modes) --- */
	case 0x29: case 0x25: case 0x35: case 0x2D: case 0x3D: case 0x39:
	case 0x21: case 0x31: case 0x32:
		if (!jit_emit_load_operand(bi, arg_node))
			break;
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_A);
		jit_andr(JIT_R1, JIT_R2, JIT_R1);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;

	/* --- ORA (all modes) --- */
	case 0x09: case 0x05: case 0x15: case 0x0D: case 0x1D: case 0x19:
	case 0x01: case 0x11: case 0x12:
		if (!jit_emit_load_operand(bi, arg_node))
			break;
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_A);
		jit_orr(JIT_R1, JIT_R2, JIT_R1);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;

	/* --- EOR (all modes) --- */
	case 0x49: case 0x45: case 0x55: case 0x4D: case 0x5D: case 0x59:
	case 0x41: case 0x51: case 0x52:
		if (!jit_emit_load_operand(bi, arg_node))
			break;
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_A);
		jit_xorr(JIT_R1, JIT_R2, JIT_R1);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;

	/* --- CMP (all modes): A - M, update C/Z/N --- */
	case 0xC9: case 0xC5: case 0xD5: case 0xCD: case 0xDD: case 0xD9:
	case 0xC1: case 0xD1: case 0xD2: {
		jit_node_t *no_carry;
		if (!jit_emit_load_operand(bi, arg_node))
			break;
		/* R1 = operand, load A into R2 */
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_A);
		/* Update carry: set if A >= M */
		jit_ldxi_uc(JIT_V1, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_V1, JIT_V1, ~(P_CARRY | P_ZERO | P_NEGATIVE));
		no_carry = jit_bltr_u(JIT_R2, JIT_R1);
		jit_ori(JIT_V1, JIT_V1, P_CARRY);
		jit_patch(no_carry);
		/* R1 = (A - M) & 0xFF for Z/N */
		jit_subr(JIT_R1, JIT_R2, JIT_R1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		/* Z flag */
		no_carry = jit_bnei(JIT_R1, 0);
		jit_ori(JIT_V1, JIT_V1, P_ZERO);
		jit_patch(no_carry);
		/* N flag */
		no_carry = jit_bmci(JIT_R1, 0x80);
		jit_ori(JIT_V1, JIT_V1, P_NEGATIVE);
		jit_patch(no_carry);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_V1);
		jit_emit_advance_pc(size);
		return NULL;
	}

	/* --- CPX (all modes) --- */
	case 0xE0: case 0xE4: case 0xEC: {
		jit_node_t *no_carry;
		if (!jit_emit_load_operand(bi, arg_node))
			break;
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_X);
		jit_ldxi_uc(JIT_V1, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_V1, JIT_V1, ~(P_CARRY | P_ZERO | P_NEGATIVE));
		no_carry = jit_bltr_u(JIT_R2, JIT_R1);
		jit_ori(JIT_V1, JIT_V1, P_CARRY);
		jit_patch(no_carry);
		jit_subr(JIT_R1, JIT_R2, JIT_R1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		no_carry = jit_bnei(JIT_R1, 0);
		jit_ori(JIT_V1, JIT_V1, P_ZERO);
		jit_patch(no_carry);
		no_carry = jit_bmci(JIT_R1, 0x80);
		jit_ori(JIT_V1, JIT_V1, P_NEGATIVE);
		jit_patch(no_carry);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_V1);
		jit_emit_advance_pc(size);
		return NULL;
	}

	/* --- CPY (all modes) --- */
	case 0xC0: case 0xC4: case 0xCC: {
		jit_node_t *no_carry;
		if (!jit_emit_load_operand(bi, arg_node))
			break;
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_Y);
		jit_ldxi_uc(JIT_V1, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_V1, JIT_V1, ~(P_CARRY | P_ZERO | P_NEGATIVE));
		no_carry = jit_bltr_u(JIT_R2, JIT_R1);
		jit_ori(JIT_V1, JIT_V1, P_CARRY);
		jit_patch(no_carry);
		jit_subr(JIT_R1, JIT_R2, JIT_R1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		no_carry = jit_bnei(JIT_R1, 0);
		jit_ori(JIT_V1, JIT_V1, P_ZERO);
		jit_patch(no_carry);
		no_carry = jit_bmci(JIT_R1, 0x80);
		jit_ori(JIT_V1, JIT_V1, P_NEGATIVE);
		jit_patch(no_carry);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_V1);
		jit_emit_advance_pc(size);
		return NULL;
	}

	/* --- INC memory --- */
	case 0xE6: case 0xF6: case 0xEE: case 0xFE:
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_movr(JIT_V1, JIT_R1);
		jit_emit_bus_read(arg_node);
		jit_addi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_movr(JIT_V2, JIT_R1);
		jit_movr(JIT_R2, JIT_R1);
		jit_movr(JIT_R1, JIT_V1);
		jit_emit_bus_write(arg_node);
		jit_movr(JIT_R1, JIT_V2);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;

	/* --- DEC memory --- */
	case 0xC6: case 0xD6: case 0xCE: case 0xDE:
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_movr(JIT_V1, JIT_R1);
		jit_emit_bus_read(arg_node);
		jit_subi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_movr(JIT_V2, JIT_R1);
		jit_movr(JIT_R2, JIT_R1);
		jit_movr(JIT_R1, JIT_V1);
		jit_emit_bus_write(arg_node);
		jit_movr(JIT_R1, JIT_V2);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;

	/* --- ASL accumulator --- */
	case 0x0A: {
		jit_node_t *nc;
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_A);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R2, JIT_R2, ~P_CARRY);
		nc = jit_bmci(JIT_R1, 0x80);
		jit_ori(JIT_R2, JIT_R2, P_CARRY);
		jit_patch(nc);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R2);
		jit_lshi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	}

	/* --- LSR accumulator --- */
	case 0x4A: {
		jit_node_t *nc;
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_A);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R2, JIT_R2, ~P_CARRY);
		nc = jit_bmci(JIT_R1, 0x01);
		jit_ori(JIT_R2, JIT_R2, P_CARRY);
		jit_patch(nc);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R2);
		jit_rshi_u(JIT_R1, JIT_R1, 1);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	}

	/* --- ROL accumulator --- */
	case 0x2A: {
		jit_node_t *nc;
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_A);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_V1, JIT_R2, P_CARRY);
		jit_andi(JIT_R2, JIT_R2, ~P_CARRY);
		nc = jit_bmci(JIT_R1, 0x80);
		jit_ori(JIT_R2, JIT_R2, P_CARRY);
		jit_patch(nc);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R2);
		jit_lshi(JIT_R1, JIT_R1, 1);
		jit_orr(JIT_R1, JIT_R1, JIT_V1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	}

	/* --- ROR accumulator --- */
	case 0x6A: {
		jit_node_t *nc;
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_A);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_V1, JIT_R2, P_CARRY);
		jit_lshi(JIT_V1, JIT_V1, 7);
		jit_andi(JIT_R2, JIT_R2, ~P_CARRY);
		nc = jit_bmci(JIT_R1, 0x01);
		jit_ori(JIT_R2, JIT_R2, P_CARRY);
		jit_patch(nc);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R2);
		jit_rshi_u(JIT_R1, JIT_R1, 1);
		jit_orr(JIT_R1, JIT_R1, JIT_V1);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	}

	/* --- BIT --- */
	case 0x89: case 0x24: case 0x34: case 0x2C: case 0x3C: {
		jit_node_t *br;
		if (!jit_emit_load_operand(bi, arg_node))
			break;
		/* R1 = memory value, R2 = A */
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_A);
		jit_ldxi_uc(JIT_V1, JIT_R0, OFFSET_E_P);
		/* Clear Z; if IMMEDIATE, only Z affected */
		jit_andi(JIT_V1, JIT_V1, (op == 0x89) ?
		    (uint8_t)~P_ZERO :
		    (uint8_t)~(P_ZERO | P_NEGATIVE | P_SIGN_OVERFLOW));
		/* Z: set if (A & M) == 0 */
		jit_andr(JIT_R2, JIT_R2, JIT_R1);
		br = jit_bnei(JIT_R2, 0);
		jit_ori(JIT_V1, JIT_V1, P_ZERO);
		jit_patch(br);
		/* For non-immediate: N = M bit 7, V = M bit 6 */
		if (op != 0x89) {
			br = jit_bmci(JIT_R1, 0x80);
			jit_ori(JIT_V1, JIT_V1, P_NEGATIVE);
			jit_patch(br);
			br = jit_bmci(JIT_R1, 0x40);
			jit_ori(JIT_V1, JIT_V1, P_SIGN_OVERFLOW);
			jit_patch(br);
		}
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_V1);
		jit_emit_advance_pc(size);
		return NULL;
	}

	/* --- Stack push: PHA, PHX, PHY, PHP --- */
	case 0x48: case 0xDA: case 0x5A: case 0x08: {
		int reg_off;
		if (op == 0x48)      reg_off = OFFSET_E_A;
		else if (op == 0xDA) reg_off = OFFSET_E_X;
		else if (op == 0x5A) reg_off = OFFSET_E_Y;
		else                 reg_off = OFFSET_E_P;
		/* addr = 0x100 + SP */
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_SP);
		jit_addi(JIT_R1, JIT_R2, 0x100);
		jit_ldxi_uc(JIT_R2, JIT_R0, reg_off);
		if (op == 0x08)
			jit_ori(JIT_R2, JIT_R2, P_BREAK | P_UNDEFINED);
		jit_emit_bus_write(arg_node);
		/* SP-- */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_SP);
		jit_subi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_SP, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;
	}

	/* --- Stack pull: PLA, PLX, PLY --- */
	case 0x68: case 0xFA: case 0x7A: {
		int reg_off;
		if (op == 0x68)      reg_off = OFFSET_E_A;
		else if (op == 0xFA) reg_off = OFFSET_E_X;
		else                 reg_off = OFFSET_E_Y;
		/* SP++ */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_SP);
		jit_addi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_SP, JIT_R0, JIT_R1);
		/* addr = 0x100 + SP */
		jit_addi(JIT_R1, JIT_R1, 0x100);
		jit_emit_bus_read(arg_node);
		jit_stxi_c(reg_off, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	}

	/* --- PLP (pull processor status) --- */
	case 0x28:
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_SP);
		jit_addi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_SP, JIT_R0, JIT_R1);
		jit_addi(JIT_R1, JIT_R1, 0x100);
		jit_emit_bus_read(arg_node);
		jit_ori(JIT_R1, JIT_R1, P_UNDEFINED);
		jit_andi(JIT_R1, JIT_R1, ~P_BREAK);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;

	/* --- Branches --- */
	case 0x80: /* BRA */
	case 0x90: /* BCC */ case 0xB0: /* BCS */
	case 0xF0: /* BEQ */ case 0xD0: /* BNE */
	case 0x30: /* BMI */ case 0x10: /* BPL */
	case 0x70: /* BVS */ case 0x50: /* BVC */ {
		int8_t offset = (int8_t)bi->i.op1;
		uint16_t target = (uint16_t)((int)bi->i.op1 + (int)bi->i.op2 * 0
		    + 0); /* placeholder, recalculate below */
		jit_node_t *skip;
		(void)target;

		/* For BRA, always branch. For others, check condition. */
		if (op != 0x80) {
			jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_P);
			switch (op) {
			case 0x90: /* BCC: branch if C clear */
				skip = jit_bmci(JIT_R1, P_CARRY);
				break;
			case 0xB0: /* BCS: branch if C set */
				skip = jit_bmsi(JIT_R1, P_CARRY);
				break;
			case 0xF0: /* BEQ: branch if Z set */
				skip = jit_bmsi(JIT_R1, P_ZERO);
				break;
			case 0xD0: /* BNE: branch if Z clear */
				skip = jit_bmci(JIT_R1, P_ZERO);
				break;
			case 0x30: /* BMI: branch if N set */
				skip = jit_bmsi(JIT_R1, P_NEGATIVE);
				break;
			case 0x10: /* BPL: branch if N clear */
				skip = jit_bmci(JIT_R1, P_NEGATIVE);
				break;
			case 0x70: /* BVS: branch if V set */
				skip = jit_bmsi(JIT_R1, P_SIGN_OVERFLOW);
				break;
			case 0x50: /* BVC: branch if V clear */
				skip = jit_bmci(JIT_R1, P_SIGN_OVERFLOW);
				break;
			default:
				skip = NULL;
				break;
			}
			/* Not taken: PC += size */
			jit_emit_advance_pc(size);
			jit_node_t *done = jit_jmpi();
			/* Taken: */
			jit_patch(skip);
			jit_ldxi_us(JIT_R1, JIT_R0, OFFSET_E_PC);
			jit_addi(JIT_R1, JIT_R1, (int)size + (int)offset);
			jit_andi(JIT_R1, JIT_R1, 0xFFFF);
			jit_stxi_s(OFFSET_E_PC, JIT_R0, JIT_R1);
			jit_patch(done);
		} else {
			/* BRA: always taken */
			jit_ldxi_us(JIT_R1, JIT_R0, OFFSET_E_PC);
			jit_addi(JIT_R1, JIT_R1, (int)size + (int)offset);
			jit_andi(JIT_R1, JIT_R1, 0xFFFF);
			jit_stxi_s(OFFSET_E_PC, JIT_R0, JIT_R1);
		}
		return NULL;
	}

	/* --- JMP absolute --- */
	case 0x4C: {
		uint16_t addr = bi->i.op1 | (bi->i.op2 << 8);
		jit_movi(JIT_R1, addr);
		jit_stxi_s(OFFSET_E_PC, JIT_R0, JIT_R1);
		return NULL;
	}

	default:
		break;
	}

	/* Fallback: run interpreter for one instruction. */
	jit_movr(JIT_R0, JIT_V0);
	jit_prepare();
	jit_pushargr(JIT_R0);
	jit_finishi((void *)rk65c02_exec);
	jit_movr(JIT_R0, JIT_V0);

	/* Bail out if the emulator is no longer RUNNING (e.g. STP, WAI, BRK). */
	jit_ldxi_i(JIT_R1, JIT_R0, OFFSET_E_STATE);
	bail = jit_bnei(JIT_R1, RUNNING);

	return bail;
}
#endif

static struct rk65c02_jit_block *
jit_compile_block(struct rk65c02_jit *j, uint16_t pc, bus_t *bus)
{
	struct rk65c02_jit_block *b;
	struct jit_block_insn insns[JIT_BLOCK_MAX_INSNS];
	size_t num_insns;
	size_t k;

	if (j == NULL || j->magic != JIT_MAGIC || bus == NULL)
		return NULL;

	b = (struct rk65c02_jit_block *)GC_MALLOC(sizeof(struct rk65c02_jit_block));
	assert(b != NULL);

	b->start_pc = pc;
	b->fn = NULL;

#ifdef HAVE_LIGHTNING
	{
		jit_node_t *arg_node;
		jit_node_t *bail_nodes[JIT_BLOCK_MAX_INSNS];
		jit_state_t *state;

		jit_build_block_insns(bus, pc, insns, JIT_BLOCK_MAX_INSNS,
		    &num_insns);

		state = jit_new_state();
		b->lightning_state = state;
		_jit = state;

		jit_prolog();
		arg_node = jit_arg();
		jit_getarg(JIT_V0, arg_node);
		jit_movr(JIT_R0, JIT_V0);

		for (k = 0; k < num_insns; k++)
			bail_nodes[k] = jit_emit_insn(&insns[k], arg_node);

		/* Patch all bail-out branches to the ret below. */
		for (k = 0; k < num_insns; k++) {
			if (bail_nodes[k] != NULL)
				jit_patch(bail_nodes[k]);
		}

		jit_ret();
		jit_epilog();

		b->fn = (void (*)(rk65c02emu_t *))jit_emit();
	}
#else
	(void)bus;
	(void)insns;
	(void)num_insns;
	(void)k;
#endif

	j->blocks[pc] = b;

	return b;
}

void
rk65c02_jit_enable(rk65c02emu_t *e, bool enable)
{
	assert(e != NULL);

#ifdef HAVE_LIGHTNING
	if (enable) {
		if (e->jit == NULL)
			e->jit = jit_backend_create();
		e->use_jit = true;
	} else {
		e->use_jit = false;
	}
#else
	(void)enable;
	e->use_jit = false;
#endif
}

void
rk65c02_jit_flush(rk65c02emu_t *e)
{
	assert(e != NULL);

	if (e->jit == NULL)
		return;

	jit_backend_flush(e->jit);
}

void
rk65c02_run_jit(rk65c02emu_t *e)
{
	assert(e != NULL);

	/*
	 * If JIT is not available or not enabled, fall back to the
	 * regular interpreter loop.
	 */
	if (!(e->use_jit) || (e->jit == NULL)
	    || e->trace || e->runtime_disassembly
	    || (e->bps_head != NULL)) {
		e->state = RUNNING;
		while (e->state == RUNNING)
			rk65c02_exec(e);
		return;
	}

	e->state = RUNNING;

	while (e->state == RUNNING) {
		struct rk65c02_jit_block *b;
		uint16_t pc;

		/*
		 * Honour any runtime changes in debugging state by
		 * bailing out to the interpreter if needed.
		 */
		if (e->trace || e->runtime_disassembly
		    || (e->bps_head != NULL))
			break;

		pc = e->regs.PC;
		b = jit_find_block(e->jit, pc);
		if (b == NULL)
			b = jit_compile_block(e->jit, pc, e->bus);

		if ((b == NULL) || (b->fn == NULL)) {
			rk65c02_exec(e);
			continue;
		}

		b->fn(e);
	}
}

