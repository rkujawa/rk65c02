#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "bus.h"
#include "instruction.h"
#include "rk65c02.h"


/*
int
main(void)
{
	bus_t b;

	b = bus_init();

	bus_write_1(&b, 0, OP_INX);
	bus_write_1(&b, 1, OP_NOP);
	bus_write_1(&b, 2, OP_LDY_IMM);
	bus_write_1(&b, 3, 0x1);
	bus_write_1(&b, 4, OP_TSB_ZP);
	bus_write_1(&b, 5, 0x3);
	bus_write_1(&b, 6, OP_JSR);
	bus_write_1(&b, 7, 0x09);
	bus_write_1(&b, 8, 0x0);
	bus_write_1(&b, 9, OP_STP);

	rk6502_start(&b, 0);

	bus_finish(&b);
}
*/


int main()
{
	char* input, shell_prompt[100];

	rl_bind_key('\t', rl_complete);

	while(true) {
		snprintf(shell_prompt, sizeof(shell_prompt), "> ");

		input = readline(shell_prompt);

		if (!input)
			break;

		add_history(input);

		free(input);
	}
	return EXIT_SUCCESS;
}

