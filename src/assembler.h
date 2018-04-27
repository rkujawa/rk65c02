#ifndef _ASSEMBLER_H_
#define _ASSEMBLER_H_

#include "instruction.h"
#include "rk65c02.h"

struct assembler {
	bus_t *bus;
	uint16_t pc;
};

typedef struct assembler assembler_t;

bool assemble_single_buf_implied(uint8_t **, uint8_t *, const char *);
bool assemble_single_buf(uint8_t **, uint8_t *, const char *, addressing_t, uint8_t, uint8_t);

assembler_t assemble_init(bus_t *, uint16_t);
bool assemble_single(assembler_t *, const char *, addressing_t, uint8_t, uint8_t);
bool assemble_single_implied(assembler_t *, const char *);

#endif /* _ASSEMBLER_H_ */

