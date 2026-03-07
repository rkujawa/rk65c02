/*
 * Stepper example — step-by-step execution and state inspection.
 *
 * Build: make stepper stepper.rom
 * Run:   ./stepper
 *
 * Demonstrates: rk65c02_step(e, 1) in a loop, reading regs and disassembling
 * the current instruction after each step. Stops when not STEPPED or after
 * a bounded number of steps.
 * Expected: trace of PC and disassembly for each step, then STP; PASS.
 */
#include <stdint.h>
#include <stdio.h>

#include "bus.h"
#include "instruction.h"
#include "rk65c02.h"

static const uint16_t load_addr = 0xC000;
#define MAX_STEPS  25

int
main(void)
{
	rk65c02emu_t e;
	uint16_t steps;

	e = rk65c02_load_rom("stepper.rom", load_addr, NULL);
	e.regs.SP = 0xFF;
	e.regs.PC = load_addr;

	printf("Stepping (max %d steps):\n", MAX_STEPS);
	for (steps = 0; steps < MAX_STEPS; steps++) {
		rk65c02_step(&e, 1);
		printf("  step %u  PC=$%04X  A=$%02X X=$%02X Y=$%02X SP=$%02X  ",
		    (unsigned)steps + 1, e.regs.PC, e.regs.A, e.regs.X, e.regs.Y, e.regs.SP);
		disassemble(e.bus, e.regs.PC);
		if (e.stopreason != STEPPED)
			break;
	}

	printf("Stop reason: %s\n", rk65c02_stop_reason_string(e.stopreason));
	if (e.stopreason != STP) {
		fprintf(stderr, "FAIL: expected STP, got %s\n",
		    rk65c02_stop_reason_string(e.stopreason));
		return 1;
	}
	printf("PASS: stepped to STP.\n");
	return 0;
}
