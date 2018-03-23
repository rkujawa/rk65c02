#ifndef _RK6502_H_
#define _RK6502_H_

#include "bus.h"

typedef enum {
	STOPPED,
	RUNNING,
	STEPPING	/* XXX: how to implement? */
} emu_state_t;

typedef enum {
	STP,		/* due to 65C02 STP instruction */
	WAI,		/* waiting for interrupt */
	BREAKPOINT,	/* due to breakpoint set */
	WATCHPOINT,	/* due to watchpoint set */
	STEPPED,	/* stepped appropriate number of instructions */
	HOST,		/* due to host stop function called */
	EMUERROR	/* due to emulator error */
} emu_stop_reason_t;

struct reg_state {
	uint8_t A;      /* accumulator */
	uint8_t X;      /* index X */
	uint8_t Y;      /* index Y */

	uint16_t PC;    /* program counter */
	uint8_t SP;     /* stack pointer */
	uint8_t P;      /* status */
};

typedef struct reg_state reg_state_t;

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

#define VECTOR_IRQ 0xFFFE	/* also used for BRK */

#define BIT(val,n) ((val) & (1 << (n)))

typedef struct breakpoint_t {
	uint16_t address;
	struct breakpoint_t *next;
} breakpoint_t;

typedef struct trace_t {
	uint16_t address;
	uint8_t opcode, op1, op2;
	reg_state_t regs;
	struct trace_t *prev,*next;
} trace_t;

struct rk65c02emu {
	emu_state_t state;
	bus_t *bus;
	reg_state_t regs;
	emu_stop_reason_t stopreason;
	bool irq;		/* interrupt request line state, true is asserted */

	breakpoint_t *bps_head;	/* pointer to linked list with breakpoints  */
	bool runtime_disassembly; /* disassemble code when emulator is running */
	bool trace;		/* tracing mode enable/disable */
	trace_t *trace_head;	/* pointer to linked list with trace log */
};

typedef struct rk65c02emu rk65c02emu_t;

rk65c02emu_t rk65c02_init(bus_t *);
void rk65c02_start(rk65c02emu_t *);
void rk65c02_step(rk65c02emu_t *, uint16_t);
char *rk65c02_regs_string_get(reg_state_t);
void rk65c02_dump_regs(reg_state_t);
void rk65c02_dump_stack(rk65c02emu_t *, uint8_t);
void rk65c02_irq(rk65c02emu_t *e);

#endif

