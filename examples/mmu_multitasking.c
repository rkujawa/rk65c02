/*
 * MMU Multitasking Example — Host program (minimal task switching)
 *
 * This example shows how a host uses the rk65c02 MMU API to implement
 * two tasks with private low memory ($0000-$3FFF) and shared high memory
 * ($8000-$FFFF). The guest "yields" by writing the next task id to $FF00;
 * the host polls that address and remaps virtual $0000-$3FFF to the
 * selected task's physical region. The contract (yield register at $FF00,
 * current-task id at $FF01) is defined by this host — the library only
 * provides the translation callback and begin/mark/end_update.
 *
 * Build: make mmu_multitasking mmu_multitasking_kernel.rom mmu_multitasking_task.rom
 * Run:   ./mmu_multitasking
 *
 * Expected: kernel at $8000 starts task 0; task 0 and task 1 alternate
 * (each inc $0200, yield); after a few switches both tasks hit 3 and stop.
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
 * Physical layout
 *   0x0000 - 0x3FFF   Task 0 private
 *   0x4000 - 0x7FFF   Task 1 private
 *   0x8000 - 0xFFFF   Shared (kernel; identity in translate)
 *
 * Virtual layout (what the CPU sees)
 *   0x0000 - 0x3FFF   Mapped to current task's physical region (0-$3FFF or $4000-$7FFF)
 *   0x8000 - 0xFFFF   Identity (shared)
 *
 * Guest contract: write next task id (0 or 1) to $FF00 to yield; host
 * writes current task id to $FF01 when switching so guest can yield to "the other".
 * ------------------------------------------------------------------------- */

#define TASK_SIZE         0x4000
#define TASK_ENTRY        0x1000   /* Guest JMPs here to run a task; switch only at entry */
#define PHYS_TASK0_START  0x0000
#define PHYS_TASK1_START  0x4000
#define YIELD_REG         0xFF00
#define CURRENT_TASK_REG  0xFF01
#define KERNEL_START      0x8000

struct task_state {
	uint8_t current_task;   /* 0 or 1 */
	uint8_t last_yield;    /* last value seen at $FF00 */
};

static rk65c02_mmu_result_t
task_translate(rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access, void *ctx)
{
	struct task_state *ts = (struct task_state *)ctx;
	rk65c02_mmu_result_t r = {
		.ok = true,
		.paddr = vaddr,
		.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X,
		.fault_code = 0,
		.no_fill_tlb = false,
	};

	(void)access;

	/*
	 * Low 16KB is per-task; high 32KB is shared (identity).
	 * Apply the task selection only when translating the task entry (0x1000),
	 * so we switch when the guest actually JMPs there, not in the middle of
	 * the yielding task's code. See MMU.md §3.2 (no_fill_tlb at entry).
	 */
	if (vaddr < TASK_SIZE) {
		if (vaddr == TASK_ENTRY)
			ts->current_task = bus_read_1(e->bus, YIELD_REG) & 1;
		if (ts->current_task == 0)
			r.paddr = vaddr;
		else
			r.paddr = (uint32_t)(vaddr + PHYS_TASK1_START);
		/* Do not cache so that a fetch from 0x1000 always calls us and sees latest yield. */
		r.no_fill_tlb = true;
	}
	return r;
}

static void
task_tick(rk65c02emu_t *e, void *ctx)
{
	struct task_state *ts = (struct task_state *)ctx;
	uint8_t next;

	next = bus_read_1(e->bus, YIELD_REG);
	if (next == ts->last_yield)
		return;
	ts->last_yield = next;

	/*
	 * Notify library: low 16KB mapping may change when guest next enters
	 * the task region (translate callback reads YIELD_REG and applies it).
	 */
	rk65c02_mmu_begin_update(e);
	for (uint16_t p = 0; p < 0x40; p++)
		rk65c02_mmu_mark_changed_vpage(e, (uint8_t)p);
	rk65c02_mmu_end_update(e);

	/* Guest-visible "current task" so the task can yield to the other. */
	bus_write_1(e->bus, CURRENT_TASK_REG, next & 1);
}

int
main(void)
{
	struct task_state ts = { .current_task = 0, .last_yield = 0xFF };
	rk65c02emu_t e;
	bus_t b;
	uint8_t c0, c1;

	b = bus_init_with_default_devs();
	/* Default RAM ends at 0xDFFE; add top page so YIELD_REG and CURRENT_TASK_REG are in bus. */
	bus_device_add(&b, device_ram_init(0x100), 0xFF00);
	if (!bus_load_file(&b, KERNEL_START, "mmu_multitasking_kernel.rom")) {
		fprintf(stderr, "mmu_multitasking: cannot load mmu_multitasking_kernel.rom\n");
		return 1;
	}
	if (!bus_load_file(&b, PHYS_TASK0_START + 0x1000, "mmu_multitasking_task.rom")) {
		fprintf(stderr, "mmu_multitasking: cannot load mmu_multitasking_task.rom\n");
		return 1;
	}
	/* Load same task code into task 1's region at physical $5000. */
	if (!bus_load_file(&b, PHYS_TASK1_START + 0x1000, "mmu_multitasking_task.rom")) {
		fprintf(stderr, "mmu_multitasking: cannot load task ROM into task 1 region\n");
		return 1;
	}

	e = rk65c02_init(&b);
	e.regs.SP = 0xFF;
	e.regs.PC = KERNEL_START;

	assert(rk65c02_mmu_set(&e, task_translate, &ts, NULL, NULL, true, false));

	/* Initial yield and current task so kernel's JMP 0x1000 runs task 0. */
	bus_write_1(&b, YIELD_REG, 0);
	bus_write_1(&b, CURRENT_TASK_REG, 0);

	/*
	 * Optional: tick flushes TLB when yield changes; switch is applied in
	 * translate when vaddr==TASK_ENTRY, so we can run without tick.
	 */
	rk65c02_tick_set(&e, task_tick, 1, &ts);

	rk65c02_start(&e);

	if (e.stopreason != STP) {
		fprintf(stderr, "FAIL: stop reason is %s (expected STP)\n",
		    rk65c02_stop_reason_string(e.stopreason));
		bus_finish(&b);
		return 1;
	}
	c0 = bus_read_1(&b, 0x0200);
	c1 = bus_read_1(&b, 0x4200);
	/* Both tasks run; the one that does the final STP may be 3, the other at least 2. */
	if (c0 < 2 || c1 < 2 || (c0 != 3 && c1 != 3)) {
		fprintf(stderr, "FAIL: task 0=$%02x task 1=$%02x (expected both >=2, one STP at 3)\n",
		    c0, c1);
		bus_finish(&b);
		return 1;
	}
	printf("PASS: both tasks ran and stopped with STP (task0=%u task1=%u).\n",
	    (unsigned)c0, (unsigned)c1);
	bus_finish(&b);
	return 0;
}
