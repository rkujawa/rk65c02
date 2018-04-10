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

	e.regs.SP = 0xFF;
	e.regs.PC = load_addr;
	a = 5; b = 9; c = 4;

	stack_push(&e, a);
	stack_push(&e, b);
	stack_push(&e, c);

	rk65c02_start(&e);

	min = stack_pop(&e);
	printf("Min is: %d\n", min);
}

