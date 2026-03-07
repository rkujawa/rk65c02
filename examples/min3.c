/*
 * Min3 example — calls a ROM routine to compute the minimum of three values.
 *
 * Arguments (A, B, C) and result (min) are passed on the stack: caller pushes
 * three bytes (C, B, A with A on top), routine returns one byte (the minimum).
 *
 * Build: make min3 min3.rom
 * Run:   ./min3
 * Expected: min(5, 9, 4) = 4.
 */
#include <stdio.h>
#include <stdint.h>

#include "rk65c02.h"
#include "bus.h"
#include "log.h"
#include "instruction.h"

static const uint16_t load_addr = 0xC000;

int main(void)
{
	uint8_t a, b, c;
	uint8_t min;

	rk65c02emu_t e;

	e = rk65c02_load_rom("min3.rom", load_addr, NULL);

	rk65c02_jit_enable(&e, true);

	e.regs.SP = 0xFF;
	e.regs.PC = load_addr;
	a = 5; b = 9; c = 4;

	stack_push(&e, a);
	stack_push(&e, b);
	stack_push(&e, c);

	rk65c02_start(&e);

	min = stack_pop(&e);
	printf("Min is: %d\n", min);
	if (min != 4) {
		fprintf(stderr, "FAIL: expected min=4 (min of 5,9,4), got %d\n", min);
		return 1;
	}
	printf("PASS: min(5, 9, 4) = 4.\n");
	return 0;
}

