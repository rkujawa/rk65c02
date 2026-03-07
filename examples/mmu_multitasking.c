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

#define TASK_SIZE        0x4000
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
	rk65c02_mmu_result_t r = { .ok = true, .paddr = vaddr, .perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X, .fault_code = 0 };

	(void)access;

	/* Low 16KB is per-task; high 32KB is shared (identity). */
	if (vaddr < TASK_SIZE) {
		if (ts->current_task == 0)
			r.paddr = vaddr;
		else
			r.paddr = (uint32_t)(vaddr + PHYS_TASK1_START);
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
	ts->current_task = next & 1;

	/* Notify library: low 16KB (pages 0x00-0x3F) changed mapping. */
	rk65c02_mmu_begin_update(e);
	for (uint16_t p = 0; p < 0x40; p++)
		rk65c02_mmu_mark_changed_vpage(e, (uint8_t)p);
	rk65c02_mmu_end_update(e);

	/* Guest-visible "current task" register so the task can yield to the other. */
	bus_write_1(e->bus, CURRENT_TASK_REG, ts->current_task);
}

int
main(void)
{
	struct task_state ts = { .current_task = 0, .last_yield = 0xFF };
	rk65c02emu_t e;
	bus_t b;

	b = bus_init_with_default_devs();
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

	/* Initial task 0; guest kernel will write 0 to $FF00, we'll see it and set $FF01. */
	bus_write_1(&b, CURRENT_TASK_REG, 0);

	rk65c02_tick_set(&e, task_tick, 1, &ts);

	rk65c02_start(&e);

	printf("Stop reason: %s\n", rk65c02_stop_reason_string(e.stopreason));
	printf("Task 0 counter at $0200 (phys): %u\n", bus_read_1(&b, 0x0200));
	printf("Task 1 counter at $0200 (phys $4200): %u\n", bus_read_1(&b, 0x4200));

	bus_finish(&b);
	return 0;
}
