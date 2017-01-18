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
	uint8_t opcode;
	const char *mnemonic;
	addressing_t mode;
	uint8_t size;
	void (*emul)(rk65c02emu_t *e, instruction_t *i);
};

typedef struct instrdef instrdef_t;

instruction_t instruction_fetch(bus_t *, uint16_t);
instrdef_t instruction_decode(uint8_t);
void instruction_print(instruction_t *);
void disassemble(bus_t *, uint16_t);
//void instruction_execute(rk65c02emu_t *, instruction_t *);

#endif /* _INSTRUCTION_H_ */
