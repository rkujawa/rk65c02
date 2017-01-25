#ifndef _RK6502_H_
#define _RK6502_H_

#include "bus.h"

typedef enum {
	STOPPED,
	RUNNING,
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

#define P_CARRY 0x1
#define P_ZERO 0x2
#define P_IRQ_DISABLE 0x4
#define P_DECIMAL 0x8
#define P_BREAK 0x10
#define P_UNDEFINED 0x20
#define P_SIGN_OVERFLOW 0x40
#define P_NEGATIVE 0x80

#define NEGATIVE P_NEGATIVE /* sign bit */

#define STACK_START 0x0100
#define STACK_END 0x01FF

#define BIT(val,n) ((val) & (1 << (n)))

typedef struct reg_state reg_state_t;

struct rk65c02emu {
	emu_state_t state;
	bus_t *bus;
	reg_state_t regs;
};

typedef struct rk65c02emu rk65c02emu_t;

rk65c02emu_t rk65c02_init(bus_t *);
void rk65c02_start(rk65c02emu_t *);

#endif

