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
#include "device_ram.h"
#include "rk65c02.h"

/* -------------------------------------------------------------------------
 * Physical layout (host's view)
 *   The cart's banks live in extended physical space (above 64K); the CPU
 *   never sees them directly. Only the cart window (see below) can reach them.
 *
 *   0x0000 - 0x7FFF   Main RAM (including $0200 result, $DE00 bank register)
 *   0x8000 - 0xFFFF   Main RAM (legacy 64K space; guest high range maps here)
 *   0x10000 - 0x13FFF Cart bank 0 (16K; loaded from mmu_cart_bank0.rom)
 *   0x14000 - 0x17FFF Cart bank 1 (16K; loaded from mmu_cart_bank1.rom)
 *
 * Guest (virtual) layout — C64-style: cart has its own "address space";
 * the only way the CPU can access cart content is through the window.
 *
 *   0x0000 - 0x7FFF   Identity → physical 0x0000-0x7FFF (main RAM)
 *   0x8000 - 0xBFFF   CART WINDOW → physical 0x10000-0x13FFF (bank 0) or
 *                     0x14000-0x17FFF (bank 1), depending on $DE00
 *   0xC000 - 0xFFFF   → physical 0x4000-0x7FFF (RAM mirror), NOT the cart.
 * ------------------------------------------------------------------------- */

#define CART_WINDOW_START  0x8000
#define CART_WINDOW_END    0xBFFF
#define BANK_REG          0xDE00   /* Guest address for bank select */
#define RESULT_ADDR       0x0200

#define CART_BANK_SIZE    0x4000u  /* 16K per bank */

#define PHYS_BANK0_START  0x10000u  /* Extended physical: cart bank 0 */
#define PHYS_BANK1_START  0x14000u  /* Extended physical: cart bank 1 */

/* Guest 0xC000-0xFFFF maps to physical 0x4000-0x7FFF, so guest BANK_REG (0xDE00)
 * stores to physical 0x4000 + (0xDE00 & 0x3FFF) = 0x5E00. We poll that. */
#define PHYS_BANK_REG     0x5E00

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
		reg = bus_read_1(e->bus, PHYS_BANK_REG);  /* guest $DE00 → physical $5E00 */
		if (vaddr == CART_WINDOW_START)
			cs->last_bank_reg = reg, cs->current_bank = reg & 1;
		if (cs->current_bank == 0)
			r.paddr = (uint32_t)(vaddr - CART_WINDOW_START + PHYS_BANK0_START);  /* 0x10000-0x13FFF */
		else
			r.paddr = (uint32_t)(vaddr - CART_WINDOW_START + PHYS_BANK1_START);  /* 0x14000-0x17FFF */
		r.no_fill_tlb = true;
	} else if (vaddr >= 0xC000) {
		/* Guest high range: map to RAM (0x4000-0x7FFF), not to the cart.
		 * So the cart is only visible through the window, C64-style. */
		r.paddr = (uint32_t)(0x4000 + (vaddr & 0x3FFF));
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

	reg = bus_read_1(e->bus, PHYS_BANK_REG);
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

	/* Cart banks in extended physical space (above 64K). */
	bus_device_add_phys(&b, device_ram_init(CART_BANK_SIZE), PHYS_BANK0_START);
	bus_device_add_phys(&b, device_ram_init(CART_BANK_SIZE), PHYS_BANK1_START);
	if (!bus_load_file_phys(&b, PHYS_BANK0_START, "mmu_cart_bank0.rom")) {
		fprintf(stderr, "mmu_cart: cannot load mmu_cart_bank0.rom\n");
		return 1;
	}
	if (!bus_load_file_phys(&b, PHYS_BANK1_START, "mmu_cart_bank1.rom")) {
		fprintf(stderr, "mmu_cart: cannot load mmu_cart_bank1.rom\n");
		return 1;
	}

	e = rk65c02_init(&b);
	e.regs.SP = 0xFF;
	e.regs.PC = CART_WINDOW_START;   /* start in cart window (bank 0) */

	/* Install MMU: our translate callback and optional fault callback (NULL). */
	assert(rk65c02_mmu_set(&e, cart_translate, &cs, NULL, NULL, true, false));

	/* Tick interval 0: poll bank register every opportunity (interpreter: every insn). */
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
