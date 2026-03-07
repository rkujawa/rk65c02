/*
 * 8-bit × 8-bit multiplication example — calls a ROM routine.
 *
 * Runs in interpreter mode (no JIT). Arguments and result passed on the stack.
 *
 * Build: make mul_8bit_to_8bits mul_8bit_to_8bits.rom
 * Run:   ./mul_8bit_to_8bits
 * Expected: 4 * 8 = 32.
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
	uint8_t num1, num2;
	uint8_t res;

	rk65c02emu_t e;

	e = rk65c02_load_rom("mul_8bit_to_8bits.rom", load_addr, NULL);

	e.regs.SP = 0xFF;
	e.regs.PC = load_addr;
	num1 = 4; num2 = 8;

	stack_push(&e, num1);
	stack_push(&e, num2);

	rk65c02_start(&e);

	res = stack_pop(&e);
	printf("Result of multiplication: %d\n", res);
	if (res != 32) {
		fprintf(stderr, "FAIL: expected 4*8=32, got %d\n", res);
		return 1;
	}
	printf("PASS: 4 * 8 = 32.\n");
	return 0;
}

