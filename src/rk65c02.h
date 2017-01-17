#ifndef _RK6502_H_
#define _RK6502_H_

#include "bus.h"
#include "instruction.h"

typedef enum {
	STOPPED,
	RUNNIG,
	STEPPING
} emu_state_t;

struct reg_state {
	uint8_t A;      /* accumulator */
	uint8_t X;      /* index X */
	uint8_t Y;      /* index Y */

	uint16_t PC;    /* program counter */
	uint8_t SP;     /* stack pointer */
	uint8_t P;      /* status */
};

typedef struct reg_state reg_state_t;

struct rk65c02emu {
	emu_state_t state;
	bus_t bus;
	reg_state_t regs;
};

typedef struct rk65c02emu rk65c02emu_t;

#endif
