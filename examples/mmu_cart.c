/*
 * MMU Cart Example — Host program (C64-style bank-switched cartridge)
 *
 * This example shows how a host uses the rk65c02 MMU API to implement
 * a simple "game cart" with two 16KB banks. The guest selects the bank
 * by writing to $DE00; the host polls that address and updates the
 * mapping. The contract between emulated hardware and guest software
 * (which address is the bank register, what values mean) is defined
 * entirely by this host program — the library only provides the
 * translation callback and update hooks.
 *
 * Build: make mmu_cart mmu_cart_bank0.rom mmu_cart_bank1.rom
 * Run:   ./mmu_cart
 *
 * Expected: runs bank 0 (stores 0xA5 to 0x0200), then bank 1 (stores 0xB6),
 * then stops. We verify 0x0200 == 0xB6. The host uses step() for bounded
 * execution; the tick callback polls 0xDE00 and updates the MMU mapping.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bus.h"
#include "rk65c02.h"

/* -------------------------------------------------------------------------
 * Physical layout (host's view of the 64K bus)
 *   0x0000 - 0x7FFF   Main RAM (including $0200 result, $DE00 bank register)
 *   0x8000 - 0xBFFF   Bank 0 cartridge content (loaded from mmu_cart_bank0.rom)
 *   0xC000 - 0xFFFF   Bank 1 cartridge content (loaded from mmu_cart_bank1.rom)
 *
 * Guest (virtual) layout
 *   Same as physical for 0x0000-0x7FFF and 0xC000-0xFFFF.
 *   ********** 0x8000 - 0xBFFF is the "cart window" **********
 *   The host's translate callback maps this window to either
 *   physical 0x8000-0xBFFF (bank 0) or 0xC000-0xFFFF (bank 1)
 *   based on the value the guest wrote to $DE00 (which we poll).
 * ------------------------------------------------------------------------- */

#define CART_WINDOW_START  0x8000
#define CART_WINDOW_END    0xBFFF
#define BANK_REG          0xDE00
#define RESULT_ADDR       0x0200

#define PHYS_BANK0_START  0x8000
#define PHYS_BANK1_START  0xC000

/* Host state: current bank selection (0 or 1). Updated only at cart entry 0x8000. */
struct cart_state {
	uint8_t  current_bank;
	uint8_t  last_bank_reg;   /* last value we saw at 0xDE00 (tick uses for flush only) */
	uint32_t tick_count;      /* cap run length so we don't run forever on fault */
};

static rk65c02_mmu_result_t
cart_translate(rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access, void *ctx)
{
	struct cart_state   *cs = (struct cart_state *)ctx;
	rk65c02_mmu_result_t r  = {
		.ok = true,
		.paddr = vaddr,
		.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X,
		.fault_code = 0,
		.no_fill_tlb = false,
	};
	uint8_t reg;

	(void)access;

	/*
	 * When translating an address in the cart window, use the bank selected
	 * at 0xDE00. Only apply the new bank when the guest fetches from the cart
	 * entry point (0x8000); otherwise we could switch in the middle of an
	 * instruction (e.g. JMP 0x8000 at 0x8008 would then fetch from 0xC008).
	 * no_fill_tlb: don't cache these translations so the next fetch from
	 * 0x8000 always calls us and sees the updated bank (TLB stays internal).
	 */
	if (vaddr >= CART_WINDOW_START && vaddr <= CART_WINDOW_END) {
		reg = bus_read_1(e->bus, BANK_REG);
		if (vaddr == CART_WINDOW_START)
			cs->last_bank_reg = reg, cs->current_bank = reg & 1;
		if (cs->current_bank == 0)
			r.paddr = vaddr;   /* physical 0x8000-0xBFFF */
		else
			r.paddr = (uint32_t)(vaddr - CART_WINDOW_START + PHYS_BANK1_START);  /* 0xC000-0xFFFF */
		r.no_fill_tlb = true;
	}
	return r;
}

/*
 * Poll the "bank register" at $DE00. When it changes, flush the TLB for the
 * cart window so the next fetch from the cart entry (0x8000) will call
 * translate and see the new value. We do not set current_bank here — only
 * the translate callback does that when vaddr==0x8000, so we never switch
 * in the middle of an instruction (e.g. JMP at 0x8008 keeps using bank 0).
 */
static void
cart_tick(rk65c02emu_t *e, void *ctx)
{
	struct cart_state *cs = (struct cart_state *)ctx;
	uint8_t reg;

	reg = bus_read_1(e->bus, BANK_REG);
	if (reg == cs->last_bank_reg)
		return;
	cs->last_bank_reg = reg;

	rk65c02_mmu_begin_update(e);
	for (uint16_t p = 0x80; p <= 0xBF; p++)
		rk65c02_mmu_mark_changed_vpage(e, (uint8_t)p);
	rk65c02_mmu_end_update(e);
	cs->tick_count++;
	if (cs->tick_count > 500000u)
		rk65c02_request_stop(e);
}

int
main(void)
{
	struct cart_state cs = { .current_bank = 0, .last_bank_reg = 0xFF, .tick_count = 0 };
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
	if (!bus_load_file(&b, PHYS_BANK0_START, "mmu_cart_bank0.rom")) {
		fprintf(stderr, "mmu_cart: cannot load mmu_cart_bank0.rom\n");
		return 1;
	}
	if (!bus_load_file(&b, PHYS_BANK1_START, "mmu_cart_bank1.rom")) {
		fprintf(stderr, "mmu_cart: cannot load mmu_cart_bank1.rom\n");
		return 1;
	}

	e = rk65c02_init(&b);
	e.regs.SP = 0xFF;
	e.regs.PC = CART_WINDOW_START;   /* start in cart window (bank 0) */

	/* Install MMU: our translate callback and optional fault callback (NULL). */
	assert(rk65c02_mmu_set(&e, cart_translate, &cs, NULL, NULL, true, false));

	rk65c02_tick_set(&e, cart_tick, 0, &cs);

	/* Run (bounded by step count). */
	rk65c02_step(&e, 65535);

	uint8_t result = bus_read_1(&b, RESULT_ADDR);
	bus_finish(&b);

	if (result != 0xB6) {
		fprintf(stderr, "FAIL: result at 0x0200 is 0x%02X (expected 0xB6 from bank 1)\n", result);
		return 1;
	}
	if (e.stopreason != STP) {
		fprintf(stderr, "FAIL: stop reason is %s (expected STP)\n",
		    rk65c02_stop_reason_string(e.stopreason));
		return 1;
	}
	printf("PASS: bank 0 ran, then bank 1 wrote 0xB6 to 0x0200 and stopped with STP.\n");
	return 0;
}
