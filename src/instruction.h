#ifndef _INSTRUCTION_H_
#define _INSTRUCTION_H_

#include "rk65c02.h"

typedef enum {
	IMPLIED,
	IMMEDIATE,
	ZP,
	ZPX,
	ZPY,
	IZP,
	IZPX,
	IZPY,
	ZPR,
	RELATIVE,
	ABSOLUTE,
	ABSOLUTEX,
	ABSOLUTEY,
	IABSOLUTE,
	IABSOLUTEX,
	ACCUMULATOR
} addressing_t;

struct instruction {
	uint8_t opcode;
	uint8_t op1;
	uint8_t op2;
};

typedef struct instruction instruction_t;

struct instrdef {
	uint8_t opcode;		/* opcode, normally same as in instruction */
	const char *mnemonic;
	addressing_t mode;
	uint8_t size;
	void (*emul)(rk65c02emu_t *e, void *id, instruction_t *i);
	bool modify_pc;
};

typedef struct instrdef instrdef_t;

struct assembler {
	bus_t *bus;
	uint16_t pc;
};

typedef struct assembler assembler_t;

instruction_t instruction_fetch(bus_t *, uint16_t);
instrdef_t instruction_decode(uint8_t);
void instruction_print(instruction_t *);
char * instruction_string_get(instruction_t *);
void disassemble(bus_t *, uint16_t);
uint8_t instruction_data_read_1(rk65c02emu_t *, instrdef_t *, instruction_t *);
void instruction_data_write_1(rk65c02emu_t *, instrdef_t *, instruction_t *, uint8_t);
uint16_t instruction_data_address(rk65c02emu_t *e, instrdef_t *id, instruction_t *i);
void instruction_status_adjust_zero(rk65c02emu_t *, uint8_t);
void instruction_status_adjust_negative(rk65c02emu_t *, uint8_t);
void stack_push(rk65c02emu_t *, uint8_t);
uint8_t stack_pop(rk65c02emu_t *);
void program_counter_increment(rk65c02emu_t *, instrdef_t *);
bool instruction_modify_pc(instrdef_t *);
void program_counter_branch(rk65c02emu_t *, int8_t);

bool assemble_single_buf_implied(uint8_t **, uint8_t *, const char *);
bool assemble_single_buf(uint8_t **, uint8_t *, const char *, addressing_t, uint8_t, uint8_t);

assembler_t assemble_init(bus_t *, uint16_t);
bool assemble_single(assembler_t *, const char *, addressing_t, uint8_t, uint8_t);
bool assemble_single_implied(assembler_t *, const char *);

#endif /* _INSTRUCTION_H_ */
