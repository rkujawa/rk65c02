/*
 * Interrupts example — IRQ vector at $FFFE, assert/deassert, minimal handler.
 *
 * Build: make interrupts interrupts.rom
 * Run:   ./interrupts
 *
 * Demonstrates: IRQ vector setup, rk65c02_assert_irq from idle_wait callback,
 * guest CLI + WAI + IRQ handler that increments a counter, then STP.
 * Expected: stop reason STP; IRQ count and wake count at 0x0200/0x0201 both 3.
 */
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "bus.h"
#include "device.h"
#include "rk65c02.h"

static const uint16_t load_addr = 0xC000;
static const uint16_t addr_irq_count = 0x0200;
static const uint16_t addr_wake_count = 0x0201;
#define VECTOR_BASE       0xFFFC
#define ADDR_START        0xC000
#define ADDR_IRQ_HANDLER  0xC003

/* 4-byte vector area at $FFFC (reset lo/hi, IRQ lo/hi); default RAM ends at $DFFE. */
static uint8_t vector_ram[4];
static uint8_t vector_read_1(void *config, uint16_t doff) { (void)config; return vector_ram[doff]; }
static void vector_write_1(void *config, uint16_t doff, uint8_t val) { (void)config; vector_ram[doff] = val; }
static device_t vector_device = {
	.name = "vector",
	.size = 4,
	.read_1 = vector_read_1,
	.write_1 = vector_write_1,
	.finish = NULL,
	.config = NULL,
	.aux = NULL
};

struct irq_host_state {
	uint32_t wait_calls;
};

static void
on_idle_wait(rk65c02emu_t *e, void *ctx)
{
	struct irq_host_state *hs = ctx;
	struct timespec req = { .tv_sec = 0, .tv_nsec = 5000000 }; /* 5 ms */

	(void)nanosleep(&req, NULL);
	rk65c02_assert_irq(e);
	hs->wait_calls++;
}

int
main(void)
{
	bus_t bus;
	rk65c02emu_t e;
	struct irq_host_state host;
	uint8_t irq_count, wake_count;

	bus = bus_init_with_default_devs();
	bus_device_add(&bus, &vector_device, VECTOR_BASE);

	if (!bus_load_file(&bus, load_addr, "interrupts.rom")) {
		fprintf(stderr, "FAIL: could not load interrupts.rom\n");
		bus_finish(&bus);
		return 1;
	}

	/* Set reset vector to start, IRQ vector to irq_handler. */
	vector_ram[0] = (uint8_t)(ADDR_START & 0xFF);
	vector_ram[1] = (uint8_t)(ADDR_START >> 8);
	vector_ram[2] = (uint8_t)(ADDR_IRQ_HANDLER & 0xFF);
	vector_ram[3] = (uint8_t)(ADDR_IRQ_HANDLER >> 8);

	e = rk65c02_init(&bus);
	e.regs.SP = 0xFF;
	e.regs.PC = ADDR_START;

	host.wait_calls = 0;
	rk65c02_idle_wait_set(&e, on_idle_wait, &host);

	rk65c02_start(&e);

	irq_count = bus_read_1(e.bus, addr_irq_count);
	wake_count = bus_read_1(e.bus, addr_wake_count);

	bus_finish(&bus);

	printf("Stop reason: %s\n", rk65c02_stop_reason_string(e.stopreason));
	printf("IRQ count at $%04x: %u, wake count at $%04x: %u\n",
	    addr_irq_count, irq_count, addr_wake_count, wake_count);

	if (e.stopreason != STP) {
		fprintf(stderr, "FAIL: expected STP, got %s\n",
		    rk65c02_stop_reason_string(e.stopreason));
		return 1;
	}
	if (irq_count != 3 || wake_count != 3) {
		fprintf(stderr, "FAIL: expected IRQ=3 wake=3, got IRQ=%u wake=%u\n",
		    irq_count, wake_count);
		return 1;
	}
	printf("PASS: IRQ vector and handler ran, 3 WAI wakes, STP.\n");
	return 0;
}
