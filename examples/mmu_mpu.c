/*
 * MMU MPU Example — Host program (simple MPU with MMIO, violation → IRQ)
 *
 * Uses the rk65c02 MMU API to implement a simple Memory Protection Unit:
 * flat 64K address space with programmable protection regions. An imaginary
 * MPU has MMIO registers at $FE00–$FE0D for configuration and violation
 * status. On protection violation the fault callback records the violation
 * and asserts IRQ; when the host restarts, the CPU takes the IRQ before
 * re-executing the faulting instruction. The guest handler can read
 * violation registers, clear IRQ, and optionally fix permissions then RTI.
 *
 * Build: make mmu_mpu mmu_mpu.rom
 * Run:   timeout 5 ./mmu_mpu   (always use timeout when testing)
 *
 * Expected: guest enables MPU with region 0 $0200–$02FF read-only, stores
 * to $0200 (violation), IRQ runs, handler adds W permission and RTIs,
 * store succeeds, program stops with STP.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <gc/gc.h>

#include "bus.h"
#include "device.h"
#include "device_ram.h"
#include "rk65c02.h"

/* -------------------------------------------------------------------------
 * MPU MMIO layout (base $FE00)
 *   0: control (W/R: bit0=MPU en, bit1=IRQ on violation)
 *   1-2: region0 start lo, hi
 *   3-4: region0 end lo, hi
 *   5: region0 perms (bit0=R, bit1=W, bit2=X)
 *   10-11: violation addr lo, hi (read-only)
 *   12: violation type (0=none, 1=read, 2=write, 3=fetch) (read-only)
 *   13: write any = clear violation / deassert IRQ
 * ------------------------------------------------------------------------- */

#define MPU_MMIO_BASE   0xFE00u
#define MPU_MMIO_SIZE   16u

#define REG_CONTROL     0
#define REG_R0_START_LO 1
#define REG_R0_START_HI 2
#define REG_R0_END_LO   3
#define REG_R0_END_HI   4
#define REG_R0_PERMS    5
#define REG_VIOL_ADDR_LO 10
#define REG_VIOL_ADDR_HI 11
#define REG_VIOL_TYPE   12
#define REG_CLEAR       13

#define CONTROL_MPU_EN  0x01u
#define CONTROL_IRQ_EN  0x02u

#define PERM_R          0x01u
#define PERM_W          0x02u
#define PERM_X          0x04u

#define VIOL_NONE       0
#define VIOL_READ       1
#define VIOL_WRITE      2
#define VIOL_FETCH      3

struct mpu_state {
	uint8_t  control;
	uint16_t r0_start, r0_end;
	uint8_t  r0_perms;
	uint16_t viol_addr;
	uint8_t  viol_type;
};

struct mpu_ctx {
	struct mpu_state state;
	rk65c02emu_t    *e;
};

static uint8_t mpu_device_read_1(void *vd, uint16_t doff);
static void    mpu_device_write_1(void *vd, uint16_t doff, uint8_t val);

static uint8_t
mpu_device_read_1(void *vd, uint16_t doff)
{
	device_t *d = (device_t *)vd;
	struct mpu_ctx *ctx = (struct mpu_ctx *)d->config;
	struct mpu_state *s = &ctx->state;

	if (doff == REG_CONTROL)
		return s->control;
	if (doff == REG_R0_START_LO)
		return (uint8_t)(s->r0_start);
	if (doff == REG_R0_START_HI)
		return (uint8_t)(s->r0_start >> 8);
	if (doff == REG_R0_END_LO)
		return (uint8_t)(s->r0_end);
	if (doff == REG_R0_END_HI)
		return (uint8_t)(s->r0_end >> 8);
	if (doff == REG_R0_PERMS)
		return s->r0_perms;
	if (doff == REG_VIOL_ADDR_LO)
		return (uint8_t)(s->viol_addr);
	if (doff == REG_VIOL_ADDR_HI)
		return (uint8_t)(s->viol_addr >> 8);
	if (doff == REG_VIOL_TYPE)
		return s->viol_type;
	return 0;
}

static void
mpu_device_write_1(void *vd, uint16_t doff, uint8_t val)
{
	device_t *d = (device_t *)vd;
	struct mpu_ctx *ctx = (struct mpu_ctx *)d->config;
	struct mpu_state *s = &ctx->state;

	if (doff == REG_CONTROL) {
		s->control = val;
		return;
	}
	if (doff == REG_R0_START_LO)
		s->r0_start = (s->r0_start & 0xFF00u) | (uint16_t)val;
	if (doff == REG_R0_START_HI)
		s->r0_start = (s->r0_start & 0x00FFu) | ((uint16_t)val << 8);
	if (doff == REG_R0_END_LO)
		s->r0_end = (s->r0_end & 0xFF00u) | (uint16_t)val;
	if (doff == REG_R0_END_HI)
		s->r0_end = (s->r0_end & 0x00FFu) | ((uint16_t)val << 8);
	if (doff == REG_R0_PERMS) {
		s->r0_perms = val;
		if (ctx->e != NULL) {
			rk65c02_mmu_begin_update(ctx->e);
			rk65c02_mmu_mark_changed_vpage(ctx->e, 0x02);
			rk65c02_mmu_end_update(ctx->e);
		}
	}
	if (doff == REG_CLEAR) {
		(void)val;
		s->viol_type = VIOL_NONE;
		if (ctx->e != NULL)
			rk65c02_deassert_irq(ctx->e);
	}
}

static device_t *
mpu_device_init(struct mpu_ctx *ctx)
{
	device_t *d = (device_t *)GC_MALLOC(sizeof(device_t));

	assert(d != NULL);
	d->name = "MPU";
	d->size = MPU_MMIO_SIZE;
	d->read_1 = mpu_device_read_1;
	d->write_1 = mpu_device_write_1;
	d->finish = NULL;
	d->config = ctx;
	d->aux = NULL;
	return d;
}

/* -------------------------------------------------------------------------
 * MMU translate: identity map; enforce region permissions.
 * ------------------------------------------------------------------------- */

static rk65c02_mmu_result_t
mpu_translate(rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access, void *ctx)
{
	struct mpu_ctx *mc = (struct mpu_ctx *)ctx;
	struct mpu_state *s = &mc->state;
	rk65c02_mmu_result_t r = {
		.ok = true,
		.paddr = vaddr,
		.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X,
		.fault_code = 0,
		.no_fill_tlb = false,
	};

	(void)e;

	/* MMIO window: R+W only (no execute from registers). */
	if (vaddr >= MPU_MMIO_BASE && vaddr < MPU_MMIO_BASE + MPU_MMIO_SIZE) {
		r.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W;
		r.no_fill_tlb = true;
		return r;
	}

	if ((s->control & CONTROL_MPU_EN) == 0)
		return r;

	/* Check region 0. */
	if (vaddr >= s->r0_start && vaddr <= s->r0_end) {
		uint8_t perm = 0;
		if (s->r0_perms & PERM_R)
			perm |= RK65C02_MMU_PERM_R;
		if (s->r0_perms & PERM_W)
			perm |= RK65C02_MMU_PERM_W;
		if (s->r0_perms & PERM_X)
			perm |= RK65C02_MMU_PERM_X;
		r.perms = perm;

		if (access == RK65C02_MMU_FETCH && (perm & RK65C02_MMU_PERM_X) == 0) {
			r.ok = false;
			r.fault_code = VIOL_FETCH;
			return r;
		}
		if (access == RK65C02_MMU_READ && (perm & RK65C02_MMU_PERM_R) == 0) {
			r.ok = false;
			r.fault_code = VIOL_READ;
			return r;
		}
		if (access == RK65C02_MMU_WRITE && (perm & RK65C02_MMU_PERM_W) == 0) {
			r.ok = false;
			r.fault_code = VIOL_WRITE;
			return r;
		}
	}
	return r;
}

static void
mpu_on_fault(rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access,
    uint16_t fault_code, void *ctx)
{
	struct mpu_ctx *mc = (struct mpu_ctx *)ctx;

	mc->state.viol_addr = vaddr;
	mc->state.viol_type = (uint8_t)(fault_code & 0xFFu);
	if ((mc->state.control & CONTROL_IRQ_EN) != 0)
		rk65c02_assert_irq(e);
}

#define ROM_LOAD_ADDR    0xC000
#define IRQ_VECTOR       0xFFFE
#define IRQ_HANDLER      0xC000
#define ROM_MAIN         0xC010

static void
tick_cap_cb(rk65c02emu_t *ep, void *ctx)
{
	uint32_t *n = (uint32_t *)ctx;
	if (*n == 0)
		rk65c02_request_stop(ep);
	else
		(*n)--;
}

int
main(void)
{
	bus_t b;
	rk65c02emu_t e;
	struct mpu_ctx mpu_ctx;
	device_t *mpu_dev;

	memset(&mpu_ctx, 0, sizeof(mpu_ctx));
	mpu_ctx.e = NULL;

	b = bus_init_with_default_devs();
	bus_device_add(&b, device_ram_init(0x100), 0xFF00);
	mpu_dev = mpu_device_init(&mpu_ctx);
	bus_device_add(&b, mpu_dev, MPU_MMIO_BASE);

	{
		FILE *f;
		uint8_t buf[0x4000];
		size_t n;
		long fsize;

		f = fopen("mmu_mpu.rom", "rb");
		if (f == NULL) {
			fprintf(stderr, "mmu_mpu: cannot open mmu_mpu.rom\n");
			bus_finish(&b);
			return 1;
		}
		if (fseek(f, 0, SEEK_END) != 0 ||
		    (fsize = ftell(f)) < 0 ||
		    fseek(f, 0, SEEK_SET) != 0) {
			fprintf(stderr, "mmu_mpu: cannot size rom\n");
			fclose(f);
			bus_finish(&b);
			return 1;
		}
		/* vasm -Fbin with .space 0xC000: code at file offset 0xC000. */
		if ((size_t)fsize > (size_t)ROM_LOAD_ADDR) {
			if (fseek(f, (long)ROM_LOAD_ADDR, SEEK_SET) != 0) {
				fprintf(stderr, "mmu_mpu: cannot seek to 0x%x in rom\n",
				    (unsigned)ROM_LOAD_ADDR);
				fclose(f);
				bus_finish(&b);
				return 1;
			}
			n = fread(buf, 1, sizeof(buf), f);
			if (n == 0) {
				fprintf(stderr, "mmu_mpu: no data at 0x%x in rom\n",
				    (unsigned)ROM_LOAD_ADDR);
				fclose(f);
				bus_finish(&b);
				return 1;
			}
			bus_load_buf(&b, ROM_LOAD_ADDR, buf, (uint16_t)n);
		} else {
			fprintf(stderr, "mmu_mpu: rom too small (need >= 0x%x bytes)\n",
			    (unsigned)ROM_LOAD_ADDR);
			fclose(f);
			bus_finish(&b);
			return 1;
		}
		fclose(f);
	}
	/* Sanity: handler must start with LDA #imm (0xA9) at 0xC000 */
	if (bus_read_1(&b, ROM_LOAD_ADDR) != 0xA9u) {
		fprintf(stderr, "mmu_mpu: ROM at 0x%x has 0x%02x (expected 0xA9)\n",
		    (unsigned)ROM_LOAD_ADDR, (unsigned)bus_read_1(&b, ROM_LOAD_ADDR));
		bus_finish(&b);
		return 1;
	}

	/* Set IRQ vector to handler. */
	bus_write_1(&b, IRQ_VECTOR, (uint8_t)(IRQ_HANDLER));
	bus_write_1(&b, IRQ_VECTOR + 1, (uint8_t)(IRQ_HANDLER >> 8));

	e = rk65c02_init(&b);
	mpu_ctx.e = &e;
	e.regs.SP = 0xFF;
	e.regs.PC = ROM_MAIN;

	assert(rk65c02_mmu_set(&e, mpu_translate, &mpu_ctx, mpu_on_fault, &mpu_ctx, true, false));
	rk65c02_jit_enable(&e, true);

	/* Cap execution so we never hang (tick every 1 unit, stop after 50000). */
	static uint32_t tick_cap;
	tick_cap = 50000;
	rk65c02_tick_set(&e, tick_cap_cb, 1, &tick_cap);

	rk65c02_start(&e);
	if (e.stopreason == EMUERROR && e.mmu_last_fault_code != 0) {
		tick_cap = 50000;
		rk65c02_start(&e);
		if (e.stopreason == EMUERROR) {
			fprintf(stderr, "mmu_mpu: faulted again after restart (IRQ not taken?)\n");
			bus_finish(&b);
			return 1;
		}
	}

	if (e.stopreason != STP) {
		fprintf(stderr, "FAIL: stop reason %s (expected STP)\n",
		    rk65c02_stop_reason_string(e.stopreason));
		bus_finish(&b);
		return 1;
	}
	/* Expected: handler added W to region 0, store to $0200 succeeded. */
	if (bus_read_1(&b, 0x0200) != 0x42) {
		fprintf(stderr, "FAIL: $0200 = 0x%02X (expected 0x42 after violation+IRQ+RTI)\n",
		    bus_read_1(&b, 0x0200));
		bus_finish(&b);
		return 1;
	}
	printf("PASS: MPU violation triggered IRQ, handler fixed perms, store succeeded.\n");
	bus_finish(&b);
	return 0;
}
