/*
 * Breakpoints example — debug_breakpoint_add, run until hit, inspect, continue.
 *
 * Build: make breakpoints min3.rom
 * Run:   ./breakpoints
 *
 * Demonstrates: debug_breakpoint_add, run until BREAKPOINT stop, print regs and
 * disassembly at breakpoint, debug_breakpoint_remove, then continue to STP.
 * Expected: stops at breakpoint (min3 entry), prints state; after remove, STP; PASS.
 */
#include <stdint.h>
#include <stdio.h>

#include "bus.h"
#include "debug.h"
#include "instruction.h"
#include "rk65c02.h"

static const uint16_t load_addr = 0xC000;
#define BP_ADDR  0xC004   /* first instruction of min3 routine in min3.s (after JSR+STP) */

int
main(void)
{
	rk65c02emu_t e;
	uint8_t a = 5, b = 9, c = 4;

	e = rk65c02_load_rom("min3.rom", load_addr, NULL);
	e.regs.SP = 0xFF;
	e.regs.PC = load_addr;
	stack_push(&e, a);
	stack_push(&e, b);
	stack_push(&e, c);

	if (!debug_breakpoint_add(&e, BP_ADDR)) {
		fprintf(stderr, "FAIL: could not add breakpoint at $%04X\n", BP_ADDR);
		return 1;
	}

	rk65c02_start(&e);

	if (e.stopreason != BREAKPOINT) {
		fprintf(stderr, "FAIL: expected BREAKPOINT, got %s\n",
		    rk65c02_stop_reason_string(e.stopreason));
		return 1;
	}

	printf("Breakpoint hit at $%04X\n", e.regs.PC);
	printf("  A=$%02X X=$%02X Y=$%02X SP=$%02X\n",
	    e.regs.A, e.regs.X, e.regs.Y, e.regs.SP);
	printf("  ");
	disassemble(e.bus, e.regs.PC);

	debug_breakpoint_remove(&e, BP_ADDR);
	rk65c02_start(&e);

	if (e.stopreason != STP) {
		fprintf(stderr, "FAIL: after continue expected STP, got %s\n",
		    rk65c02_stop_reason_string(e.stopreason));
		return 1;
	}

	printf("PASS: breakpoint hit, state inspected, continued to STP.\n");
	return 0;
}
