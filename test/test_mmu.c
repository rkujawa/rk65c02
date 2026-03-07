#include <atf-c.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bus.h"
#include "device_ram.h"
#include "rk65c02.h"
#include "utils.h"

struct mmu_test_state {
	unsigned translate_calls;
	unsigned translate_calls_fetch;
	unsigned translate_calls_read;
	unsigned translate_calls_write;
	unsigned fault_calls;
	uint16_t last_fault_addr;
	rk65c02_mmu_access_t last_fault_access;
	uint16_t last_fault_code;
	uint16_t page_base[256];
	uint32_t rng;
};

static rk65c02_mmu_result_t
mmu_identity_translate(rk65c02emu_t *e, uint16_t vaddr,
    rk65c02_mmu_access_t access, void *ctx)
{
	struct mmu_test_state *s;
	rk65c02_mmu_result_t r;
	(void)e;

	s = (struct mmu_test_state *)ctx;
	s->translate_calls++;
	if (access == RK65C02_MMU_FETCH)
		s->translate_calls_fetch++;
	else if (access == RK65C02_MMU_READ)
		s->translate_calls_read++;
	else if (access == RK65C02_MMU_WRITE)
		s->translate_calls_write++;
	r.ok = true;
	r.paddr = vaddr;
	r.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X;
	r.fault_code = 0;
	return r;
}

static rk65c02_mmu_result_t
mmu_deny_write_page10_translate(rk65c02emu_t *e, uint16_t vaddr,
    rk65c02_mmu_access_t access, void *ctx)
{
	struct mmu_test_state *s;
	rk65c02_mmu_result_t r;
	(void)e;

	s = (struct mmu_test_state *)ctx;
	s->translate_calls++;
	if (access == RK65C02_MMU_FETCH)
		s->translate_calls_fetch++;
	else if (access == RK65C02_MMU_READ)
		s->translate_calls_read++;
	else if (access == RK65C02_MMU_WRITE)
		s->translate_calls_write++;
	r.ok = true;
	r.paddr = vaddr;
	r.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X;
	r.fault_code = 0;
	if ((access == RK65C02_MMU_WRITE) && (vaddr == 0x0010)) {
		r.ok = false;
		r.fault_code = 0x20;
	}
	return r;
}

static rk65c02_mmu_result_t
mmu_page_translate(rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access,
    void *ctx)
{
	struct mmu_test_state *s;
	rk65c02_mmu_result_t r;
	uint16_t base;
	(void)e;

	s = (struct mmu_test_state *)ctx;
	base = s->page_base[vaddr >> 8];
	s->translate_calls++;
	if (access == RK65C02_MMU_FETCH)
		s->translate_calls_fetch++;
	else if (access == RK65C02_MMU_READ)
		s->translate_calls_read++;
	else if (access == RK65C02_MMU_WRITE)
		s->translate_calls_write++;
	r.ok = true;
	r.paddr = (uint16_t)(base + (vaddr & 0x00FFu));
	r.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X;
	r.fault_code = 0;
	return r;
}

static rk65c02_mmu_result_t
mmu_deny_exec_translate(rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access,
    void *ctx)
{
	struct mmu_test_state *s;
	rk65c02_mmu_result_t r;
	(void)e;

	s = (struct mmu_test_state *)ctx;
	s->translate_calls++;
	if (access == RK65C02_MMU_FETCH)
		s->translate_calls_fetch++;
	else if (access == RK65C02_MMU_READ)
		s->translate_calls_read++;
	else if (access == RK65C02_MMU_WRITE)
		s->translate_calls_write++;

	r.ok = true;
	r.paddr = vaddr;
	r.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X;
	r.fault_code = 0;
	if ((access == RK65C02_MMU_FETCH) && ((vaddr >> 8) == (ROM_LOAD_ADDR >> 8))) {
		r.ok = false;
		r.fault_code = 0x40;
	}
	return r;
}

static rk65c02_mmu_result_t
mmu_no_write_perm_translate(rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access,
    void *ctx)
{
	struct mmu_test_state *s;
	rk65c02_mmu_result_t r;
	(void)e;

	s = (struct mmu_test_state *)ctx;
	s->translate_calls++;
	if (access == RK65C02_MMU_FETCH)
		s->translate_calls_fetch++;
	else if (access == RK65C02_MMU_READ)
		s->translate_calls_read++;
	else if (access == RK65C02_MMU_WRITE)
		s->translate_calls_write++;

	r.ok = true;
	r.paddr = vaddr;
	r.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_X;
	r.fault_code = 0;
	return r;
}

static rk65c02_mmu_result_t
mmu_out_of_range_translate(rk65c02emu_t *e, uint16_t vaddr,
    rk65c02_mmu_access_t access, void *ctx)
{
	struct mmu_test_state *s;
	rk65c02_mmu_result_t r;
	(void)e;

	s = (struct mmu_test_state *)ctx;
	s->translate_calls++;
	if (access == RK65C02_MMU_FETCH)
		s->translate_calls_fetch++;
	else if (access == RK65C02_MMU_READ)
		s->translate_calls_read++;
	else if (access == RK65C02_MMU_WRITE)
		s->translate_calls_write++;

	r.ok = true;
	/* Physical addresses >= RK65C02_PHYS_MAX are out of range and fault with 0xFFFF. */
	r.paddr = (uint32_t)RK65C02_PHYS_MAX;
	r.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X;
	r.fault_code = 0;
	if (access == RK65C02_MMU_FETCH)
		r.paddr = vaddr;
	return r;
}

/* Demand-paging translate: page 1 not present until ctx->page_1_present. */
struct demand_page_state {
	bool page_1_present;
};
static rk65c02_mmu_result_t
mmu_demand_page_translate(rk65c02emu_t *e, uint16_t vaddr,
    rk65c02_mmu_access_t access, void *ctx)
{
	struct demand_page_state *ds = (struct demand_page_state *)ctx;
	rk65c02_mmu_result_t r;

	(void)e;
	r.ok = true;
	r.paddr = vaddr;
	r.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X;
	r.fault_code = 0;
	if ((vaddr >> 8) == 0x01 && !ds->page_1_present) {
		r.ok = false;
		r.fault_code = 1;
	}
	return r;
}

/* Map guest page 0 to extended physical 0x10000-0x100FF; rest identity. */
static rk65c02_mmu_result_t
mmu_extended_phys_translate(rk65c02emu_t *e, uint16_t vaddr,
    rk65c02_mmu_access_t access, void *ctx)
{
	rk65c02_mmu_result_t r;
	(void)e;
	(void)ctx;

	r.ok = true;
	r.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X;
	r.fault_code = 0;
	if ((vaddr >> 8) == 0x00)
		r.paddr = 0x10000u + (vaddr & 0xFFu);
	else
		r.paddr = vaddr;
	return r;
}

static rk65c02_mmu_result_t
mmu_offset_mismatch_translate(rk65c02emu_t *e, uint16_t vaddr,
    rk65c02_mmu_access_t access, void *ctx)
{
	struct mmu_test_state *s;
	rk65c02_mmu_result_t r;
	(void)e;

	s = (struct mmu_test_state *)ctx;
	s->translate_calls++;
	if (access == RK65C02_MMU_FETCH)
		s->translate_calls_fetch++;
	else if (access == RK65C02_MMU_READ)
		s->translate_calls_read++;
	else if (access == RK65C02_MMU_WRITE)
		s->translate_calls_write++;

	r.ok = true;
	r.paddr = vaddr;
	if ((access == RK65C02_MMU_READ) && ((vaddr >> 8) == 0x00))
		r.paddr = (uint16_t)(vaddr ^ 0x0001u);
	r.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X;
	r.fault_code = 0;
	return r;
}

static uint32_t
mmu_rand_next(uint32_t *st)
{
	*st = *st * 1664525u + 1013904223u;
	return *st;
}

static void
mmu_record_fault(rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access,
    uint16_t fault_code, void *ctx)
{
	struct mmu_test_state *s;
	(void)e;

	s = (struct mmu_test_state *)ctx;
	s->fault_calls++;
	s->last_fault_addr = vaddr;
	s->last_fault_access = access;
	s->last_fault_code = fault_code;
}

static void
setup_rom_lda_zp10_stp(bus_t *b)
{
	bus_write_1(b, ROM_LOAD_ADDR + 0, 0xA5); /* LDA $10 */
	bus_write_1(b, ROM_LOAD_ADDR + 1, 0x10);
	bus_write_1(b, ROM_LOAD_ADDR + 2, 0xDB); /* STP */
}

static void
setup_rom_lda_imm_sta_zp10_stp(bus_t *b, uint8_t imm)
{
	bus_write_1(b, ROM_LOAD_ADDR + 0, 0xA9); /* LDA #imm */
	bus_write_1(b, ROM_LOAD_ADDR + 1, imm);
	bus_write_1(b, ROM_LOAD_ADDR + 2, 0x85); /* STA $10 */
	bus_write_1(b, ROM_LOAD_ADDR + 3, 0x10);
	bus_write_1(b, ROM_LOAD_ADDR + 4, 0xDB); /* STP */
}

static void
do_mmu_set_requires_translate(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	(void)tc;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_CHECK(!rk65c02_mmu_set(&e, NULL, NULL, NULL, NULL, true, false));
	bus_write_1(&b, ROM_LOAD_ADDR, 0xDB);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.stopreason == STP);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_set_requires_translate, do_mmu_set_requires_translate)

static void
do_mmu_identity_fastpath(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_test_state s;
	(void)tc;

	memset(&s, 0, sizeof(s));
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_identity_translate, &s,
	    mmu_record_fault, &s, true, true));
	setup_rom_lda_imm_sta_zp10_stp(&b, 0x2A);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.stopreason == STP);
	ATF_CHECK(bus_read_1(&b, 0x10) == 0x2A);
	ATF_CHECK(s.translate_calls == 0);
	ATF_CHECK(s.fault_calls == 0);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_identity_fastpath, do_mmu_identity_fastpath)

static void
do_mmu_fault_write(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_test_state s;
	(void)tc;

	memset(&s, 0, sizeof(s));
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_deny_write_page10_translate, &s,
	    mmu_record_fault, &s, true, false));
	setup_rom_lda_imm_sta_zp10_stp(&b, 0x01);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.stopreason == EMUERROR);
	ATF_CHECK(s.translate_calls > 0);
	ATF_CHECK(s.fault_calls == 1);
	ATF_CHECK(s.last_fault_addr == 0x0010);
	ATF_CHECK(s.last_fault_access == RK65C02_MMU_WRITE);
	ATF_CHECK(s.last_fault_code == 0x20);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_fault_write, do_mmu_fault_write)

static void
do_mmu_exec_fault(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_test_state s;
	(void)tc;

	memset(&s, 0, sizeof(s));
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_deny_exec_translate, &s,
	    mmu_record_fault, &s, true, false));
	bus_write_1(&b, ROM_LOAD_ADDR, 0xDB);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.stopreason == EMUERROR);
	ATF_CHECK(s.fault_calls >= 1);
	ATF_CHECK(s.last_fault_access == RK65C02_MMU_FETCH);
	ATF_CHECK((s.last_fault_addr >> 8) == (ROM_LOAD_ADDR >> 8));
	ATF_CHECK(s.last_fault_code == 0x40);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_exec_fault, do_mmu_exec_fault)

static void
do_mmu_write_perm_fault(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_test_state s;
	(void)tc;

	memset(&s, 0, sizeof(s));
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_no_write_perm_translate, &s,
	    mmu_record_fault, &s, true, false));
	setup_rom_lda_imm_sta_zp10_stp(&b, 0x77);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.stopreason == EMUERROR);
	ATF_CHECK(s.fault_calls == 1);
	ATF_CHECK(s.last_fault_access == RK65C02_MMU_WRITE);
	ATF_CHECK(s.last_fault_addr == 0x0010);
	ATF_CHECK(s.last_fault_code == 0xFFFE);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_write_perm_fault, do_mmu_write_perm_fault)

static void
do_mmu_out_of_range_fault(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_test_state s;
	(void)tc;

	memset(&s, 0, sizeof(s));
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_out_of_range_translate, &s,
	    mmu_record_fault, &s, true, false));
	setup_rom_lda_zp10_stp(&b);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(s.fault_calls == 1);
	ATF_CHECK(s.last_fault_access == RK65C02_MMU_READ);
	ATF_CHECK(s.last_fault_code == 0xFFFF);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_out_of_range_fault, do_mmu_out_of_range_fault)

/** Explicitly verify extended physical address range: guest page 0 -> phys 0x10000. */
static void
do_mmu_extended_phys(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	const uint32_t phys_base = 0x10000u;
	const uint16_t page_size = 256;

	(void)tc;
	b = bus_init_with_default_devs();
	bus_device_add_phys(&b, device_ram_init(page_size), phys_base);

	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_extended_phys_translate, NULL,
	    NULL, NULL, true, false));

	/* Seed extended phys so guest $10 reads 0x5A. */
	bus_write_1_phys(&b, phys_base + 0x10, 0x5A);
	/* Program: LDA $10, STA $20, STP (guest $10/$20 are in page 0 -> phys 0x10010/0x10020). */
	bus_write_1(&b, ROM_LOAD_ADDR + 0, 0xA5);
	bus_write_1(&b, ROM_LOAD_ADDR + 1, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 2, 0x85);
	bus_write_1(&b, ROM_LOAD_ADDR + 3, 0x20);
	bus_write_1(&b, ROM_LOAD_ADDR + 4, 0xDB);

	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);

	ATF_CHECK(e.stopreason == STP);
	ATF_CHECK(e.regs.A == 0x5A);
	ATF_CHECK(bus_read_1_phys(&b, phys_base + 0x20) == 0x5A);

	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_extended_phys, do_mmu_extended_phys)

/** Demand paging with JIT: fault on page 1, install, restart; PC must stay at faulting insn. */
static void
do_mmu_demand_page_jit(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct demand_page_state ds = { .page_1_present = false };
	unsigned runs;

	(void)tc;
	ATF_REQUIRE(use_jit);
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, true);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_demand_page_translate, &ds,
	    NULL, NULL, true, false));

	bus_write_1(&b, 0x0111, 0xAB);
	bus_write_1(&b, ROM_LOAD_ADDR + 0, 0xAD); /* LDA abs $0111 */
	bus_write_1(&b, ROM_LOAD_ADDR + 1, 0x11);
	bus_write_1(&b, ROM_LOAD_ADDR + 2, 0x01);
	bus_write_1(&b, ROM_LOAD_ADDR + 3, 0xDB); /* STP */

	e.regs.PC = ROM_LOAD_ADDR;
	runs = 0;
	for (;;) {
		rk65c02_start(&e);
		runs++;
		if (e.stopreason == STP)
			break;
		if (e.stopreason == EMUERROR && e.mmu_last_fault_code == 1) {
			ds.page_1_present = true;
			rk65c02_mmu_begin_update(&e);
			rk65c02_mmu_mark_changed_vpage(&e, 1);
			rk65c02_mmu_end_update(&e);
			ATF_REQUIRE(runs < 4);
			continue;
		}
		ATF_REQUIRE(e.stopreason == EMUERROR);
		ATF_REQUIRE(e.mmu_last_fault_code == 1);
	}
	ATF_CHECK(e.regs.A == 0xAB);
	ATF_CHECK(runs == 2);
	bus_finish(&b);
}
ATF_TC_WITHOUT_HEAD(mmu_demand_page_jit);
ATF_TC_BODY(mmu_demand_page_jit, tc) { do_mmu_demand_page_jit(tc, true); }

static void
do_mmu_tlb_hits(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_test_state s;
	uint16_t p;
	(void)tc;

	memset(&s, 0, sizeof(s));
	for (p = 0; p < 256; p++)
		s.page_base[p] = (uint16_t)(p << 8);
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_page_translate, &s, NULL, NULL, true, false));
	bus_write_1(&b, 0x0010, 0x11);
	bus_write_1(&b, ROM_LOAD_ADDR + 0, 0xA5);
	bus_write_1(&b, ROM_LOAD_ADDR + 1, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 2, 0xA5);
	bus_write_1(&b, ROM_LOAD_ADDR + 3, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 4, 0xA5);
	bus_write_1(&b, ROM_LOAD_ADDR + 5, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 6, 0xDB);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.stopreason == STP);
	ATF_CHECK(e.regs.A == 0x11);
	ATF_CHECK(s.translate_calls >= 2);
	ATF_CHECK(e.mmu_tlb_hits > 0);
	ATF_CHECK(e.mmu_tlb_misses > 0);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_tlb_hits, do_mmu_tlb_hits)

static void
do_mmu_tlb_update_invalidate(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_test_state s;
	uint16_t p;
	(void)tc;

	memset(&s, 0, sizeof(s));
	for (p = 0; p < 256; p++)
		s.page_base[p] = (uint16_t)(p << 8);
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_page_translate, &s, NULL, NULL, true, false));
	bus_write_1(&b, 0x0010, 0x33);
	bus_write_1(&b, 0x0110, 0x77);
	setup_rom_lda_zp10_stp(&b);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.regs.A == 0x33);
	rk65c02_mmu_begin_update(&e);
	s.page_base[0x00] = 0x0100;
	rk65c02_mmu_mark_changed_vpage(&e, 0x00);
	rk65c02_mmu_end_update(&e);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.stopreason == STP);
	ATF_CHECK(e.regs.A == 0x77);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_tlb_update_invalidate, do_mmu_tlb_update_invalidate)

static void
do_mmu_tlb_disabled(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_test_state s;
	uint16_t p;
	(void)tc;

	memset(&s, 0, sizeof(s));
	for (p = 0; p < 256; p++)
		s.page_base[p] = (uint16_t)(p << 8);
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_page_translate, &s, NULL, NULL, true, false));
	rk65c02_mmu_tlb_set(&e, false);
	bus_write_1(&b, 0x0010, 0x55);
	bus_write_1(&b, ROM_LOAD_ADDR + 0, 0xA5);
	bus_write_1(&b, ROM_LOAD_ADDR + 1, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 2, 0xA5);
	bus_write_1(&b, ROM_LOAD_ADDR + 3, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 4, 0xA5);
	bus_write_1(&b, ROM_LOAD_ADDR + 5, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 6, 0xDB);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.stopreason == STP);
	ATF_CHECK(e.regs.A == 0x55);
	ATF_CHECK(e.mmu_tlb_hits == 0);
	ATF_CHECK(e.mmu_tlb_misses > 0);
	ATF_CHECK(s.translate_calls >= 3);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_tlb_disabled, do_mmu_tlb_disabled)

static void
do_mmu_tlb_offset_mismatch_no_fill(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_test_state s;
	(void)tc;

	memset(&s, 0, sizeof(s));
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_offset_mismatch_translate, &s,
	    NULL, NULL, true, false));
	bus_write_1(&b, 0x0011, 0x66);
	bus_write_1(&b, ROM_LOAD_ADDR + 0, 0xA5);
	bus_write_1(&b, ROM_LOAD_ADDR + 1, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 2, 0xA5);
	bus_write_1(&b, ROM_LOAD_ADDR + 3, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 4, 0xA5);
	bus_write_1(&b, ROM_LOAD_ADDR + 5, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 6, 0xDB);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.stopreason == STP);
	ATF_CHECK(e.regs.A == 0x66);
	ATF_CHECK(e.mmu_tlb_misses >= 3);
	ATF_CHECK(s.translate_calls_read >= 3);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_tlb_offset_mismatch_no_fill,
    do_mmu_tlb_offset_mismatch_no_fill)

static void
do_mmu_tlb_flush_vpage(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_test_state s;
	uint16_t p;
	(void)tc;

	memset(&s, 0, sizeof(s));
	for (p = 0; p < 256; p++)
		s.page_base[p] = (uint16_t)(p << 8);
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_page_translate, &s, NULL, NULL, true, false));
	bus_write_1(&b, 0x0010, 0x42);
	bus_write_1(&b, ROM_LOAD_ADDR + 0, 0xA5);
	bus_write_1(&b, ROM_LOAD_ADDR + 1, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 2, 0xA5);
	bus_write_1(&b, ROM_LOAD_ADDR + 3, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 4, 0xDB);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.regs.A == 0x42);
	ATF_CHECK(e.mmu_tlb_hits > 0);
	e.mmu_tlb_hits = 0;
	e.mmu_tlb_misses = 0;
	s.translate_calls = 0;
	rk65c02_mmu_tlb_flush_vpage(&e, 0x00);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.regs.A == 0x42);
	ATF_CHECK(s.translate_calls > 0);
	ATF_CHECK(e.mmu_tlb_hits > 0);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_tlb_flush_vpage, do_mmu_tlb_flush_vpage)

static void
do_mmu_vrange_wrap_invalidate_all(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_test_state s;
	uint16_t p;
	(void)tc;

	memset(&s, 0, sizeof(s));
	for (p = 0; p < 256; p++)
		s.page_base[p] = (uint16_t)(p << 8);
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_page_translate, &s, NULL, NULL, true, false));
	bus_write_1(&b, 0x0110, 0x44);
	bus_write_1(&b, 0x0210, 0x88);
	bus_write_1(&b, ROM_LOAD_ADDR + 0, 0xAD);
	bus_write_1(&b, ROM_LOAD_ADDR + 1, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 2, 0x01);
	bus_write_1(&b, ROM_LOAD_ADDR + 3, 0xDB);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.regs.A == 0x44);
	rk65c02_mmu_begin_update(&e);
	s.page_base[0x01] = 0x0200;
	rk65c02_mmu_mark_changed_vrange(&e, 0xFF00, 0x00FF);
	rk65c02_mmu_end_update(&e);
	e.regs.PC = ROM_LOAD_ADDR;
	rk65c02_start(&e);
	ATF_CHECK(e.regs.A == 0x88);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_vrange_wrap_invalidate_all, do_mmu_vrange_wrap_invalidate_all)

static void
do_mmu_remap_stress(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_test_state s;
	uint16_t p;
	unsigned i;
	(void)tc;

	memset(&s, 0, sizeof(s));
	for (p = 0; p < 256; p++)
		s.page_base[p] = (uint16_t)(p << 8);
	s.rng = 0x12345678u;
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_page_translate, &s, NULL, NULL, true, false));
	bus_write_1(&b, 0x0010, 0x11);
	bus_write_1(&b, 0x0110, 0x22);
	setup_rom_lda_zp10_stp(&b);

	for (i = 0; i < 200; i++) {
		rk65c02_mmu_begin_update(&e);
		if (mmu_rand_next(&s.rng) & 1u)
			s.page_base[0x00] = 0x0000;
		else
			s.page_base[0x00] = 0x0100;
		rk65c02_mmu_mark_changed_vpage(&e, 0x00);
		rk65c02_mmu_end_update(&e);
		e.regs.PC = ROM_LOAD_ADDR;
		rk65c02_step(&e, 2);
		ATF_CHECK((e.regs.A == 0x11) || (e.regs.A == 0x22)
		    || (e.stopreason == EMUERROR));
	}
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_remap_stress, do_mmu_remap_stress)

static void
do_mmu_remap_fuzz(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	struct mmu_test_state s;
	uint16_t p;
	unsigned i;
	(void)tc;

	memset(&s, 0, sizeof(s));
	for (p = 0; p < 256; p++)
		s.page_base[p] = (uint16_t)(p << 8);
	s.rng = 0xCAFEBABEu;
	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	ATF_REQUIRE(rk65c02_mmu_set(&e, mmu_page_translate, &s, NULL, NULL, true, false));
	bus_write_1(&b, 0x0010, 0x3C);
	bus_write_1(&b, 0x0110, 0xC3);
	setup_rom_lda_zp10_stp(&b);

	for (i = 0; i < 200; i++) {
		uint8_t vp;
		uint8_t phys;
		uint32_t rnd;

		rnd = mmu_rand_next(&s.rng);
		vp = (uint8_t)(rnd >> 24);
		phys = (uint8_t)(rnd >> 16);
		rk65c02_mmu_begin_update(&e);
		s.page_base[vp] = (uint16_t)(phys << 8);
		if ((rnd & 0x3u) == 0u)
			rk65c02_mmu_mark_changed_vrange(&e, 0xFF00, 0x00FF);
		else
			rk65c02_mmu_mark_changed_vpage(&e, vp);
		rk65c02_mmu_end_update(&e);
		e.regs.PC = ROM_LOAD_ADDR;
		rk65c02_step(&e, 2);
		ATF_CHECK((e.stopreason == STEPPED) || (e.stopreason == EMUERROR)
		    || (e.stopreason == STP));
	}
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(mmu_remap_fuzz, do_mmu_remap_fuzz)

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mmu_set_requires_translate);
	ATF_TP_ADD_TC(tp, mmu_set_requires_translate_jit);
	ATF_TP_ADD_TC(tp, mmu_identity_fastpath);
	ATF_TP_ADD_TC(tp, mmu_identity_fastpath_jit);
	ATF_TP_ADD_TC(tp, mmu_fault_write);
	ATF_TP_ADD_TC(tp, mmu_fault_write_jit);
	ATF_TP_ADD_TC(tp, mmu_exec_fault);
	ATF_TP_ADD_TC(tp, mmu_exec_fault_jit);
	ATF_TP_ADD_TC(tp, mmu_write_perm_fault);
	ATF_TP_ADD_TC(tp, mmu_write_perm_fault_jit);
	ATF_TP_ADD_TC(tp, mmu_out_of_range_fault);
	ATF_TP_ADD_TC(tp, mmu_out_of_range_fault_jit);
	ATF_TP_ADD_TC(tp, mmu_extended_phys);
	ATF_TP_ADD_TC(tp, mmu_extended_phys_jit);
#ifdef HAVE_LIGHTNING
	ATF_TP_ADD_TC(tp, mmu_demand_page_jit);
#endif
	ATF_TP_ADD_TC(tp, mmu_tlb_hits);
	ATF_TP_ADD_TC(tp, mmu_tlb_hits_jit);
	ATF_TP_ADD_TC(tp, mmu_tlb_update_invalidate);
	ATF_TP_ADD_TC(tp, mmu_tlb_update_invalidate_jit);
	ATF_TP_ADD_TC(tp, mmu_tlb_disabled);
	ATF_TP_ADD_TC(tp, mmu_tlb_disabled_jit);
	ATF_TP_ADD_TC(tp, mmu_tlb_offset_mismatch_no_fill);
	ATF_TP_ADD_TC(tp, mmu_tlb_offset_mismatch_no_fill_jit);
	ATF_TP_ADD_TC(tp, mmu_tlb_flush_vpage);
	ATF_TP_ADD_TC(tp, mmu_tlb_flush_vpage_jit);
	ATF_TP_ADD_TC(tp, mmu_vrange_wrap_invalidate_all);
	ATF_TP_ADD_TC(tp, mmu_vrange_wrap_invalidate_all_jit);
	ATF_TP_ADD_TC(tp, mmu_remap_stress);
	ATF_TP_ADD_TC(tp, mmu_remap_stress_jit);
	ATF_TP_ADD_TC(tp, mmu_remap_fuzz);
	ATF_TP_ADD_TC(tp, mmu_remap_fuzz_jit);
	return atf_no_error();
}
