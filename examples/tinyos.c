/*
 * Tiny OS Example — Host program (task scheduler with extended physical RAM)
 *
 * Demonstrates 64K virtual address space with 512KB physical: first 64KB is
 * system RAM (kernel, vectors, MMIO); three tasks each have 32KB in extended
 * physical space (0x10000, 0x18000, 0x20000). Virtual $0000–$7FFF is
 * per-task (ZP, stack, code); $8000–$FFFF is shared. Tasks yield cooperatively
 * (write next id to $FF00, JMP $1000) or via WAI (host idle_wait picks next
 * task, updates MMU, asserts IRQ; handler JMP $1000).
 *
 * Build: make tinyos tinyos_kernel.rom tinyos_task.rom
 * Run:   ./tinyos  (or: make run-tinyos)
 *
 * Expected: tasks 0, 1, 2 print to console in round-robin; cooperative and
 * IRQ-driven switches; eventual STP and PASS.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bus.h"
#include "device.h"
#include "device_ram.h"
#include "rk65c02.h"

/* -------------------------------------------------------------------------
 * Physical layout (512KB conceptual; we use 64K system + 3×32K task regions)
 *   0x00000 – 0x0FFFF   System RAM (kernel at $8000, vectors $FFFC–$FFFF,
 *                      yield $FF00, current task $FF01, console MMIO $DE00)
 *   0x10000 – 0x17FFF   Task 0 private (32KB)
 *   0x18000 – 0x1FFFF   Task 1 private (32KB)
 *   0x20000 – 0x27FFF   Task 2 private (32KB)
 *
 * Virtual layout
 *   $0000 – $7FFF      Per-task → physical 0x10000 + (current_task * 0x8000)
 *   $8000 – $FFFF      Shared (identity to physical $8000–$FFFF)
 * ------------------------------------------------------------------------- */

#define TASK_SIZE         0x8000u
#define TASK_ENTRY        0x1000u
#define PHYS_TASK0_BASE   0x10000u
#define PHYS_TASK1_BASE   0x18000u
#define PHYS_TASK2_BASE   0x20000u
#define YIELD_REG         0xFF00u
#define CURRENT_TASK_REG  0xFF01u
#define KERNEL_START      0x8000u
#define IRQ_HANDLER       0x8010u
#define CONSOLE_BASE      0xDE00u
#define NUM_TASKS         3u

struct task_state {
	uint8_t current_task;   /* 0, 1, or 2 */
};

static uint8_t
console_read_1(void *dev, uint16_t doff)
{
	(void)dev;
	(void)doff;
	return 0;
}

static void
console_write_1(void *dev, uint16_t doff, uint8_t val)
{
	struct task_state *ts;

	(void)doff;
	ts = (struct task_state *)((device_t *)dev)->config;
	if (ts != NULL)
		printf("[%u] %c", (unsigned)ts->current_task, (char)val);
	else
		putchar((char)val);
}

static device_t console_device = {
	.name = "console",
	.size = 16,
	.read_1 = console_read_1,
	.write_1 = console_write_1,
	.finish = NULL,
	.config = NULL,
	.aux = NULL
};

static rk65c02_mmu_result_t
tinyos_translate(rk65c02emu_t *e, uint16_t vaddr, rk65c02_mmu_access_t access,
    void *ctx)
{
	struct task_state *ts = (struct task_state *)ctx;
	rk65c02_mmu_result_t r = {
		.ok = true,
		.paddr = (uint32_t)vaddr,
		.perms = RK65C02_MMU_PERM_R | RK65C02_MMU_PERM_W | RK65C02_MMU_PERM_X,
		.fault_code = 0,
		.no_fill_tlb = false,
	};

	(void)access;

	if (vaddr < TASK_SIZE) {
		if (vaddr == TASK_ENTRY) {
			ts->current_task = bus_read_1(e->bus, YIELD_REG) % NUM_TASKS;
			bus_write_1(e->bus, CURRENT_TASK_REG, ts->current_task);
		}
		switch (ts->current_task) {
		case 0:
			r.paddr = PHYS_TASK0_BASE + vaddr;
			break;
		case 1:
			r.paddr = PHYS_TASK1_BASE + vaddr;
			break;
		case 2:
			r.paddr = PHYS_TASK2_BASE + vaddr;
			break;
		default:
			r.paddr = PHYS_TASK0_BASE + vaddr;
			break;
		}
		r.no_fill_tlb = true;
	}
	return r;
}

static void
on_idle_wait(rk65c02emu_t *e, void *ctx)
{
	struct task_state *ts = (struct task_state *)ctx;
	uint8_t next;
	unsigned int p;

	next = (ts->current_task + 1) % NUM_TASKS;
	bus_write_1(e->bus, YIELD_REG, next);
	bus_write_1(e->bus, CURRENT_TASK_REG, next);
	ts->current_task = next;

	rk65c02_mmu_begin_update(e);
	for (p = 0; p < 0x80; p++)
		rk65c02_mmu_mark_changed_vpage(e, (uint8_t)p);
	rk65c02_mmu_end_update(e);

	rk65c02_assert_irq(e);
}

int
main(void)
{
	struct task_state ts = { .current_task = 0 };
	rk65c02emu_t e;
	bus_t b;
	device_t *sys_ram;
	device_t *task0_ram;
	device_t *task1_ram;
	device_t *task2_ram;

	b = bus_init();
	/* System RAM: 0-$FFFE (0xFFFF bytes) + $FF00-$FFFF (256 bytes). */
	sys_ram = device_ram_init(0xFFFF);
	bus_device_add(&b, sys_ram, 0);
	bus_device_add(&b, device_ram_init(0x100), 0xFF00);

	console_device.config = &ts;
	bus_device_add(&b, &console_device, CONSOLE_BASE);

	task0_ram = device_ram_init(TASK_SIZE);
	task1_ram = device_ram_init(TASK_SIZE);
	task2_ram = device_ram_init(TASK_SIZE);
	bus_device_add_phys(&b, task0_ram, PHYS_TASK0_BASE);
	bus_device_add_phys(&b, task1_ram, PHYS_TASK1_BASE);
	bus_device_add_phys(&b, task2_ram, PHYS_TASK2_BASE);

	if (!bus_load_file(&b, KERNEL_START, "tinyos_kernel.rom")) {
		fprintf(stderr, "tinyos: cannot load tinyos_kernel.rom\n");
		bus_finish(&b);
		return 1;
	}
	if (!bus_load_file_phys(&b, PHYS_TASK0_BASE + TASK_ENTRY,
	    "tinyos_task.rom")) {
		fprintf(stderr, "tinyos: cannot load tinyos_task.rom at task 0\n");
		bus_finish(&b);
		return 1;
	}
	if (!bus_load_file_phys(&b, PHYS_TASK1_BASE + TASK_ENTRY,
	    "tinyos_task.rom")) {
		fprintf(stderr, "tinyos: cannot load tinyos_task.rom at task 1\n");
		bus_finish(&b);
		return 1;
	}
	if (!bus_load_file_phys(&b, PHYS_TASK2_BASE + TASK_ENTRY,
	    "tinyos_task.rom")) {
		fprintf(stderr, "tinyos: cannot load tinyos_task.rom at task 2\n");
		bus_finish(&b);
		return 1;
	}

	bus_write_1(&b, 0xFFFC, (uint8_t)(KERNEL_START & 0xFF));
	bus_write_1(&b, 0xFFFD, (uint8_t)(KERNEL_START >> 8));
	bus_write_1(&b, 0xFFFE, (uint8_t)(IRQ_HANDLER & 0xFF));
	bus_write_1(&b, 0xFFFF, (uint8_t)(IRQ_HANDLER >> 8));

	e = rk65c02_init(&b);
	e.regs.SP = 0xFF;
	e.regs.PC = KERNEL_START;

	assert(rk65c02_mmu_set(&e, tinyos_translate, &ts, NULL, NULL, true, false));

	bus_write_1(&b, YIELD_REG, 0);
	bus_write_1(&b, CURRENT_TASK_REG, 0);

	rk65c02_idle_wait_set(&e, on_idle_wait, &ts);

	rk65c02_start(&e);

	if (e.stopreason != STP) {
		fprintf(stderr, "FAIL: stop reason is %s (expected STP)\n",
		    rk65c02_stop_reason_string(e.stopreason));
		bus_finish(&b);
		return 1;
	}
	printf("\nPASS: Tiny OS tasks ran (cooperative + IRQ yield), stopped with STP.\n");
	bus_finish(&b);
	return 0;
}
