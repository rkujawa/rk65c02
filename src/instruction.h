#ifndef _INSTRUCTION_H_
#define _INSTRUCTION_H_

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

struct instrdef {
	uint8_t opcode;
	const char *mnemonic;
	addressing_t mode;
	uint8_t size;
};

typedef struct instrdef instrdef_t;

struct instruction {
	instrdef_t def;
	uint8_t op1;
	uint8_t op2;
};

typedef struct instruction instruction_t;

instruction_t instruction_fetch(bus_t *, uint16_t);
void instruction_print(instruction_t *);
void disassemble(bus_t *, uint16_t);
instrdef_t instrdef_get(uint8_t);

#endif /* _INSTRUCTION_H_ */
