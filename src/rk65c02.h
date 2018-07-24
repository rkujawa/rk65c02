/** @file rk65c02.h
 *  @brief Public functions for managing rk65c02 emulator.
 */
#ifndef _RK6502_H_
#define _RK6502_H_

#include "bus.h"

/**
 * @brief State of the emulator.
 */
typedef enum {
	STOPPED,	/**< Stopped. */
	RUNNING,	/**< Running. */
	STEPPING	/**< Stepping. */
} emu_state_t;

/**
 * @brief Enum describing why emulation stopped.
 */
typedef enum {
	STP,		/**< Due to 65C02 STP instruction. */
	WAI,		/**< Waiting for interrupt (WAI instruction). */
	BREAKPOINT,	/**< Due to breakpoint set. */
	WATCHPOINT,	/**< Due to watchpoint set (not implemented). */
	STEPPED,	/**< Stepped appropriate number of instructions. */
	HOST,		/**< Due to host stop function called. */
	EMUERROR	/**< Due to emulator error. */
} emu_stop_reason_t;

/**
 * @brief State of the emulated CPU registers.
 */
struct reg_state {
	uint8_t A;	/**< Accumulator. */
	uint8_t X;      /**< Index X. */
	uint8_t Y;      /**< Index Y. */

	uint16_t PC;    /**< Program counter. */
	uint8_t SP;     /**< Stack pointer. */
	uint8_t P;      /**< Status. */
};

typedef struct reg_state reg_state_t;

#define P_CARRY 0x1
#define P_ZERO 0x2
#define P_IRQ_DISABLE 0x4	/**< Status register flag: IRQ disabled */
#define P_DECIMAL 0x8		/**< Status register flag: BCD mode */
#define P_BREAK 0x10		/**< Status register flag: BRK was the cause of interrupt */
#define P_UNDEFINED 0x20	/**< Status register flag: Undefined (always 1) */
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

/**
 * @brief Instance of the emulator.
 */
struct rk65c02emu {
	emu_state_t state;	/**< Current emulator status. */
	bus_t *bus;		/**< Bus to which CPU is attached. */
	reg_state_t regs;	/**< CPU registers. */
	emu_stop_reason_t stopreason; /**< Reason for stopping emulation. */
	bool irq;		/**< Interrupt request line state, true is asserted. */

	breakpoint_t *bps_head;	/**< Pointer to linked list with breakpoints. */
	bool runtime_disassembly; /**< Disassemble code when emulator is running. */
	bool trace;		/**< Tracing mode enable/disable. */
	trace_t *trace_head;	/**< Pointer to linked list with trace log. */
};

typedef struct rk65c02emu rk65c02emu_t;

/**
 * @brief Initialize the new emulator instance. Set initial CPU state.
 * @param b Bus description.
 * @return New emulator instance.
 */
rk65c02emu_t rk65c02_init(bus_t *b);

/**
 * @brief Start the emulator.
 * @param e Emulator instance.
 */
void rk65c02_start(rk65c02emu_t *e);

/**
 * @brief Execute as many instructions as specified in steps argument.
 * @param e Emulator instance.
 * @param steps Number of instructions to execute.
 */
void rk65c02_step(rk65c02emu_t *e, uint16_t steps);

char *rk65c02_regs_string_get(reg_state_t);
void rk65c02_dump_regs(reg_state_t);
void rk65c02_dump_stack(rk65c02emu_t *, uint8_t);

/**
 * @brief Assert the IRQ line.
 * @param e Emulator instance.
 */
void rk65c02_assert_irq(rk65c02emu_t *e);

/**
 * @brief Respond to interrupt and start the interrupt service routine.
 * @param e Emulator instance.
 */
void rk65c02_irq(rk65c02emu_t *e);

/**
 * @brief Handle critical error - send message to log and stop emulation.
 * @param e Emulator instance.
 * @param fmt Message in printf-like format.
 */
void rk65c02_panic(rk65c02emu_t *e, const char *fmt, ...);

/**
 * @brief Prep the emulator, load code from file, pass bus config optionally.
 * @param path Path to ROM file to be loaded.
 * @param load_addr Address on the bus where ROM should be loaded.
 * @param b Pre-existing bus configuration, pass NULL if default requested.
 * @return New instance of the emulator prepared to run the ROM.
 */
rk65c02emu_t rk65c02_load_rom(const char *path, uint16_t load_addr,
    bus_t *b);

#endif

