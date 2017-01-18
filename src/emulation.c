#include <stdio.h>

#include "emulation.h"

void
emul_nop(rk65c02emu_t *e, instruction_t *i)
{
	printf("nop!\n");
	printf("nop!\n");
	printf("nop!\n");
}

void
emul_stp(rk65c02emu_t *e, instruction_t *i)
{
	e->state = STOPPED;
}
