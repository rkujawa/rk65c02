/*
 *      SPDX-License-Identifier: GPL-3.0-only
 *
 *      rk65c02
 *      Copyright (C) 2017-2021  Radoslaw Kujawa
 *
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, version 3 of the License.
 * 
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include <errno.h>
#include <assert.h>
#include <string.h>

#include <gc/gc.h>

#include "bus.h"
#include "instruction.h"
#include "rk65c02.h"
#include "log.h"
#include "debug.h"
#include "jit.h"

void rk65c02_exec(rk65c02emu_t *);
static void rk65c02_mmu_refresh_accessors(rk65c02emu_t *e);

static uint8_t
rk65c02_mem_read_direct(rk65c02emu_t *e, uint16_t addr,
    rk65c02_mmu_access_t access)
{
	(void)access;
	return bus_read_1(e->bus, addr);
}

static void
rk65c02_mem_write_direct(rk65c02emu_t *e, uint16_t addr, uint8_t val,
    rk65c02_mmu_access_t access)
{
	(void)access;
	bus_write_1(e->bus, addr, val);
}

static bool
rk65c02_mmu_translate_addr(rk65c02emu_t *e, uint16_t vaddr,
    rk65c02_mmu_access_t access, uint8_t req_perm, uint32_t *paddr_out,
    uint16_t *fault_code_out)
{
	struct rk65c02_mmu_tlb_entry *te;
	rk65c02_mmu_result_t r;
	uint8_t vpage;

	if ((e == NULL) || (paddr_out == NULL) || (fault_code_out == NULL))
		return false;
	vpage = (uint8_t)(vaddr >> 8);
	te = &e->mmu_tlb[vpage];
	if (e->mmu_tlb_enabled && te->valid
	    && (te->vpage == vpage)
	    && (te->epoch == e->mmu_epoch)) {
		e->mmu_tlb_hits++;
		if ((te->perms & req_perm) != req_perm) {
			*fault_code_out = 0xFFFEu;
			return false;
		}
		*paddr_out = (uint32_t)(te->ppage_base | (vaddr & 0x00FFu));
		return true;
	}
	e->mmu_tlb_misses++;

	if (e->mmu_translate == NULL) {
		*fault_code_out = 0xFFFDu;
		return false;
	}

	r = e->mmu_translate(e, vaddr, access, e->mmu_translate_ctx);
	if (!r.ok) {
		*fault_code_out = r.fault_code;
		return false;
	}
	if ((r.perms & req_perm) != req_perm) {
		*fault_code_out = 0xFFFEu;
		return false;
	}
	if (r.paddr >= RK65C02_PHYS_MAX) {
		*fault_code_out = 0xFFFFu;
		return false;
	}

	*paddr_out = r.paddr;
	if (e->mmu_tlb_enabled && !r.no_fill_tlb
	    && ((r.paddr & 0xFFu) == (vaddr & 0xFFu))) {
		te->valid = true;
		te->vpage = vpage;
		te->ppage_base = r.paddr & 0xFFFFFF00u;
		te->perms = r.perms;
		te->epoch = e->mmu_epoch;
	}
	return true;
}

static void rk65c02_maybe_call_on_stop(rk65c02emu_t *e);

/*
 * Stop execution with EMUERROR (shared by panic and MMU fault).
 * Caller must have set any fault-related state (e.g. mmu_last_fault_*) before this.
 */
static void
emu_stop_error(rk65c02emu_t *e)
{
	bool was_active;

	was_active = ((e->state == RUNNING) || (e->state == STEPPING));
	e->state = STOPPED;
	e->stopreason = EMUERROR;
	e->stop_requested = false;
	if (e->in_jit_run) {
		e->in_jit_run = false;
		longjmp(e->jit_fault_env, 1);
	}
	if (!was_active)
		rk65c02_maybe_call_on_stop(e);
}

static void
rk65c02_mmu_fault(rk65c02emu_t *e, uint16_t vaddr,
    rk65c02_mmu_access_t access, uint16_t fault_code)
{
	assert(e != NULL);

	e->mmu_last_fault_addr = vaddr;
	e->mmu_last_fault_access = access;
	e->mmu_last_fault_code = fault_code;
	e->mmu_fault_reexec = true;
	e->use_jit = false;
	if (e->mmu_fault != NULL)
		e->mmu_fault(e, vaddr, access, fault_code, e->mmu_fault_ctx);
	/* MMU fault is a normal stop condition when host uses demand paging; log as INFO. */
	rk65c02_log(LOG_INFO, "MMU fault at vaddr=%04x access=%u code=%u",
	    (unsigned)vaddr, (unsigned)access, (unsigned)fault_code);
	emu_stop_error(e);
}

static uint8_t
rk65c02_mem_read_mmu(rk65c02emu_t *e, uint16_t addr, rk65c02_mmu_access_t access)
{
	uint8_t req_perm;
	uint32_t paddr;
	uint16_t fault_code = 0;

	switch (access) {
	case RK65C02_MMU_FETCH:
		req_perm = RK65C02_MMU_PERM_X;
		break;
	case RK65C02_MMU_READ:
		req_perm = RK65C02_MMU_PERM_R;
		break;
	default:
		req_perm = RK65C02_MMU_PERM_R;
		break;
	}

	if (!rk65c02_mmu_translate_addr(e, addr, access, req_perm, &paddr,
	    &fault_code)) {
		rk65c02_mmu_fault(e, addr, access, fault_code);
		return 0xFF;
	}
	if (paddr < RK65C02_BUS_SIZE)
		return bus_read_1(e->bus, (uint16_t)paddr);
	return bus_read_1_phys(e->bus, paddr);
}

static void
rk65c02_mem_write_mmu(rk65c02emu_t *e, uint16_t addr, uint8_t val,
    rk65c02_mmu_access_t access)
{
	uint32_t paddr;
	uint16_t fault_code = 0;

	(void)access;
	if (!rk65c02_mmu_translate_addr(e, addr, RK65C02_MMU_WRITE,
	    RK65C02_MMU_PERM_W, &paddr, &fault_code)) {
		rk65c02_mmu_fault(e, addr, RK65C02_MMU_WRITE, fault_code);
		return;
	}
	if (paddr < RK65C02_BUS_SIZE)
		bus_write_1(e->bus, (uint16_t)paddr, val);
	else
		bus_write_1_phys(e->bus, paddr, val);
}

static void
rk65c02_mmu_refresh_accessors(rk65c02emu_t *e)
{
	bool mmu_active;

	assert(e != NULL);
	mmu_active = e->mmu_enabled && !(e->mmu_identity_active);
	if (mmu_active) {
		e->mem_read_1_fn = rk65c02_mem_read_mmu;
		e->mem_write_1_fn = rk65c02_mem_write_mmu;
	} else {
		e->mem_read_1_fn = rk65c02_mem_read_direct;
		e->mem_write_1_fn = rk65c02_mem_write_direct;
	}
}

void
rk65c02_mmu_tlb_flush(rk65c02emu_t *e)
{
	assert(e != NULL);
	memset(e->mmu_tlb, 0, sizeof(e->mmu_tlb));
}

void
rk65c02_mmu_tlb_flush_vpage(rk65c02emu_t *e, uint8_t vpage)
{
	assert(e != NULL);
	e->mmu_tlb[vpage].valid = false;
}

void
rk65c02_mmu_tlb_set(rk65c02emu_t *e, bool enabled)
{
	assert(e != NULL);
	e->mmu_tlb_enabled = enabled;
	if (!enabled)
		rk65c02_mmu_tlb_flush(e);
}

static void
rk65c02_maybe_call_on_stop(rk65c02emu_t *e)
{
	if (e->on_stop != NULL)
		e->on_stop(e, e->stopreason, e->on_stop_ctx);
}

static void
rk65c02_apply_host_stop_request(rk65c02emu_t *e)
{
	if (!(e->stop_requested))
		return;

	e->state = STOPPED;
	e->stopreason = HOST;
	e->stop_requested = false;
}

static void
rk65c02_maybe_tick(rk65c02emu_t *e)
{
	if (e->tick == NULL)
		return;

	if ((e->state != RUNNING) && (e->state != STEPPING))
		return;

	if (e->tick_interval == 0) {
		e->tick(e, e->tick_ctx);
		return;
	}

	if (e->tick_countdown > 0)
		e->tick_countdown--;

	if (e->tick_countdown == 0) {
		e->tick(e, e->tick_ctx);
		e->tick_countdown = e->tick_interval;
	}
}

static bool
rk65c02_can_use_jit(const rk65c02emu_t *e)
{
	return e->use_jit && e->jit != NULL && !(e->trace)
	    && !(e->runtime_disassembly) && (e->bps_head == NULL);
}

void
rk65c02_poll_host_controls(rk65c02emu_t *e)
{
	assert(e != NULL);

	rk65c02_apply_host_stop_request(e);
	rk65c02_maybe_tick(e);
	rk65c02_apply_host_stop_request(e);
}

bool
rk65c02_maybe_wait_on_idle(rk65c02emu_t *e)
{
	assert(e != NULL);

	if (!(e->idle_wait_enabled) || (e->idle_wait == NULL))
		return false;
	if ((e->state != STOPPED) || (e->stopreason != WAI))
		return false;

	e->idle_wait(e, e->idle_wait_ctx);
	rk65c02_apply_host_stop_request(e);
	return true;
}

const char *
rk65c02_stop_reason_string(emu_stop_reason_t reason)
{
	switch (reason) {
	case STP:
		return "STP";
	case WAI:
		return "WAI";
	case BREAKPOINT:
		return "BREAKPOINT";
	case WATCHPOINT:
		return "WATCHPOINT";
	case STEPPED:
		return "STEPPED";
	case HOST:
		return "HOST";
	case EMUERROR:
		return "EMUERROR";
	default:
		return "UNKNOWN";
	}
}

rk65c02emu_t
rk65c02_load_rom(const char *path, uint16_t load_addr, bus_t *b)
{
	rk65c02emu_t e;

	if (b == NULL) {
		b = GC_MALLOC(sizeof(bus_t));
		assert(b != NULL);
		*b = bus_init_with_default_devs();
	}

	/* Library-style error propagation can replace this assert in a later API pass. */
	assert(bus_load_file(b, load_addr, path));

	e = rk65c02_init(b);

	return e;
}

rk65c02emu_t
rk65c02_init(bus_t *b)
{
	rk65c02emu_t e;

	e.bus = b;
	e.state = STOPPED;
	e.stopreason = HOST;
	e.regs.P = P_UNDEFINED|P_IRQ_DISABLE;
	/* reset also clears the decimal flag */
	e.regs.P &= ~P_DECIMAL;

	e.irq = false;

	e.bps_head = NULL;
	e.trace = false;
	e.trace_head = NULL;
	e.runtime_disassembly = false;

	e.use_jit = false;
	e.jit_requested = false;
	e.jit = NULL;
	e.in_jit_run = false;
	e.stop_requested = false;
	e.on_stop = NULL;
	e.on_stop_ctx = NULL;
	e.tick = NULL;
	e.tick_ctx = NULL;
	e.tick_interval = 0;
	e.tick_countdown = 0;
	e.idle_wait_enabled = false;
	e.idle_wait = NULL;
	e.idle_wait_ctx = NULL;
	e.mmu_enabled = false;
	e.mmu_identity_fastpath = false;
	e.mmu_identity_active = false;
	e.mmu_update_pending = false;
	e.mmu_epoch = 0;
	e.mmu_translate = NULL;
	e.mmu_translate_ctx = NULL;
	e.mmu_fault = NULL;
	e.mmu_fault_ctx = NULL;
	e.mmu_last_fault_addr = 0;
	e.mmu_last_fault_access = RK65C02_MMU_READ;
	e.mmu_last_fault_code = 0;
	e.mmu_fault_reexec = false;
	e.mmu_tlb_enabled = true;
	e.mmu_changed_all = false;
	memset(e.mmu_changed_vpage, 0, sizeof(e.mmu_changed_vpage));
	e.mmu_tlb_hits = 0;
	e.mmu_tlb_misses = 0;
	memset(e.mmu_tlb, 0, sizeof(e.mmu_tlb));
	e.mem_read_1_fn = rk65c02_mem_read_direct;
	e.mem_write_1_fn = rk65c02_mem_write_direct;

	rk65c02_log(LOG_DEBUG, "Initialized new emulator.");

	return e;
}

void
rk65c02_assert_irq(rk65c02emu_t *e)
{
	/*
	 * This is a level-triggered aggregate line model: any source can assert IRQ
	 * and the core observes it as a single boolean state.
	 */
	e->irq = true;

	/*
	 * If the CPU was put to sleep by executing WAI instruction, resume
	 * operation.
	 *
	 * Whether interrupt will immediately be serviced, or not, depends
	 * on normal "interrupt disable" flag behaviour, so here we just
	 * need to start the CPU.
	 */
	if ((e->state == STOPPED) && (e->stopreason == WAI))
		e->state = RUNNING;
}

void
rk65c02_deassert_irq(rk65c02emu_t *e)
{
	assert(e != NULL);
	e->irq = false;
}

void
rk65c02_irq(rk65c02emu_t *e)
{
	/* push return address to the stack */
	stack_push(e, e->regs.PC >> 8);
	stack_push(e, e->regs.PC & 0xFF);
	/* push processor status to the stack with break flag set */
	stack_push(e, e->regs.P);

	/* 
	 * The IRQ disable is set, decimal flags is cleared _after_ pushing
	 * the P register to the stack.
	 */
	e->regs.P |= P_IRQ_DISABLE;
	e->regs.P &= ~P_DECIMAL;

	/* break flag is only saved to the stack, it is not present in the ISR... */
	e->regs.P &= ~P_BREAK;

	/* load address from IRQ vector into program counter */
	e->regs.PC = rk65c02_mem_read_1(e, VECTOR_IRQ);
	e->regs.PC |= rk65c02_mem_read_1(e, VECTOR_IRQ + 1) << 8;
	/* clear IRQ so we run the handler; device must re-assert if needed */
	e->irq = false;
}

/*
 * Execute a single instruction within emulator.
 */
void
rk65c02_exec(rk65c02emu_t *e)
{
	instruction_t i;
	instrdef_t id;
	uint16_t tpc;	/* saved PC for tracing */

	tpc = e->regs.PC;

	if (e->irq && (!(e->regs.P & P_IRQ_DISABLE)))
		rk65c02_irq(e);

	/* Watchpoint support is not implemented yet. */
	if (debug_PC_is_breakpoint(e)) {
		e->state = STOPPED;
		e->stopreason = BREAKPOINT;
		return;
	}

	if (e->runtime_disassembly)
		disassemble(e->bus, e->regs.PC);

	i = instruction_fetch_emu(e, e->regs.PC);
	id = instruction_decode(i.opcode);

	assert(id.emul);

	id.emul(e, &id, &i);

	if (e->state == STOPPED && e->mmu_fault_reexec) {
		e->mmu_fault_reexec = false;
	} else if (!instruction_modify_pc(&id)) {
		program_counter_increment(e, &id);
	}

	if (e->trace)
		debug_trace_savestate(e, tpc, &id, &i);

}

void
rk65c02_start(rk65c02emu_t *e) {

	assert(e != NULL);

	/*
	 * Prefer JIT execution path when enabled, JIT state is allocated,
	 * and no debugging features that rely on per-instruction interpreter
	 * state are active. Otherwise fall back to the interpreter loop.
	 */
	e->use_jit = e->jit_requested;
	e->stop_requested = false;
	e->mmu_fault_reexec = false;
	e->tick_countdown = e->tick_interval;
	rk65c02_mmu_refresh_accessors(e);

	if (rk65c02_can_use_jit(e))
		rk65c02_run_jit(e);
	else {
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
	}

	rk65c02_poll_host_controls(e);
	if (e->state == STOPPED)
		rk65c02_maybe_call_on_stop(e);
}

void
rk65c02_step(rk65c02emu_t *e, uint16_t steps) {

	uint16_t i = 0;

	assert(e != NULL);

	e->stop_requested = false;
	e->tick_countdown = e->tick_interval;
	rk65c02_mmu_refresh_accessors(e);
	e->state = STEPPING;
	while ((e->state == STEPPING) && (i < steps)) {
		rk65c02_poll_host_controls(e);
		if (e->state != STEPPING)
			break;

		rk65c02_exec(e);
		rk65c02_poll_host_controls(e);
		i++;
	}

	rk65c02_poll_host_controls(e);
	if (e->state == STEPPING) {
		e->state = STOPPED;
		e->stopreason = STEPPED;
	}

	if (e->state == STOPPED)
		rk65c02_maybe_call_on_stop(e);
}

void
rk65c02_on_stop_set(rk65c02emu_t *e, rk65c02_on_stop_cb_t cb, void *ctx)
{
	assert(e != NULL);

	e->on_stop = cb;
	e->on_stop_ctx = ctx;
}

void
rk65c02_on_stop_clear(rk65c02emu_t *e)
{
	assert(e != NULL);

	e->on_stop = NULL;
	e->on_stop_ctx = NULL;
}

void
rk65c02_tick_set(rk65c02emu_t *e, rk65c02_tick_cb_t cb, uint32_t interval,
    void *ctx)
{
	assert(e != NULL);

	e->tick = cb;
	e->tick_ctx = ctx;
	e->tick_interval = interval;
	e->tick_countdown = interval;
}

void
rk65c02_tick_clear(rk65c02emu_t *e)
{
	assert(e != NULL);

	e->tick = NULL;
	e->tick_ctx = NULL;
	e->tick_interval = 0;
	e->tick_countdown = 0;
}

void
rk65c02_idle_wait_set(rk65c02emu_t *e, rk65c02_wait_cb_t cb, void *ctx)
{
	assert(e != NULL);

	e->idle_wait = cb;
	e->idle_wait_ctx = ctx;
	e->idle_wait_enabled = (cb != NULL);
}

void
rk65c02_idle_wait_clear(rk65c02emu_t *e)
{
	assert(e != NULL);

	e->idle_wait_enabled = false;
	e->idle_wait = NULL;
	e->idle_wait_ctx = NULL;
}

bool
rk65c02_mmu_set(rk65c02emu_t *e, rk65c02_mmu_translate_cb_t translate,
    void *translate_ctx, rk65c02_mmu_fault_cb_t on_fault, void *fault_ctx,
    bool enabled, bool identity_fastpath)
{
	assert(e != NULL);

	if (enabled && (translate == NULL))
		return false;

	e->mmu_translate = translate;
	e->mmu_translate_ctx = translate_ctx;
	e->mmu_fault = on_fault;
	e->mmu_fault_ctx = fault_ctx;
	e->mmu_enabled = enabled;
	e->mmu_identity_fastpath = identity_fastpath;
	e->mmu_identity_active = enabled ? identity_fastpath : false;
	e->mmu_update_pending = false;
	e->mmu_changed_all = false;
	memset(e->mmu_changed_vpage, 0, sizeof(e->mmu_changed_vpage));
	e->mmu_epoch++;

	rk65c02_mmu_refresh_accessors(e);
	rk65c02_mmu_tlb_flush(e);
	if (e->jit != NULL)
		rk65c02_jit_flush(e);

	return true;
}

void
rk65c02_mmu_clear(rk65c02emu_t *e)
{
	assert(e != NULL);

	e->mmu_enabled = false;
	e->mmu_identity_fastpath = false;
	e->mmu_identity_active = false;
	e->mmu_update_pending = false;
	e->mmu_translate = NULL;
	e->mmu_translate_ctx = NULL;
	e->mmu_fault = NULL;
	e->mmu_fault_ctx = NULL;
	e->mmu_epoch++;
	e->mmu_changed_all = false;
	memset(e->mmu_changed_vpage, 0, sizeof(e->mmu_changed_vpage));
	rk65c02_mmu_refresh_accessors(e);
	rk65c02_mmu_tlb_flush(e);
}

void
rk65c02_mmu_begin_update(rk65c02emu_t *e)
{
	assert(e != NULL);

	if (!(e->mmu_enabled))
		return;
	e->mmu_update_pending = true;
	e->mmu_changed_all = false;
	memset(e->mmu_changed_vpage, 0, sizeof(e->mmu_changed_vpage));
}

void
rk65c02_mmu_mark_changed_vpage(rk65c02emu_t *e, uint8_t vpage)
{
	assert(e != NULL);
	(void)vpage;

	if (!(e->mmu_enabled))
		return;
	e->mmu_update_pending = true;
	e->mmu_changed_vpage[vpage] = true;
	e->mmu_identity_active = false;
	rk65c02_mmu_refresh_accessors(e);
}

void
rk65c02_mmu_mark_changed_vrange(rk65c02emu_t *e, uint16_t start, uint16_t end)
{
	assert(e != NULL);
	(void)start;
	(void)end;

	if (!(e->mmu_enabled))
		return;
	e->mmu_update_pending = true;
	if (start <= end) {
		uint8_t sp = (uint8_t)(start >> 8);
		uint8_t ep = (uint8_t)(end >> 8);
		uint16_t p;
		for (p = sp; p <= ep; p++)
			e->mmu_changed_vpage[(uint8_t)p] = true;
	} else {
		e->mmu_changed_all = true;
	}
	e->mmu_identity_active = false;
	rk65c02_mmu_refresh_accessors(e);
}

void
rk65c02_mmu_end_update(rk65c02emu_t *e)
{
	assert(e != NULL);

	if (!(e->mmu_enabled))
		return;
	if (!(e->mmu_update_pending))
		return;

	e->mmu_update_pending = false;
	if (e->mmu_changed_all) {
		e->mmu_epoch++;
		rk65c02_mmu_tlb_flush(e);
		if (e->jit != NULL)
			rk65c02_jit_invalidate_all(e);
	} else {
		uint16_t p;
		for (p = 0; p < 256; p++) {
			if (!(e->mmu_changed_vpage[p]))
				continue;
			rk65c02_mmu_tlb_flush_vpage(e, (uint8_t)p);
			/* Only invalidate JIT blocks whose code lies on this page;
			 * data accesses go through accessors and see new mapping. */
			if (e->jit != NULL)
				rk65c02_jit_invalidate_code_vpage(e, (uint8_t)p);
		}
	}
	e->mmu_changed_all = false;
	memset(e->mmu_changed_vpage, 0, sizeof(e->mmu_changed_vpage));
	rk65c02_mmu_refresh_accessors(e);
}

void
rk65c02_request_stop(rk65c02emu_t *e)
{
	assert(e != NULL);

	e->stop_requested = true;
}

uint8_t
rk65c02_mem_fetch_1(rk65c02emu_t *e, uint16_t addr)
{
	assert(e != NULL);
	assert(e->mem_read_1_fn != NULL);
	return e->mem_read_1_fn(e, addr, RK65C02_MMU_FETCH);
}

uint8_t
rk65c02_mem_read_1(rk65c02emu_t *e, uint16_t addr)
{
	assert(e != NULL);
	assert(e->mem_read_1_fn != NULL);
	return e->mem_read_1_fn(e, addr, RK65C02_MMU_READ);
}

void
rk65c02_mem_write_1(rk65c02emu_t *e, uint16_t addr, uint8_t val)
{
	assert(e != NULL);
	assert(e->mem_write_1_fn != NULL);
	e->mem_write_1_fn(e, addr, val, RK65C02_MMU_WRITE);
}

void
rk65c02_dump_stack(rk65c02emu_t *e, uint8_t n)
{
	uint16_t stackaddr;

	stackaddr = STACK_END-n;

	while (stackaddr <= STACK_END) {

		if ((stackaddr == STACK_END-n) || !((stackaddr % 0x10)))
			printf("stack %#02x: ", stackaddr);

		printf("%#02x ", bus_read_1(e->bus, stackaddr));

		stackaddr++;

		if (!(stackaddr % 0x10))
			printf("\n");
	}
}

void
rk65c02_dump_regs(reg_state_t regs)
{
	char *str;

	str = rk65c02_regs_string_get(regs);

	printf ("%s\n", str);

}

char *
rk65c02_regs_string_get(reg_state_t regs)
{
#define REGS_STR_LEN 50
	char *str;

	/* Keep allocation local until shared string helpers are introduced. */
	str = GC_MALLOC(REGS_STR_LEN);
	assert(str != NULL);
	memset(str, 0, REGS_STR_LEN);

	snprintf(str, REGS_STR_LEN, "A: %X X: %X Y: %X PC: %X SP: %X P: %c%c%c%c%c%c%c%c", 
	    regs.A, regs.X, regs.Y, regs.PC, regs.SP, 
	    (regs.P & P_NEGATIVE) ? 'N' : '-',
	    (regs.P & P_SIGN_OVERFLOW) ? 'V' : '-',
	    (regs.P & P_UNDEFINED) ? '1' : '-',
	    (regs.P & P_BREAK) ? 'B' : '-',
	    (regs.P & P_DECIMAL) ? 'D' : '-',
	    (regs.P & P_IRQ_DISABLE) ? 'I' : '-',
	    (regs.P & P_ZERO) ? 'Z' : '-',
	    (regs.P & P_CARRY) ? 'C' : '-');

	return str;
}

void
rk65c02_panic(rk65c02emu_t *e, const char* fmt, ...)
{
	va_list args;

	assert(e != NULL);

	va_start(args, fmt);
	rk65c02_logv(LOG_CRIT, fmt, args);
	va_end(args);

	emu_stop_error(e);
}

