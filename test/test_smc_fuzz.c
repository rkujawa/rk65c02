/*
 * Differential fuzzer: generate deterministic random 65C02 programs and
 * execute each to STP on two fresh emulators - interpreter and JIT -
 * then compare final registers, stop reason, and low memory. Programs
 * are constructed so they always terminate and never leave $02xx code:
 *
 *  - mode A (SMC): straight-line code that stores into its own future
 *    instruction bytes: immediate operands patched with arbitrary
 *    values, opcodes swapped only within a same-length whitelist so
 *    instruction alignment and forward progress are preserved.
 *  - mode B (branches): forward conditional branches to aligned future
 *    instruction boundaries, no code stores.
 *  - mode C (loops+SMC): a DEY/BNE-bounded loop whose body may patch
 *    the immediate operand of a post-loop instruction.
 */
#include <atf-c.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bus.h"
#include "device_ram.h"
#include "rk65c02.h"
#include "utils.h"

#define CODE_BASE 0x0200
#define DATA_BASE 0x0300
#define MAX_PROG 200		/* bytes of generated code */
#define MAX_INSNS 48
#define FUZZ_CASES 100
#define POLL_LIMIT 4000000ULL

/* xorshift32 PRNG - deterministic across platforms. */
static uint32_t
xs32(uint32_t *s)
{
	uint32_t x = *s;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*s = x;
	return x;
}

static uint32_t
rnd(uint32_t *s, uint32_t n)
{
	return xs32(s) % n;
}

/* Instruction menu entries: opcode + size (imm operand randomized). */
struct menu_insn {
	uint8_t opcode;
	uint8_t size;
};

/* 1-byte opcodes safe anywhere (no PC, no Y for mode C reuse). */
static const struct menu_insn menu1[] = {
	{ 0xEA, 1 }, /* NOP */  { 0x18, 1 }, /* CLC */
	{ 0x38, 1 }, /* SEC */  { 0xE8, 1 }, /* INX */
	{ 0xCA, 1 }, /* DEX */  { 0xAA, 1 }, /* TAX */
	{ 0x8A, 1 }, /* TXA */  { 0x0A, 1 }, /* ASL A */
	{ 0x4A, 1 }, /* LSR A */{ 0x2A, 1 }, /* ROL A */
	{ 0x6A, 1 }, /* ROR A */{ 0x1A, 1 }, /* INC A */
	{ 0x3A, 1 }, /* DEC A */{ 0x48, 1 }, /* PHA */
	{ 0x08, 1 }, /* PHP */  { 0x68, 1 }, /* PLA */
	{ 0xD8, 1 }, /* CLD */  { 0xF8, 1 }, /* SED */
	{ 0xB8, 1 }, /* CLV */
};

/* 2-byte immediate opcodes (also the same-length opcode-patch whitelist). */
static const struct menu_insn menu2[] = {
	{ 0xA9, 2 }, /* LDA # */{ 0xA2, 2 }, /* LDX # */
	{ 0x69, 2 }, /* ADC # */{ 0xE9, 2 }, /* SBC # */
	{ 0x29, 2 }, /* AND # */{ 0x09, 2 }, /* ORA # */
	{ 0x49, 2 }, /* EOR # */{ 0xC9, 2 }, /* CMP # */
	{ 0xE0, 2 }, /* CPX # */{ 0x89, 2 }, /* BIT # */
};

/* Generated instruction record. */
struct gen_insn {
	uint16_t addr;
	uint8_t bytes[3];
	uint8_t size;
	bool has_imm;		/* 2-byte immediate form (patchable) */
};

struct gen_prog {
	struct gen_insn insns[MAX_INSNS + 8];
	int n;
	uint16_t end;		/* first free address */
	uint8_t code[MAX_PROG];
	int code_len;
};

static void
prog_add(struct gen_prog *p, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t size,
    bool has_imm)
{
	struct gen_insn *gi = &p->insns[p->n];

	gi->addr = (uint16_t)(CODE_BASE + p->code_len);
	gi->bytes[0] = b0;
	gi->bytes[1] = b1;
	gi->bytes[2] = b2;
	gi->size = size;
	gi->has_imm = has_imm;
	p->code[p->code_len++] = b0;
	if (size > 1)
		p->code[p->code_len++] = b1;
	if (size > 2)
		p->code[p->code_len++] = b2;
	p->n++;
	p->end = (uint16_t)(CODE_BASE + p->code_len);
}

/* Random non-store, non-branch body instruction. Excludes Y ops. */
static void
gen_body_insn(struct gen_prog *p, uint32_t *s)
{
	if (rnd(s, 2) == 0) {
		const struct menu_insn *m = &menu1[rnd(s, sizeof(menu1) / sizeof(menu1[0]))];

		prog_add(p, m->opcode, 0, 0, 1, false);
	} else {
		const struct menu_insn *m = &menu2[rnd(s, sizeof(menu2) / sizeof(menu2[0]))];

		prog_add(p, m->opcode, (uint8_t)rnd(s, 256), 0, 2, true);
	}
}

/* Store to a plain data location (data page or upper ZP). */
static void
gen_data_store(struct gen_prog *p, uint32_t *s)
{
	uint16_t target;
	uint32_t which = rnd(s, 3);
	uint8_t op = (which == 0) ? 0x8D : (which == 1) ? 0x8E : 0x8C; /* STA/STX/STY abs */

	if (rnd(s, 2) == 0)
		target = (uint16_t)(DATA_BASE + rnd(s, 0x80));
	else
		target = (uint16_t)(0x0080 + rnd(s, 0x40)); /* upper ZP, abs form */
	prog_add(p, op, (uint8_t)(target & 0xFF), (uint8_t)(target >> 8), 3, false);
}

/*
 * Fill remaining planned slots then terminate with STP. Used by all modes.
 */
static void
gen_terminate(struct gen_prog *p)
{
	prog_add(p, 0xDB, 0, 0, 1, false); /* STP */
}

/* Mode A: straight-line with stores into future code bytes. */
static void
gen_mode_a(struct gen_prog *p, uint32_t *s)
{
	int planned = 12 + (int)rnd(s, MAX_INSNS - 16);
	int i;

	memset(p, 0, sizeof(*p));
	for (i = 0; i < planned; i++) {
		uint32_t roll = rnd(s, 10);

		if (p->n >= MAX_INSNS - 2)
			break;
		if (roll < 5) {
			gen_body_insn(p, s);
		} else if (roll < 7) {
			gen_data_store(p, s);
		} else if (roll < 9) {
			/*
			 * Patch the immediate operand of a future
			 * instruction: reserve space by planning it later -
			 * here we emit STA abs to (future estimated addr).
			 * We fix up targets after generation instead:
			 * emit a placeholder store to the data page now and
			 * retarget it below once addresses are final.
			 */
			prog_add(p, 0x8D, 0x00, 0x03, 3, false);
			p->insns[p->n - 1].has_imm = false;
			p->insns[p->n - 1].bytes[2] = 0xFE; /* mark: retarget */
		} else {
			/*
			 * Opcode patch pair: LDA #whitelisted ; STA opcode.
			 * Value chosen from the same-length whitelist at
			 * retarget time; emit LDA #idx placeholder + store.
			 */
			prog_add(p, 0xA9, (uint8_t)rnd(s, 256), 0, 2, false);
			prog_add(p, 0x8D, 0x00, 0x03, 3, false);
			p->insns[p->n - 1].has_imm = false;
			p->insns[p->n - 1].bytes[2] = 0xFD; /* mark: opcode patch */
		}
		if (p->code_len >= MAX_PROG - 8)
			break;
	}
	gen_terminate(p);

	/* Retarget marked stores to bytes of strictly-later instructions. */
	for (i = 0; i < p->n; i++) {
		struct gen_insn *gi = &p->insns[i];
		int j, cand[MAX_INSNS + 8], ncand = 0;

		if (gi->bytes[0] != 0x8D ||
		    (gi->bytes[2] != 0xFE && gi->bytes[2] != 0xFD))
			continue;
		for (j = i + 1; j < p->n; j++) {
			if (p->insns[j].has_imm)
				cand[ncand++] = j;
		}
		if (ncand == 0) {
			/* No later immediate insn: point at the data page. */
			gi->bytes[1] = (uint8_t)rnd(s, 0x80);
			gi->bytes[2] = 0x03;
		} else {
			struct gen_insn *t = &p->insns[cand[rnd(s, (uint32_t)ncand)]];
			uint16_t taddr;

			if ((gi->bytes[2] == 0xFD) && (i > 0)) {
				/*
				 * Opcode byte: safe only if the stored value
				 * comes from the 2-byte whitelist; rewrite
				 * the preceding LDA immediate accordingly.
				 */
				taddr = t->addr;
				p->insns[i - 1].bytes[1] =
				    menu2[rnd(s, sizeof(menu2) / sizeof(menu2[0]))].opcode;
			} else {
				/* Operand byte: any value is safe. */
				taddr = (uint16_t)(t->addr + 1);
			}
			gi->bytes[1] = (uint8_t)(taddr & 0xFF);
			gi->bytes[2] = (uint8_t)(taddr >> 8);
		}
		/* Reflect fixups into the flat code image. */
		p->code[gi->addr - CODE_BASE + 1] = gi->bytes[1];
		p->code[gi->addr - CODE_BASE + 2] = gi->bytes[2];
		if (gi->bytes[0] == 0x8D && i > 0 &&
		    p->insns[i - 1].bytes[0] == 0xA9)
			p->code[p->insns[i - 1].addr - CODE_BASE + 1] =
			    p->insns[i - 1].bytes[1];
	}
}

/* Mode B: forward conditional branches to aligned future boundaries. */
static void
gen_mode_b(struct gen_prog *p, uint32_t *s)
{
	static const uint8_t branches[] =
	    { 0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0 };
	int planned = 16 + (int)rnd(s, MAX_INSNS - 20);
	int i;

	memset(p, 0, sizeof(*p));
	for (i = 0; i < planned; i++) {
		uint32_t roll = rnd(s, 10);

		if (p->n >= MAX_INSNS - 2)
			break;
		if (roll < 6)
			gen_body_insn(p, s);
		else if (roll < 8)
			gen_data_store(p, s);
		else {
			/* Branch placeholder; offset patched below. */
			prog_add(p, branches[rnd(s, 8)], 0x00, 0, 2, false);
			p->insns[p->n - 1].bytes[2] = 0xFC; /* mark */
		}
		if (p->code_len >= MAX_PROG - 8)
			break;
	}
	gen_terminate(p);

	/* Patch branch offsets to later instruction boundaries. */
	for (i = 0; i < p->n; i++) {
		struct gen_insn *gi = &p->insns[i];
		int j, cand[MAX_INSNS + 8], ncand = 0;

		if (gi->bytes[2] != 0xFC || gi->size != 2)
			continue;
		gi->bytes[2] = 0;
		for (j = i + 1; j < p->n; j++) {
			int off = (int)p->insns[j].addr - ((int)gi->addr + 2);

			if (off >= 1 && off <= 127)
				cand[ncand++] = j;
		}
		if (ncand == 0)
			gi->bytes[1] = 0; /* branch to next insn */
		else
			gi->bytes[1] = (uint8_t)((int)p->insns[cand[rnd(s, (uint32_t)ncand)]].addr
			    - ((int)gi->addr + 2));
		p->code[gi->addr - CODE_BASE + 1] = gi->bytes[1];
	}
}

/* Mode C: LDY #k ; loop body (no Y ops) ; DEY ; BNE loop ; tail ; STP. */
static void
gen_mode_c(struct gen_prog *p, uint32_t *s)
{
	int body = 2 + (int)rnd(s, 10);
	int tail = 1 + (int)rnd(s, 4);
	uint16_t loop_addr;
	int i, off;
	int patch_slot = -1;

	memset(p, 0, sizeof(*p));
	prog_add(p, 0xA0, (uint8_t)(3 + rnd(s, 8)), 0, 2, false); /* LDY #k */
	loop_addr = p->end;
	for (i = 0; i < body; i++) {
		if ((rnd(s, 4) == 0) && (patch_slot == -1)) {
			/* Store that will patch a tail immediate operand. */
			prog_add(p, 0x8D, 0x00, 0x03, 3, false);
			patch_slot = p->n - 1;
		} else if (rnd(s, 5) == 0) {
			gen_data_store(p, s);
		} else {
			gen_body_insn(p, s);
		}
	}
	prog_add(p, 0x88, 0, 0, 1, false); /* DEY */
	off = (int)loop_addr - ((int)p->end + 2);
	ATF_REQUIRE(off >= -128 && off < 0);
	prog_add(p, 0xD0, (uint8_t)(int8_t)off, 0, 2, false); /* BNE loop */
	for (i = 0; i < tail; i++) {
		const struct menu_insn *m = &menu2[rnd(s, sizeof(menu2) / sizeof(menu2[0]))];

		prog_add(p, m->opcode, (uint8_t)rnd(s, 256), 0, 2, true);
	}
	gen_terminate(p);

	if (patch_slot >= 0) {
		/* Retarget the marked store at a tail immediate operand. */
		int j, cand[8], ncand = 0;

		for (j = 0; j < p->n; j++)
			if (p->insns[j].has_imm && p->insns[j].addr > p->insns[patch_slot].addr
			    && p->insns[j].bytes[0] != 0xD0)
				cand[ncand < 8 ? ncand++ : 7] = j;
		if (ncand > 0) {
			uint16_t taddr = (uint16_t)(p->insns[cand[rnd(s, (uint32_t)ncand)]].addr + 1);

			p->insns[patch_slot].bytes[1] = (uint8_t)(taddr & 0xFF);
			p->insns[patch_slot].bytes[2] = (uint8_t)(taddr >> 8);
			p->code[p->insns[patch_slot].addr - CODE_BASE + 1] =
			    p->insns[patch_slot].bytes[1];
			p->code[p->insns[patch_slot].addr - CODE_BASE + 2] =
			    p->insns[patch_slot].bytes[2];
		}
	}
}

struct fuzz_bound {
	uint64_t polls;
	bool tripped;
};

static void
fuzz_tick(rk65c02emu_t *e, void *ctx)
{
	struct fuzz_bound *fb = ctx;

	if (++fb->polls >= POLL_LIMIT) {
		fb->tripped = true;
		rk65c02_request_stop(e);
	}
}

struct fuzz_result {
	reg_state_t regs;
	emu_stop_reason_t stopreason;
	bool bounded;
	uint8_t mem[0x0400];
};

static void
fuzz_run(const struct gen_prog *p, bool use_jit, struct fuzz_result *r)
{
	rk65c02emu_t e;
	bus_t b;
	struct fuzz_bound fb = { 0, false };
	int i;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);

	for (i = 0; i < p->code_len; i++)
		bus_write_1(&b, (uint16_t)(CODE_BASE + i), p->code[i]);
	/* Guard tail: STP everywhere after the program. */
	for (; i < MAX_PROG + 16; i++)
		bus_write_1(&b, (uint16_t)(CODE_BASE + i), 0xDB);

	e.regs.PC = CODE_BASE;
	e.regs.SP = 0xFF;
	e.regs.A = 0x11;
	e.regs.X = 0x22;
	e.regs.Y = 0x33;
	e.regs.P = P_UNDEFINED;
	rk65c02_tick_set(&e, fuzz_tick, 0, &fb);
	rk65c02_start(&e);
	rk65c02_tick_clear(&e);

	r->regs = e.regs;
	r->stopreason = e.stopreason;
	r->bounded = fb.tripped;
	for (i = 0; i < 0x0400; i++)
		r->mem[i] = bus_read_1(&b, (uint16_t)i);
	bus_finish(&b);
}

static void
fuzz_compare(const struct gen_prog *p, uint32_t seed, int mode)
{
	struct fuzz_result ri, rj;
	int i;

	fuzz_run(p, false, &ri);
	fuzz_run(p, true, &rj);

	ATF_REQUIRE_MSG(!ri.bounded && !rj.bounded,
	    "mode %c seed %u: runaway program (interp=%d jit=%d)",
	    'A' + mode, seed, ri.bounded, rj.bounded);
	ATF_CHECK_MSG(ri.stopreason == STP && rj.stopreason == STP,
	    "mode %c seed %u: stopreason interp=%d jit=%d",
	    'A' + mode, seed, ri.stopreason, rj.stopreason);
	ATF_CHECK_MSG(ri.regs.A == rj.regs.A && ri.regs.X == rj.regs.X
	    && ri.regs.Y == rj.regs.Y && ri.regs.SP == rj.regs.SP
	    && ri.regs.P == rj.regs.P && ri.regs.PC == rj.regs.PC,
	    "mode %c seed %u: regs interp A=%02X X=%02X Y=%02X SP=%02X P=%02X PC=%04X"
	    " jit A=%02X X=%02X Y=%02X SP=%02X P=%02X PC=%04X",
	    'A' + mode, seed,
	    ri.regs.A, ri.regs.X, ri.regs.Y, ri.regs.SP, ri.regs.P, ri.regs.PC,
	    rj.regs.A, rj.regs.X, rj.regs.Y, rj.regs.SP, rj.regs.P, rj.regs.PC);
	for (i = 0; i < 0x0400; i++) {
		if (ri.mem[i] != rj.mem[i]) {
			ATF_CHECK_MSG(false,
			    "mode %c seed %u: mem[%04X] interp=%02X jit=%02X",
			    'A' + mode, seed, i, ri.mem[i], rj.mem[i]);
			break;
		}
	}
}

ATF_TC_WITHOUT_HEAD(smc_fuzz_straightline);
ATF_TC_BODY(smc_fuzz_straightline, tc)
{
	struct gen_prog p;
	uint32_t c;

	(void)tc;
	for (c = 0; c < FUZZ_CASES; c++) {
		uint32_t seed = 0xA0000001u + c * 7919u;
		uint32_t s = seed;

		gen_mode_a(&p, &s);
		fuzz_compare(&p, seed, 0);
	}
}

ATF_TC_WITHOUT_HEAD(smc_fuzz_branches);
ATF_TC_BODY(smc_fuzz_branches, tc)
{
	struct gen_prog p;
	uint32_t c;

	(void)tc;
	for (c = 0; c < FUZZ_CASES; c++) {
		uint32_t seed = 0xB0000001u + c * 104729u;
		uint32_t s = seed;

		gen_mode_b(&p, &s);
		fuzz_compare(&p, seed, 1);
	}
}

ATF_TC_WITHOUT_HEAD(smc_fuzz_loops);
ATF_TC_BODY(smc_fuzz_loops, tc)
{
	struct gen_prog p;
	uint32_t c;

	(void)tc;
	for (c = 0; c < FUZZ_CASES; c++) {
		uint32_t seed = 0xC0000001u + c * 65537u;
		uint32_t s = seed;

		gen_mode_c(&p, &s);
		fuzz_compare(&p, seed, 2);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, smc_fuzz_straightline);
	ATF_TP_ADD_TC(tp, smc_fuzz_branches);
	ATF_TP_ADD_TC(tp, smc_fuzz_loops);
	return atf_no_error();
}
