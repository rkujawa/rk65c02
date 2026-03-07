/*
 * MMU PAE-like Example — Host program (one-level page table, extended phys, demand paging)
 *
 * Demonstrates a PAE-like design on top of the rk65c02 MMU API:
 * - A single-level "page table" at physical 0x0400 (256 entries × 4 bytes).
 *   Entry format: bits 8–31 = physical page base, bit 0 = present.
 * - The translate callback walks the table via bus_read_1 (direct bus read; no recursion).
 * - Page 0 is mapped to extended physical 0x10000; other pages identity or not present.
 * - On "not present" fault the host installs the mapping and restarts (demand paging).
 *
 * Build: make mmu_pae
 * Run:   ./mmu_pae
 *
 * Expected: guest runs LDA $10 (from extended 0x10010), STA $20 (to 0x10020),
 * then LDA $0111 (absolute; page 1) faults (page 1 not present); host installs
 * page 1 (identity), restarts; guest reads 0xAB from $0111 and stops with STP.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bus.h"
#include "device_ram.h"
#include "rk65c02.h"

#define PAGE_TABLE_PHYS  0x0400u  /* 256 entries × 4 bytes = 1 KiB */
#define EXT_PHYS_BASE    0x10000u
#define EXT_PAGE_SIZE    256u
#define ROM_LOAD_ADDR    0xC000

#define PRESENT_BIT      1u

static rk65c02_mmu_result_t
pae_translate(rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access, void *ctx)
{
	uint8_t vpage;
	uint32_t entry_addr;
	uint32_t entry;
	uint32_t paddr;
	rk65c02_mmu_result_t r;

	(void)ctx;
	(void)access;

	vpage = (uint8_t)(vaddr >> 8);
	entry_addr = PAGE_TABLE_PHYS + (uint32_t)vpage * 4u;

	/* Walk page table: read 4-byte little-endian entry from bus (direct read). */
	entry = (uint32_t)bus_read_1(e->bus, (uint16_t)(entry_addr + 0));
	entry |= (uint32_t)bus_read_1(e->bus, (uint16_t)(entry_addr + 1)) << 8;
	entry |= (uint32_t)bus_read_1(e->bus, (uint16_t)(entry_addr + 2)) << 16;
	entry |= (uint32_t)bus_read_1(e->bus, (uint16_t)(entry_addr + 3)) << 24;

	if ((entry & PRESENT_BIT) == 0) {
		r.ok = false;
		r.paddr = 0;
		r.perms = 0;
		r.fault_code = 1; /* page not present */
		r.no_fill_tlb = false;
		return r;
	}

	paddr = (entry & 0xFFFFFF00u) | (uint32_t)(vaddr & 0xFFu);
	r.ok = true;
	r.paddr = paddr;
	r.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X;
	r.fault_code = 0;
	r.no_fill_tlb = false;
	return r;
}

static void
install_page(bus_t *b, uint8_t vpage, uint32_t phys_base)
{
	uint32_t addr = PAGE_TABLE_PHYS + (uint32_t)vpage * 4u;
	uint32_t entry = (phys_base & 0xFFFFFF00u) | PRESENT_BIT;

	bus_write_1(b, (uint16_t)(addr + 0), (uint8_t)(entry >> 0));
	bus_write_1(b, (uint16_t)(addr + 1), (uint8_t)(entry >> 8));
	bus_write_1(b, (uint16_t)(addr + 2), (uint8_t)(entry >> 16));
	bus_write_1(b, (uint16_t)(addr + 3), (uint8_t)(entry >> 24));
}

int
main(void)
{
	bus_t b;
	rk65c02emu_t e;
	unsigned run_count;
	b = bus_init_with_default_devs();
	bus_device_add_phys(&b, device_ram_init(EXT_PAGE_SIZE), EXT_PHYS_BASE);

	/* Page table: page 0 -> extended 0x10000, page 1 not present (demand-paged), rest identity. */
	install_page(&b, 0, EXT_PHYS_BASE);  /* page 0 -> extended 0x10000 */
	/* Page 1: not present initially (demand-paged on first access). */
	bus_write_1(&b, (uint16_t)(PAGE_TABLE_PHYS + 4*1 + 0), 0);
	bus_write_1(&b, (uint16_t)(PAGE_TABLE_PHYS + 4*1 + 1), 0);
	bus_write_1(&b, (uint16_t)(PAGE_TABLE_PHYS + 4*1 + 2), 0);
	bus_write_1(&b, (uint16_t)(PAGE_TABLE_PHYS + 4*1 + 3), 0);
	for (uint16_t vp = 2; vp < 256; vp++) {
		uint32_t base = (uint32_t)vp << 8;
		install_page(&b, (uint8_t)vp, base);
	}

	/* Seed extended phys and identity for page 1 (after demand page). */
	bus_write_1_phys(&b, EXT_PHYS_BASE + 0x10, 0x5A);
	bus_write_1(&b, 0x0111, 0xAB);

	/* Program: LDA $10 (zp, page 0 -> extended), STA $20 (zp), LDA $0111 (abs, page 1 -> identity), STP */
	bus_write_1(&b, ROM_LOAD_ADDR + 0, 0xA5);
	bus_write_1(&b, ROM_LOAD_ADDR + 1, 0x10);
	bus_write_1(&b, ROM_LOAD_ADDR + 2, 0x85);
	bus_write_1(&b, ROM_LOAD_ADDR + 3, 0x20);
	bus_write_1(&b, ROM_LOAD_ADDR + 4, 0xAD); /* LDA abs $0111 */
	bus_write_1(&b, ROM_LOAD_ADDR + 5, 0x11);
	bus_write_1(&b, ROM_LOAD_ADDR + 6, 0x01);
	bus_write_1(&b, ROM_LOAD_ADDR + 7, 0xDB);

	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, true);
	if (!rk65c02_mmu_set(&e, pae_translate, NULL, NULL, NULL, true, false)) {
		fprintf(stderr, "mmu_pae: rk65c02_mmu_set failed\n");
		bus_finish(&b);
		return 1;
	}

	e.regs.PC = ROM_LOAD_ADDR;
	run_count = 0;
	for (;;) {
		rk65c02_start(&e);
		run_count++;
		if (e.stopreason == STP)
			break;
		if (e.stopreason == EMUERROR && e.mmu_last_fault_code == 1) {
			uint8_t vpage = (uint8_t)(e.mmu_last_fault_addr >> 8);
			install_page(&b, vpage, (uint32_t)vpage << 8);
			rk65c02_mmu_begin_update(&e);
			rk65c02_mmu_mark_changed_vpage(&e, vpage);
			rk65c02_mmu_end_update(&e);
			if (run_count >= 4u) {
				fprintf(stderr, "mmu_pae: too many demand-page runs\n");
				bus_finish(&b);
				return 1;
			}
			continue;
		}
		fprintf(stderr, "mmu_pae: unexpected stop reason %d (fault_code %u)\n",
		    (int)e.stopreason, (unsigned)e.mmu_last_fault_code);
		bus_finish(&b);
		return 1;
	}

	if (e.regs.A != 0xAB) {
		fprintf(stderr, "mmu_pae: expected A=0xAB, got 0x%02X\n", e.regs.A);
		bus_finish(&b);
		return 1;
	}
	if (bus_read_1_phys(&b, EXT_PHYS_BASE + 0x20) != 0x5A) {
		fprintf(stderr, "mmu_pae: expected 0x10020=0x5A, got 0x%02X\n",
		    bus_read_1_phys(&b, EXT_PHYS_BASE + 0x20));
		bus_finish(&b);
		return 1;
	}

	printf("PASS: PAE-like table walk, extended phys, and demand paging.\n");
	bus_finish(&b);
	return 0;
}
