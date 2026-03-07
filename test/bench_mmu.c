#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "bus.h"
#include "rk65c02.h"

#define STEPS 2000000u
#define STEP_CHUNK 50000u

struct mmu_bench_state {
	uint16_t page_base[256];
	uint64_t translate_calls;
	bool remap_heavy;
	uint32_t remap_counter;
};

static uint64_t
mono_ns_now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static rk65c02_mmu_result_t
bench_translate(rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access,
    void *ctx)
{
	struct mmu_bench_state *s = (struct mmu_bench_state *)ctx;
	rk65c02_mmu_result_t r;
	uint16_t base;
	(void)e;
	(void)access;

	s->translate_calls++;
	base = s->page_base[vaddr >> 8];
	r.ok = true;
	r.paddr = (uint16_t)(base + (vaddr & 0x00FFu));
	r.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X;
	r.fault_code = 0;
	return r;
}

static void
bench_tick(rk65c02emu_t *e, void *ctx)
{
	struct mmu_bench_state *s = (struct mmu_bench_state *)ctx;

	if (!(s->remap_heavy))
		return;
	s->remap_counter++;
	/* With tick_interval=1024 we are invoked only when it's time to remap. */
	rk65c02_mmu_begin_update(e);
	if (s->page_base[0x00] == 0x0000)
		s->page_base[0x00] = 0x0100;
	else
		s->page_base[0x00] = 0x0000;
	rk65c02_mmu_mark_changed_vpage(e, 0x00);
	rk65c02_mmu_end_update(e);
}

static void
setup_loop_program(bus_t *b)
{
	/* LDA $10 ; BRA -4 */
	bus_write_1(b, 0xC000, 0xA5);
	bus_write_1(b, 0xC001, 0x10);
	bus_write_1(b, 0xC002, 0x80);
	bus_write_1(b, 0xC003, 0xFC);
}

static void
run_case(const char *name, bool use_jit, bool mmu_enabled, bool tlb_enabled,
    bool remap_heavy)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_bench_state s;
	uint16_t i;
	uint64_t t0, t1;
	double nspi;

	memset(&s, 0, sizeof(s));
	for (i = 0; i < 256; i++)
		s.page_base[i] = (uint16_t)(i << 8);
	s.remap_heavy = remap_heavy;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	setup_loop_program(&b);
	bus_write_1(&b, 0x0010, 0x33);
	bus_write_1(&b, 0x0110, 0x77);
	e.regs.PC = 0xC000;

	if (mmu_enabled) {
		(void)rk65c02_mmu_set(&e, bench_translate, &s, NULL, NULL, true, false);
		rk65c02_mmu_tlb_set(&e, tlb_enabled);
	}
	/* Remap-heavy: fire tick only when we remap (every 1024 steps) to measure
	 * remap+JIT cost, not per-dispatcher callback overhead. */
	rk65c02_tick_set(&e, bench_tick, remap_heavy ? 1024u : 1u, &s);

	t0 = mono_ns_now();
	for (i = 0; i < (STEPS / STEP_CHUNK); i++)
		rk65c02_step(&e, STEP_CHUNK);
	t1 = mono_ns_now();

	nspi = (double)(t1 - t0) / (double)STEPS;
	printf("%-20s mode=%s mmu=%s tlb=%s remap=%s ns/insn=%.2f translate=%" PRIu64
	       " tlb_hits=%" PRIu64 " tlb_misses=%" PRIu64 "\n",
	    name,
	    use_jit ? "jit" : "interp",
	    mmu_enabled ? "on" : "off",
	    tlb_enabled ? "on" : "off",
	    remap_heavy ? "heavy" : "none",
	    nspi,
	    (uint64_t)s.translate_calls,
	    (uint64_t)e.mmu_tlb_hits,
	    (uint64_t)e.mmu_tlb_misses);

	bus_finish(&b);
}

int
main(void)
{
	printf("MMU benchmark (%u steps)\n", STEPS);
	run_case("baseline_interp", false, false, false, false);
	run_case("baseline_jit", true, false, false, false);
	run_case("mmu_tlb_on_interp", false, true, true, false);
	run_case("mmu_tlb_on_jit", true, true, true, false);
	run_case("mmu_tlb_off_interp", false, true, false, false);
	run_case("mmu_tlb_off_jit", true, true, false, false);
	run_case("mmu_remap_interp", false, true, true, true);
	run_case("mmu_remap_jit", true, true, true, true);
	return 0;
}
