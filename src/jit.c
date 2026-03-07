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

#define JIT_CACHE_SIZE 65536
#define JIT_BLOCK_MAX_INSNS 64
#define JIT_MAX_ACTIVE_BLOCKS 1024
#define JIT_MAX_INVALIDATIONS_PER_RUN 128
#define JIT_PAGE_INVALIDATION_THRESHOLD 4
#define JIT_MAGIC 0x4a495431u  /* "JIT1" */
/* Enable optional per-run telemetry for profiling JIT behavior. */
#ifndef RK65C02_JIT_PERF_DEBUG
#define RK65C02_JIT_PERF_DEBUG 0
#endif

/* Offsets into emulator state for generated code (no magic numbers). */
#define OFFSET_E_STATE  offsetof(struct rk65c02emu, state)
#define OFFSET_E_REGS   offsetof(struct rk65c02emu, regs)
#define OFFSET_E_BUS    offsetof(struct rk65c02emu, bus)
#define OFFSET_E_USE_JIT offsetof(struct rk65c02emu, use_jit)
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
	uint16_t end_pc_exclusive;
	void (*fn)(rk65c02emu_t *);
#ifdef HAVE_LIGHTNING
	jit_state_t *lightning_state;  /* one state per block: stable fn, no buffer realloc */
#endif
};

struct rk65c02_jit {
	unsigned int magic;
	size_t active_blocks;
	bool needs_flush;
	bool write_event_pending;
	uint16_t last_write_addr;
	/* Runtime state used by adaptive invalidation policy. */
	uint64_t invalidation_events_this_run;
#if RK65C02_JIT_PERF_DEBUG
	/* Optional per-run counters for profiling fallback pressure. */
	uint64_t debug_blocks_executed;
	uint64_t debug_fallback_calls;
	uint64_t debug_write_events;
	uint64_t debug_invalidated_blocks;
	uint64_t debug_fallback_by_opcode[256];
#endif
	uint16_t compiled_page_refcnt[256];
	uint16_t covered_addr_refcnt[65536];
	uint16_t page_invalidation_events[256];
	bool mutable_code_page[256];
	struct rk65c02_jit_block *blocks[JIT_CACHE_SIZE];
};

/* One decoded instruction in a block. */
struct jit_block_insn {
	instruction_t i;
	instrdef_t id;
};

/* Forward declaration from rk65c02.c */
void rk65c02_exec(rk65c02emu_t *e);

#ifdef HAVE_LIGHTNING
static void jit_block_page_refs_update(struct rk65c02_jit *j,
    const struct rk65c02_jit_block *b, bool add);
#endif

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
		if ((pc >> 8) != (start_pc >> 8))
			break;
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
	j->active_blocks = 0;
	j->needs_flush = false;
	j->write_event_pending = false;
	j->last_write_addr = 0;
	j->invalidation_events_this_run = 0;
#if RK65C02_JIT_PERF_DEBUG
	j->debug_blocks_executed = 0;
	j->debug_fallback_calls = 0;
	j->debug_write_events = 0;
	j->debug_invalidated_blocks = 0;
	memset(j->debug_fallback_by_opcode, 0, sizeof(j->debug_fallback_by_opcode));
#endif
	memset(j->compiled_page_refcnt, 0, sizeof(j->compiled_page_refcnt));
	memset(j->covered_addr_refcnt, 0, sizeof(j->covered_addr_refcnt));
	memset(j->page_invalidation_events, 0, sizeof(j->page_invalidation_events));
	memset(j->mutable_code_page, 0, sizeof(j->mutable_code_page));
	memset(j->blocks, 0, sizeof(j->blocks));

	return j;
}

static void
jit_backend_flush(struct rk65c02_jit *j)
{
	size_t i;

	if (j == NULL)
		return;

#ifdef HAVE_LIGHTNING
	for (i = 0; i < JIT_CACHE_SIZE; i++) {
		struct rk65c02_jit_block *b;

		b = j->blocks[i];
		if (b == NULL)
			continue;
		if (b->lightning_state != NULL) {
			_jit_destroy_state(b->lightning_state);
			b->lightning_state = NULL;
		}
		if (b->fn != NULL)
			jit_block_page_refs_update(j, b, false);
		b->fn = NULL;
	}
#endif

	j->active_blocks = 0;
	j->needs_flush = false;
	j->write_event_pending = false;
	j->last_write_addr = 0;
	memset(j->compiled_page_refcnt, 0, sizeof(j->compiled_page_refcnt));
	memset(j->covered_addr_refcnt, 0, sizeof(j->covered_addr_refcnt));
	memset(j->page_invalidation_events, 0, sizeof(j->page_invalidation_events));
	memset(j->mutable_code_page, 0, sizeof(j->mutable_code_page));
}

static struct rk65c02_jit_block *
jit_find_block(struct rk65c02_jit *j, uint16_t pc)
{
	if (j == NULL || j->magic != JIT_MAGIC)
		return NULL;

	return j->blocks[pc];
}

#ifdef HAVE_LIGHTNING
static void
jit_block_page_refs_update(struct rk65c02_jit *j,
    const struct rk65c02_jit_block *b, bool add)
{
	uint16_t addr;
	uint8_t seen[256];
	uint32_t i;
	int delta;

	if ((j == NULL) || (b == NULL))
		return;
	memset(seen, 0, sizeof(seen));
	delta = add ? 1 : -1;
	if (b->start_pc == b->end_pc_exclusive) {
		memset(seen, 1, sizeof(seen));
		for (i = 0; i < 65536; i++) {
			int next;

			next = (int)j->covered_addr_refcnt[i] + delta;
			if (next < 0)
				next = 0;
			j->covered_addr_refcnt[i] = (uint16_t)next;
		}
	} else {
		addr = b->start_pc;
		while (addr != b->end_pc_exclusive) {
			int next;

			seen[addr >> 8] = 1;
			next = (int)j->covered_addr_refcnt[addr] + delta;
			if (next < 0)
				next = 0;
			j->covered_addr_refcnt[addr] = (uint16_t)next;
			addr = (uint16_t)(addr + 1);
		}
	}
	for (addr = 0; addr < 256; addr++) {
		int next;

		if (!seen[addr])
			continue;
		next = (int)j->compiled_page_refcnt[addr] + delta;
		if (next < 0)
			next = 0;
		j->compiled_page_refcnt[addr] = (uint16_t)next;
	}
}

static void
jit_run_state_reset(struct rk65c02_jit *j)
{
	if (j == NULL)
		return;
	j->invalidation_events_this_run = 0;
#if RK65C02_JIT_PERF_DEBUG
	j->debug_blocks_executed = 0;
	j->debug_fallback_calls = 0;
	j->debug_write_events = 0;
	j->debug_invalidated_blocks = 0;
	memset(j->debug_fallback_by_opcode, 0, sizeof(j->debug_fallback_by_opcode));
#endif
	memset(j->page_invalidation_events, 0, sizeof(j->page_invalidation_events));
	memset(j->mutable_code_page, 0, sizeof(j->mutable_code_page));
}

static void
jit_run_debug_log(const struct rk65c02_jit *j)
{
#if RK65C02_JIT_PERF_DEBUG
	int k;
	int m;

	if (j == NULL)
		return;
	rk65c02_log(LOG_DEBUG,
	    "JIT run stats: blocks=%llu fallback=%llu writes=%llu invalidations=%llu invalidated_blocks=%llu",
	    (unsigned long long)j->debug_blocks_executed,
	    (unsigned long long)j->debug_fallback_calls,
	    (unsigned long long)j->debug_write_events,
	    (unsigned long long)j->invalidation_events_this_run,
	    (unsigned long long)j->debug_invalidated_blocks);
	for (k = 0; k < 256; k++) {
		if (j->debug_fallback_by_opcode[k] == 0)
			continue;
		rk65c02_log(LOG_DEBUG, "JIT fallback opcode $%02X: %llu",
		    k, (unsigned long long)j->debug_fallback_by_opcode[k]);
	}
	for (m = 0; m < 256; m++) {
		if (!j->mutable_code_page[m])
			continue;
		rk65c02_log(LOG_DEBUG, "JIT mutable code page $%02X: invalidations=%u",
		    m, (unsigned)j->page_invalidation_events[m]);
	}
#else
	(void)j;
#endif
}

static bool
jit_block_contains_addr(const struct rk65c02_jit_block *b, uint16_t addr)
{
	uint16_t start, end;

	if (b == NULL)
		return false;
	start = b->start_pc;
	end = b->end_pc_exclusive;
	if (start < end)
		return addr >= start && addr < end;
	if (start > end)
		return (addr >= start) || (addr < end);
	/* start == end means full 64K coverage by wrap; treat as all addresses. */
	return true;
}

static void
jit_invalidate_blocks_for_addr(struct rk65c02_jit *j, uint16_t addr)
{
	size_t i;

	if ((j == NULL) || (j->magic != JIT_MAGIC))
		return;
	j->invalidation_events_this_run++;
	for (i = 0; i < JIT_CACHE_SIZE; i++) {
		struct rk65c02_jit_block *b;

		b = j->blocks[i];
		if ((b == NULL) || (b->fn == NULL))
			continue;
		if (!jit_block_contains_addr(b, addr))
			continue;
		if (b->lightning_state != NULL) {
			_jit_destroy_state(b->lightning_state);
			b->lightning_state = NULL;
		}
		jit_block_page_refs_update(j, b, false);
		b->fn = NULL;
		if (j->active_blocks > 0)
			j->active_blocks--;
#if RK65C02_JIT_PERF_DEBUG
		j->debug_invalidated_blocks++;
#endif
	}
}

static bool
jit_block_overlaps_page(const struct rk65c02_jit_block *b, uint8_t page)
{
	uint16_t start, end, page_start, page_end;

	if (b == NULL)
		return false;
	start = b->start_pc;
	end = b->end_pc_exclusive;
	page_start = (uint16_t)(page << 8);
	page_end = (uint16_t)(page_start + 0xFF);
	if (start == end)
		return true;
	if (start < end)
		return !(end <= page_start || start > page_end);
	/* Wrapped range: [start,0xFFFF] U [0,end). */
	return (start <= page_end) || (end > page_start);
}

static void
jit_invalidate_blocks_for_page(struct rk65c02_jit *j, uint8_t page)
{
	size_t i;

	if ((j == NULL) || (j->magic != JIT_MAGIC))
		return;
	for (i = 0; i < JIT_CACHE_SIZE; i++) {
		struct rk65c02_jit_block *b;

		b = j->blocks[i];
		if ((b == NULL) || (b->fn == NULL))
			continue;
		if (!jit_block_overlaps_page(b, page))
			continue;
		if (b->lightning_state != NULL) {
			_jit_destroy_state(b->lightning_state);
			b->lightning_state = NULL;
		}
		jit_block_page_refs_update(j, b, false);
		b->fn = NULL;
		if (j->active_blocks > 0)
			j->active_blocks--;
#if RK65C02_JIT_PERF_DEBUG
		j->debug_invalidated_blocks++;
#endif
	}
}

/* Write helper for JIT-native stores. */
static void
jit_bus_write_1(rk65c02emu_t *e, uint16_t addr, uint8_t val)
{
	assert(e != NULL);

	bus_write_1(e->bus, addr, val);
#if RK65C02_JIT_PERF_DEBUG
	if ((e->jit != NULL) && (e->jit->magic == JIT_MAGIC))
		e->jit->debug_write_events++;
#endif
	if ((e->use_jit) && (e->jit != NULL) && (e->jit->magic == JIT_MAGIC)
	    && (e->jit->compiled_page_refcnt[addr >> 8] != 0)
	    && (e->jit->covered_addr_refcnt[addr] != 0)) {
		/* Force immediate block bail-out; dispatcher will invalidate spans. */
		e->use_jit = false;
		e->jit->write_event_pending = true;
		e->jit->last_write_addr = addr;
	}
}

static void
jit_exec_fallback(rk65c02emu_t *e, uint8_t opcode)
{
#if RK65C02_JIT_PERF_DEBUG
	if ((e != NULL) && (e->jit != NULL) && (e->jit->magic == JIT_MAGIC)) {
		e->jit->debug_fallback_calls++;
		e->jit->debug_fallback_by_opcode[opcode]++;
	}
#else
	(void)opcode;
#endif
	rk65c02_exec(e);
}

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
	jit_prepare();
	jit_pushargr(JIT_R0);
	jit_pushargr(JIT_R1);
	jit_pushargr(JIT_R2);
	jit_finishi((void *)jit_bus_write_1);
	jit_movr(JIT_R0, JIT_V0);
}

/*
 * If this instruction disabled JIT (e.g. due to self-modifying code write),
 * branch to block return so modified bytes are not executed stale in this
 * same compiled block.
 */
static jit_node_t *
jit_emit_bail_if_jit_disabled(void)
{
	jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_USE_JIT);
	return jit_beqi(JIT_R1, 0);
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
	case 0xEA: /* NOP */
		jit_emit_advance_pc(size);
		return NULL;

	/* --- Flag modifiers --- */
	case 0x18: /* CLC */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R1, JIT_R1, ~P_CARRY);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;
	case 0x38: /* SEC */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_P);
		jit_ori(JIT_R1, JIT_R1, P_CARRY);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;
	case 0x58: /* CLI */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R1, JIT_R1, ~P_IRQ_DISABLE);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;
	case 0x78: /* SEI */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_P);
		jit_ori(JIT_R1, JIT_R1, P_IRQ_DISABLE);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;
	case 0xB8: /* CLV */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R1, JIT_R1, ~P_SIGN_OVERFLOW);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;
	case 0xD8: /* CLD */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R1, JIT_R1, ~P_DECIMAL);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;
	case 0xF8: /* SED */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_P);
		jit_ori(JIT_R1, JIT_R1, P_DECIMAL);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;

	/* --- Transfer --- */
	case 0xAA: /* TAX */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_A);
		jit_stxi_c(OFFSET_E_X, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case 0xA8: /* TAY */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_A);
		jit_stxi_c(OFFSET_E_Y, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case 0x8A: /* TXA */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_X);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case 0x98: /* TYA */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_Y);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case 0xBA: /* TSX */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_SP);
		jit_stxi_c(OFFSET_E_X, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case 0x9A: /* TXS */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_X);
		jit_stxi_c(OFFSET_E_SP, JIT_R0, JIT_R1);
		jit_emit_advance_pc(size);
		return NULL;

	/* --- INX/DEX/INY/DEY --- */
	case 0xE8: /* INX */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_X);
		jit_addi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_X, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case 0xCA: /* DEX */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_X);
		jit_subi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_X, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case 0xC8: /* INY */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_Y);
		jit_addi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_Y, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		return NULL;
	case 0x88: /* DEY */
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
		return jit_emit_bail_if_jit_disabled();

	/* --- STX (all modes) --- */
	case 0x86: case 0x96: case 0x8E:
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_X);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return jit_emit_bail_if_jit_disabled();

	/* --- STY (all modes) --- */
	case 0x84: case 0x94: case 0x8C:
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_Y);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return jit_emit_bail_if_jit_disabled();

	/* --- STZ (all modes) --- */
	case 0x64: case 0x74: case 0x9C: case 0x9E:
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_movi(JIT_R2, 0);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return jit_emit_bail_if_jit_disabled();

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

	/* --- ADC (all modes): A = A + M + C, update N/Z/C/V; BCD when P_DECIMAL --- */
	case 0x69: case 0x65: case 0x75: case 0x6D: case 0x7D: case 0x79:
	case 0x61: case 0x71: case 0x72: {
		jit_node_t *binary_path, *adc_done;
		if (!jit_emit_load_operand(bi, arg_node))
			break;
		/* If decimal mode, call BCD helper and advance PC. */
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R2, JIT_R2, P_DECIMAL);
		binary_path = jit_beqi(JIT_R2, 0);
		jit_prepare();
		jit_pushargr(JIT_R0);
		jit_pushargr(JIT_R1);
		jit_finishi((void *)rk65c02_do_adc_bcd);
		jit_movr(JIT_R0, JIT_V0);
		jit_emit_advance_pc(size);
		adc_done = jit_jmpi();
		jit_patch(binary_path);
		/* Binary path: R1 = operand (M), save M and A, compute A+M+carry. */
		jit_movr(JIT_V1, JIT_R1);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_A);
		jit_movr(JIT_V2, JIT_R2);
		jit_addr(JIT_R1, JIT_R2, JIT_R1);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R2, JIT_R2, P_CARRY);
		{
			jit_node_t *no_carry = jit_beqi(JIT_R2, 0);
			jit_addi(JIT_R1, JIT_R1, 1);
			jit_patch(no_carry);
		}
		/* R2 = carry bit (0x100), R1 = 8-bit result. */
		jit_movr(JIT_R2, JIT_R1);
		jit_andi(JIT_R2, JIT_R2, 0x100);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		/* V = (A ^ res) & (M ^ res) & 0x80 */
		jit_xorr(JIT_V2, JIT_V2, JIT_R1);
		jit_xorr(JIT_V1, JIT_V1, JIT_R1);
		jit_andr(JIT_V2, JIT_V2, JIT_V1);
		jit_andi(JIT_V2, JIT_V2, 0x80);
		jit_ldxi_uc(JIT_V1, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_V1, JIT_V1, ~(P_CARRY | P_SIGN_OVERFLOW));
		{
			jit_node_t *no_c = jit_beqi(JIT_R2, 0);
			jit_ori(JIT_V1, JIT_V1, P_CARRY);
			jit_patch(no_c);
		}
		{
			jit_node_t *no_v = jit_beqi(JIT_V2, 0);
			jit_ori(JIT_V1, JIT_V1, P_SIGN_OVERFLOW);
			jit_patch(no_v);
		}
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_V1);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		jit_patch(adc_done);
		return NULL;
	}

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

	/* --- SBC (all modes): A = A - M - (1-C), update N/Z/C/V; BCD when P_DECIMAL --- */
	case 0xE9: case 0xE5: case 0xF5: case 0xED: case 0xFD: case 0xF9:
	case 0xE1: case 0xF1: case 0xF2: {
		jit_node_t *binary_path, *sbc_done;
		if (!jit_emit_load_operand(bi, arg_node))
			break;
		/* If decimal mode, call BCD helper and advance PC. */
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R2, JIT_R2, P_DECIMAL);
		binary_path = jit_beqi(JIT_R2, 0);
		jit_prepare();
		jit_pushargr(JIT_R0);
		jit_pushargr(JIT_R1);
		jit_finishi((void *)rk65c02_do_sbc_bcd);
		jit_movr(JIT_R0, JIT_V0);
		jit_emit_advance_pc(size);
		sbc_done = jit_jmpi();
		jit_patch(binary_path);
		/* Binary path: SBC is A + (~M) + C. R1 = operand (M), then ~M. */
		jit_xori(JIT_R1, JIT_R1, 0xFF);
		jit_movr(JIT_V1, JIT_R1);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_A);
		jit_movr(JIT_V2, JIT_R2);
		jit_addr(JIT_R1, JIT_R2, JIT_R1);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R2, JIT_R2, P_CARRY);
		{
			jit_node_t *no_carry = jit_beqi(JIT_R2, 0);
			jit_addi(JIT_R1, JIT_R1, 1);
			jit_patch(no_carry);
		}
		/* R2 = carry bit (0x100), R1 = 8-bit result. */
		jit_movr(JIT_R2, JIT_R1);
		jit_andi(JIT_R2, JIT_R2, 0x100);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		/* V = (A ^ res) & (~M ^ res) & 0x80 (operand was complemented). */
		jit_xorr(JIT_V2, JIT_V2, JIT_R1);
		jit_xorr(JIT_V1, JIT_V1, JIT_R1);
		jit_andr(JIT_V2, JIT_V2, JIT_V1);
		jit_andi(JIT_V2, JIT_V2, 0x80);
		jit_ldxi_uc(JIT_V1, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_V1, JIT_V1, ~(P_CARRY | P_SIGN_OVERFLOW));
		{
			jit_node_t *no_c = jit_beqi(JIT_R2, 0);
			jit_ori(JIT_V1, JIT_V1, P_CARRY);
			jit_patch(no_c);
		}
		{
			jit_node_t *no_v = jit_beqi(JIT_V2, 0);
			jit_ori(JIT_V1, JIT_V1, P_SIGN_OVERFLOW);
			jit_patch(no_v);
		}
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_V1);
		jit_stxi_c(OFFSET_E_A, JIT_R0, JIT_R1);
		jit_emit_update_zn();
		jit_emit_advance_pc(size);
		jit_patch(sbc_done);
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
		return jit_emit_bail_if_jit_disabled();

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
		return jit_emit_bail_if_jit_disabled();

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

	/* --- ASL memory (read-modify-write) --- */
	case 0x06: case 0x16: case 0x0E: case 0x1E: {
		jit_node_t *nc;
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_movr(JIT_V1, JIT_R1);
		jit_emit_bus_read(arg_node);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R2, JIT_R2, ~P_CARRY);
		nc = jit_bmci(JIT_R1, 0x80);
		jit_ori(JIT_R2, JIT_R2, P_CARRY);
		jit_patch(nc);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R2);
		jit_lshi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_movr(JIT_V2, JIT_R1);
		jit_emit_update_zn();
		jit_movr(JIT_R1, JIT_V1);
		jit_movr(JIT_R2, JIT_V2);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return jit_emit_bail_if_jit_disabled();
	}

	/* --- LSR memory --- */
	case 0x46: case 0x56: case 0x4E: case 0x5E: {
		jit_node_t *nc;
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_movr(JIT_V1, JIT_R1);
		jit_emit_bus_read(arg_node);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R2, JIT_R2, ~P_CARRY);
		nc = jit_bmci(JIT_R1, 0x01);
		jit_ori(JIT_R2, JIT_R2, P_CARRY);
		jit_patch(nc);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R2);
		jit_rshi_u(JIT_R1, JIT_R1, 1);
		jit_movr(JIT_V2, JIT_R1);
		jit_emit_update_zn();
		jit_movr(JIT_R1, JIT_V1);
		jit_movr(JIT_R2, JIT_V2);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return jit_emit_bail_if_jit_disabled();
	}

	/* --- ROL memory --- */
	case 0x26: case 0x36: case 0x2E: case 0x3E: {
		jit_node_t *nc;
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_movr(JIT_V1, JIT_R1);
		jit_emit_bus_read(arg_node);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_V2, JIT_R2, P_CARRY);
		jit_andi(JIT_R2, JIT_R2, ~P_CARRY);
		nc = jit_bmci(JIT_R1, 0x80);
		jit_ori(JIT_R2, JIT_R2, P_CARRY);
		jit_patch(nc);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R2);
		jit_lshi(JIT_R1, JIT_R1, 1);
		jit_orr(JIT_R1, JIT_R1, JIT_V2);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_movr(JIT_V2, JIT_R1);
		jit_emit_update_zn();
		jit_movr(JIT_R1, JIT_V1);
		jit_movr(JIT_R2, JIT_V2);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return jit_emit_bail_if_jit_disabled();
	}

	/* --- ROR memory --- */
	case 0x66: case 0x76: case 0x6E: case 0x7E: {
		jit_node_t *nc;
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_movr(JIT_V1, JIT_R1);
		jit_emit_bus_read(arg_node);
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_V2, JIT_R2, P_CARRY);
		jit_lshi(JIT_V2, JIT_V2, 7);
		jit_andi(JIT_R2, JIT_R2, ~P_CARRY);
		nc = jit_bmci(JIT_R1, 0x01);
		jit_ori(JIT_R2, JIT_R2, P_CARRY);
		jit_patch(nc);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R2);
		jit_rshi_u(JIT_R1, JIT_R1, 1);
		jit_orr(JIT_R1, JIT_R1, JIT_V2);
		jit_movr(JIT_V2, JIT_R1);
		jit_emit_update_zn();
		jit_movr(JIT_R1, JIT_V1);
		jit_movr(JIT_R2, JIT_V2);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return jit_emit_bail_if_jit_disabled();
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

	/* --- RMBx: clear bit in zero page memory --- */
	case 0x07: case 0x17: case 0x27: case 0x37:
	case 0x47: case 0x57: case 0x67: case 0x77: {
		uint8_t mask = (uint8_t)~(1u << ((op >> 4) & 0x07));
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_movr(JIT_V1, JIT_R1);
		jit_emit_bus_read(arg_node);
		jit_andi(JIT_R2, JIT_R1, mask);
		jit_movr(JIT_R1, JIT_V1);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return jit_emit_bail_if_jit_disabled();
	}

	/* --- SMBx: set bit in zero page memory --- */
	case 0x87: case 0x97: case 0xA7: case 0xB7:
	case 0xC7: case 0xD7: case 0xE7: case 0xF7: {
		uint8_t mask = (uint8_t)(1u << ((op >> 4) & 0x07));
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_movr(JIT_V1, JIT_R1);
		jit_emit_bus_read(arg_node);
		jit_ori(JIT_R2, JIT_R1, mask);
		jit_movr(JIT_R1, JIT_V1);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return jit_emit_bail_if_jit_disabled();
	}

	/* --- TRB/TSB: test memory with A; update Z; then reset/set bits --- */
	case 0x14: case 0x1C: /* TRB ZP/ABS */
	case 0x04: case 0x0C: { /* TSB ZP/ABS */
		jit_node_t *nz;
		bool is_trb = (op == 0x14 || op == 0x1C);
		if (!jit_emit_effaddr(bi, arg_node))
			break;
		jit_movr(JIT_V1, JIT_R1);             /* save effective address */
		jit_emit_bus_read(arg_node);          /* R1 = mem */
		jit_movr(JIT_V2, JIT_R1);             /* save mem */
		/* Update Z based on (A & mem) == 0; only Z changes. */
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_P);
		jit_andi(JIT_R2, JIT_R2, (uint8_t)~P_ZERO);
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_A);
		jit_andr(JIT_R1, JIT_R1, JIT_V2);
		nz = jit_bnei(JIT_R1, 0);
		jit_ori(JIT_R2, JIT_R2, P_ZERO);
		jit_patch(nz);
		jit_stxi_c(OFFSET_E_P, JIT_R0, JIT_R2);
		/* Compute written value into R2. */
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_A);
		if (is_trb) {
			jit_xori(JIT_R2, JIT_R2, 0xFF); /* ~A */
			jit_andr(JIT_R2, JIT_V2, JIT_R2);
		} else {
			jit_orr(JIT_R2, JIT_V2, JIT_R2);
		}
		jit_movr(JIT_R1, JIT_V1);
		jit_emit_bus_write(arg_node);
		jit_emit_advance_pc(size);
		return jit_emit_bail_if_jit_disabled();
	}

	/* --- BBRx/BBSx: test ZP bit and branch relative --- */
	case 0x0F: case 0x1F: case 0x2F: case 0x3F:
	case 0x4F: case 0x5F: case 0x6F: case 0x7F: /* BBR0..7 */
	case 0x8F: case 0x9F: case 0xAF: case 0xBF:
	case 0xCF: case 0xDF: case 0xEF: case 0xFF: { /* BBS0..7 */
		jit_node_t *take, *done;
		uint8_t bit = (op >> 4) & 0x07;
		uint8_t mask = (uint8_t)(1u << bit);
		int8_t rel = (int8_t)bi->i.op2;
		bool is_bbs = (op & 0x80) != 0;
		/* Read zero page operand for bit test. */
		jit_movi(JIT_R1, bi->i.op1);
		jit_emit_bus_read(arg_node);
		if (is_bbs)
			take = jit_bmsi(JIT_R1, mask); /* branch if tested bit is set */
		else
			take = jit_bmci(JIT_R1, mask); /* branch if tested bit is clear */
		/* Not taken path. */
		jit_emit_advance_pc(size);
		done = jit_jmpi();
		/* Taken path. */
		jit_patch(take);
		jit_ldxi_us(JIT_R1, JIT_R0, OFFSET_E_PC);
		jit_addi(JIT_R1, JIT_R1, (int)size + (int)rel);
		jit_andi(JIT_R1, JIT_R1, 0xFFFF);
		jit_stxi_s(OFFSET_E_PC, JIT_R0, JIT_R1);
		jit_patch(done);
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
		return jit_emit_bail_if_jit_disabled();
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

	/* --- JMP indirect (0x6C): PC = (mem[addr+1]<<8) | mem[addr] --- */
	case 0x6C: {
		uint16_t iaddr = bi->i.op1 | (bi->i.op2 << 8);
		uint16_t iaddr2 = (uint16_t)(iaddr + 1);
		jit_movi(JIT_R1, iaddr);
		jit_emit_bus_read(arg_node);
		jit_movr(JIT_V1, JIT_R1);
		jit_movi(JIT_R1, iaddr2);
		jit_emit_bus_read(arg_node);
		jit_lshi(JIT_R1, JIT_R1, 8);
		jit_orr(JIT_R1, JIT_R1, JIT_V1);
		jit_stxi_s(OFFSET_E_PC, JIT_R0, JIT_R1);
		return NULL;
	}

	/* --- JSR (0x20): push (PC+2) high then low, PC = target --- */
	case 0x20: {
		uint16_t jumpaddr = bi->i.op1 | (bi->i.op2 << 8);
		/* retaddr = PC + 2 (address of byte after JSR) */
		jit_ldxi_us(JIT_R1, JIT_R0, OFFSET_E_PC);
		jit_addi(JIT_R1, JIT_R1, 2);
		jit_andi(JIT_R1, JIT_R1, 0xFFFF);
		/* Keep retaddr in V2: bus_write uses V1 internally. */
		jit_movr(JIT_V2, JIT_R1);
		/* Push high: addr = 0x100+SP, write high, SP-- */
		jit_ldxi_uc(JIT_R2, JIT_R0, OFFSET_E_SP);
		jit_addi(JIT_R1, JIT_R2, 0x100);
		jit_rshi_u(JIT_R2, JIT_V2, 8);
		jit_emit_bus_write(arg_node);
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_SP);
		jit_subi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_SP, JIT_R0, JIT_R1);
		/* Push low: addr = 0x100+SP, write low, SP-- */
		jit_addi(JIT_R1, JIT_R1, 0x100);
		jit_andi(JIT_V2, JIT_V2, 0xFF);
		jit_movr(JIT_R2, JIT_V2);
		jit_emit_bus_write(arg_node);
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_SP);
		jit_subi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_SP, JIT_R0, JIT_R1);
		/* PC = jumpaddr */
		jit_movi(JIT_R1, jumpaddr);
		jit_stxi_s(OFFSET_E_PC, JIT_R0, JIT_R1);
		return jit_emit_bail_if_jit_disabled();
	}

	/* --- RTS (0x60): PC = pop() | (pop()<<8) --- */
	case 0x60: {
		/* SP++, read from 0x100+SP -> low */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_SP);
		jit_addi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_SP, JIT_R0, JIT_R1);
		jit_addi(JIT_R1, JIT_R1, 0x100);
		jit_emit_bus_read(arg_node);
		jit_movr(JIT_V1, JIT_R1);
		/* SP++, read from 0x100+SP -> high */
		jit_ldxi_uc(JIT_R1, JIT_R0, OFFSET_E_SP);
		jit_addi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFF);
		jit_stxi_c(OFFSET_E_SP, JIT_R0, JIT_R1);
		jit_addi(JIT_R1, JIT_R1, 0x100);
		jit_emit_bus_read(arg_node);
		jit_lshi(JIT_R1, JIT_R1, 8);
		jit_orr(JIT_R1, JIT_R1, JIT_V1);
		/* Interpreter increments PC by 1 after RTS (modify_pc=false). */
		jit_addi(JIT_R1, JIT_R1, 1);
		jit_andi(JIT_R1, JIT_R1, 0xFFFF);
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
	jit_pushargi(op);
	jit_finishi((void *)jit_exec_fallback);
	jit_movr(JIT_R0, JIT_V0);

	/*
	 * Fallback opcodes that modify PC must end the current native block.
	 * For others we continue, but still bail if execution stopped.
	 */
	if (bi->id.modify_pc) {
		bail = jit_jmpi();
	} else {
		jit_ldxi_i(JIT_R1, JIT_R0, OFFSET_E_STATE);
		bail = jit_bnei(JIT_R1, RUNNING);
	}

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
	uint16_t block_end;

	if (j == NULL || j->magic != JIT_MAGIC || bus == NULL)
		return NULL;

	b = j->blocks[pc];
	if (b == NULL) {
		b = (struct rk65c02_jit_block *)GC_MALLOC(sizeof(struct rk65c02_jit_block));
		assert(b != NULL);
		b->start_pc = pc;
		b->end_pc_exclusive = pc;
		b->fn = NULL;
#ifdef HAVE_LIGHTNING
		b->lightning_state = NULL;
#endif
		j->blocks[pc] = b;
	}
	if (b->fn != NULL)
		return b;

#ifdef HAVE_LIGHTNING
	{
		jit_node_t *arg_node;
		jit_node_t *bail_nodes[JIT_BLOCK_MAX_INSNS];
		jit_state_t *state;

		jit_build_block_insns(bus, pc, insns, JIT_BLOCK_MAX_INSNS,
		    &num_insns);
		block_end = pc;
		for (k = 0; k < num_insns; k++)
			block_end = (uint16_t)(block_end + insns[k].id.size);
		b->end_pc_exclusive = block_end;

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
		if (b->fn != NULL) {
			jit_block_page_refs_update(j, b, true);
			j->active_blocks++;
		}
	}
#else
	(void)bus;
	(void)insns;
	(void)num_insns;
	(void)k;
#endif

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
		e->jit_requested = true;
		e->use_jit = true;
	} else {
		e->jit_requested = false;
		e->use_jit = false;
	}
#else
	(void)enable;
	e->jit_requested = false;
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
	bool disable_jit_for_run;

	assert(e != NULL);

	/*
	 * If JIT is not available or not enabled, fall back to the
	 * regular interpreter loop.
	 */
	if (!(e->use_jit) || (e->jit == NULL)
	    || e->trace || e->runtime_disassembly
	    || (e->bps_head != NULL)) {
		e->state = RUNNING;
		while (e->state == RUNNING) {
			rk65c02_poll_host_controls(e);
			if (e->state != RUNNING) {
				if (rk65c02_maybe_wait_on_idle(e))
					continue;
				break;
			}
			rk65c02_exec(e);
			rk65c02_poll_host_controls(e);
			if (e->state != RUNNING) {
				if (rk65c02_maybe_wait_on_idle(e) &&
				    e->state == RUNNING)
					continue;
				break;
			}
		}
		return;
	}

	e->state = RUNNING;
	jit_run_state_reset(e->jit);
	disable_jit_for_run = false;

	while (e->state == RUNNING) {
		struct rk65c02_jit_block *b;
		uint16_t pc;

		rk65c02_poll_host_controls(e);
		if (e->state != RUNNING) {
			if (rk65c02_maybe_wait_on_idle(e))
				continue;
			break;
		}

		/*
		 * Honour any runtime changes in debugging state by
		 * bailing out to the interpreter if needed.
		 */
		if (e->trace || e->runtime_disassembly
		    || (e->bps_head != NULL))
			break;
		if (!(e->use_jit) || (e->jit == NULL)) {
			while (e->state == RUNNING) {
				rk65c02_poll_host_controls(e);
				if (e->state != RUNNING) {
					if (rk65c02_maybe_wait_on_idle(e))
						continue;
					break;
				}
				rk65c02_exec(e);
				rk65c02_poll_host_controls(e);
				if (e->state != RUNNING) {
					if (rk65c02_maybe_wait_on_idle(e) &&
					    e->state == RUNNING)
						continue;
					break;
				}
			}
			jit_run_debug_log(e->jit);
			return;
		}

		pc = e->regs.PC;
		if (e->jit->mutable_code_page[pc >> 8]) {
			rk65c02_exec(e);
			continue;
		}
		b = jit_find_block(e->jit, pc);
		if ((b == NULL) || (b->fn == NULL)) {
			if (e->jit->active_blocks >= JIT_MAX_ACTIVE_BLOCKS) {
				/*
				 * Cap active native blocks to keep JIT memory bounded
				 * during very long-running workloads (e.g. functional ROMs).
				 */
				jit_backend_flush(e->jit);
			}
			b = jit_compile_block(e->jit, pc, e->bus);
		}

		if ((b == NULL) || (b->fn == NULL)) {
			rk65c02_exec(e);
			continue;
		}

#if RK65C02_JIT_PERF_DEBUG
		e->jit->debug_blocks_executed++;
#endif
		b->fn(e);
		if (e->jit->write_event_pending) {
			uint8_t write_page = (uint8_t)(e->jit->last_write_addr >> 8);

			jit_invalidate_blocks_for_addr(e->jit, e->jit->last_write_addr);
			e->jit->page_invalidation_events[write_page]++;
			if (!e->jit->mutable_code_page[write_page] &&
			    e->jit->page_invalidation_events[write_page] >= JIT_PAGE_INVALIDATION_THRESHOLD) {
				e->jit->mutable_code_page[write_page] = true;
				jit_invalidate_blocks_for_page(e->jit, write_page);
			}
			e->jit->write_event_pending = false;
			if (e->jit->invalidation_events_this_run >= JIT_MAX_INVALIDATIONS_PER_RUN)
				disable_jit_for_run = true;
			e->use_jit = disable_jit_for_run ? false : e->jit_requested;
		}
		if (e->jit->needs_flush)
			jit_backend_flush(e->jit);
		rk65c02_poll_host_controls(e);
		if (e->state != RUNNING) {
			if (rk65c02_maybe_wait_on_idle(e) &&
			    e->state == RUNNING)
				continue;
			break;
		}
	}
	jit_run_debug_log(e->jit);
}

