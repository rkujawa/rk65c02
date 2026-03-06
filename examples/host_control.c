#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bus.h"
#include "rk65c02.h"

static const uint16_t load_addr = 0xC000;
static const uint16_t counter_addr = 0x0200;

struct host_state {
	uint32_t ticks_seen;
	uint32_t tick_budget;
	bool stop_notified;
};

static void
on_stop(rk65c02emu_t *e, emu_stop_reason_t reason, void *ctx)
{
	struct host_state *hs = ctx;

	hs->stop_notified = true;
	printf("Emulation stopped: %s (PC=%#04x)\n",
	    rk65c02_stop_reason_string(reason),
	    e->regs.PC);
}

static void
on_tick(rk65c02emu_t *e, void *ctx)
{
	struct host_state *hs = ctx;

	hs->ticks_seen++;
	if (hs->ticks_seen >= hs->tick_budget)
		rk65c02_request_stop(e);
}

int
main(void)
{
	struct host_state host = {
		.ticks_seen = 0,
		.tick_budget = 20000,
		.stop_notified = false,
	};
	rk65c02emu_t e;
	uint8_t counter_before;
	uint8_t counter_after;

	e = rk65c02_load_rom("host_control.rom", load_addr, NULL);
	e.regs.SP = 0xFF;
	e.regs.PC = load_addr;

	/*
	 * Tick runs every 100 poll points and asks the core to stop after
	 * tick_budget callbacks. In JIT mode poll points are block boundaries,
	 * so cadence is coarser than per-instruction interpreter ticking.
	 */
	rk65c02_on_stop_set(&e, on_stop, &host);
	rk65c02_tick_set(&e, on_tick, 100, &host);
	rk65c02_jit_enable(&e, true);

	counter_before = bus_read_1(e.bus, counter_addr);
	rk65c02_start(&e);
	counter_after = bus_read_1(e.bus, counter_addr);

	printf("Tick callbacks: %" PRIu32 "\n", host.ticks_seen);
	printf("Counter at $%04x before=%u after=%u\n", counter_addr,
	    counter_before, counter_after);
	printf("on_stop callback invoked: %s\n",
	    host.stop_notified ? "yes" : "no");

	/* Continue manually with stepping after host-initiated stop. */
	rk65c02_tick_clear(&e);
	host.stop_notified = false;
	rk65c02_step(&e, 5);
	printf("After 5-step run, stop reason is %s\n",
	    rk65c02_stop_reason_string(e.stopreason));

	return 0;
}
