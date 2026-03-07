#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "bus.h"
#include "rk65c02.h"

static const uint16_t load_addr = 0xC000;
static const uint16_t counter_addr = 0x0200;

struct idle_wait_host_state {
	uint32_t wait_calls;
	uint64_t total_wait_ns;
};

static uint64_t
mono_ns_now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void
on_idle_wait(rk65c02emu_t *e, void *ctx)
{
	struct idle_wait_host_state *hs;
	struct timespec req;
	uint64_t t0, t1;

	hs = (struct idle_wait_host_state *)ctx;
	req.tv_sec = 0;
	req.tv_nsec = 5 * 1000 * 1000; /* 5 ms */

	t0 = mono_ns_now();
	(void)nanosleep(&req, NULL);
	/* Wake CPU from WAI (IRQ delivery can be handled by guest logic). */
	rk65c02_assert_irq(e);
	t1 = mono_ns_now();

	hs->wait_calls++;
	hs->total_wait_ns += (t1 - t0);
}

int
main(void)
{
	rk65c02emu_t e;
	struct idle_wait_host_state host;
	uint8_t counter;

	host.wait_calls = 0;
	host.total_wait_ns = 0;

	e = rk65c02_load_rom("idle_wait.rom", load_addr, NULL);
	e.regs.SP = 0xFF;
	e.regs.PC = load_addr;

	/* Keep default throughput mode unless guest is in WAI. */
	rk65c02_jit_enable(&e, true);
	rk65c02_idle_wait_set(&e, on_idle_wait, &host);

	rk65c02_start(&e);
	counter = bus_read_1(e.bus, counter_addr);

	printf("Stop reason: %s\n", rk65c02_stop_reason_string(e.stopreason));
	printf("Counter at $%04x: %u\n", counter_addr, counter);
	printf("Idle wait callbacks: %" PRIu32 "\n", host.wait_calls);
	printf("Approx idle wait time: %" PRIu64 " us\n",
	    (uint64_t)(host.total_wait_ns / 1000ULL));

	return 0;
}
