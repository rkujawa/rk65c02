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
	bus_t bus;
	rk65c02emu_t e;

	bus = bus_init_with_default_devs();
	e = rk65c02_init(&bus);

	bus_load_file(e.bus, load_addr, "min3.rom");

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

