#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include <errno.h>
#include <assert.h>
#include <string.h>

#include <gc/gc.h>

#include "bus.h"
#include "instruction.h"
#include "rk65c02.h"
#include "log.h"
#include "debug.h"

void rk65c02_exec(rk65c02emu_t *);

rk65c02emu_t
rk65c02_load_rom(const char *path, uint16_t load_addr, bus_t *b)
{
	rk65c02emu_t e;

	if (b == NULL) {
		b = GC_MALLOC(sizeof(bus_t));
		*b = bus_init_with_default_devs();
	}

	/* XXX: normal error handling instead of assert would be preferred */
	assert(bus_load_file(b, load_addr, path));

	e = rk65c02_init(b);

	return e;
}

rk65c02emu_t
rk65c02_init(bus_t *b)
{
	rk65c02emu_t e;

	e.bus = b;
	e.state = STOPPED;
	e.stopreason = HOST;
	e.regs.P = P_UNDEFINED|P_IRQ_DISABLE;
	/* reset also clears the decimal flag */
	e.regs.P &= ~P_DECIMAL;

	e.irq = false;

	e.bps_head = NULL;
	e.trace = false;
	e.trace_head = NULL;
	e.runtime_disassembly = false;

	rk65c02_log(LOG_DEBUG, "Initialized new emulator.");

	return e;
}

void
rk65c02_assert_irq(rk65c02emu_t *e)
{
	/* 
	 * Clearly this is too simpleton'ish, because more than one device
	 * might want to assert the interrupt line (on hardware level it is
	 * active low, so can just be pulled down by any device connected
	 * to it.
	 */
	e->irq = true;

	if ((e->state == STOPPED) && (e->stopreason == WAI))
		rk65c02_start(e);
}

void
rk65c02_irq(rk65c02emu_t *e)
{
	/* push return address to the stack */
	stack_push(e, e->regs.PC >> 8);
	stack_push(e, e->regs.PC & 0xFF);
	/* push processor status to the stack with break flag set */
	stack_push(e, e->regs.P);

	/* 
	 * The IRQ disable is set, decimal flags is cleared _after_ pushing
	 * the P register to the stack.
	 */
	e->regs.P |= P_IRQ_DISABLE;
	e->regs.P &= ~P_DECIMAL;

	/* break flag is only saved to the stack, it is not present in the ISR... */
	e->regs.P &= ~P_BREAK;

	/* load address from IRQ vector into program counter */
	e->regs.PC = bus_read_1(e->bus, VECTOR_IRQ);
	e->regs.PC |= bus_read_1(e->bus, VECTOR_IRQ + 1) << 8;
}

/*
 * Execute a single instruction within emulator.
 */
void
rk65c02_exec(rk65c02emu_t *e)
{
	instruction_t i;
	instrdef_t id;
	uint16_t tpc;	/* saved PC for tracing */

	tpc = e->regs.PC;

	if (e->irq && (!(e->regs.P & P_IRQ_DISABLE)))
		rk65c02_irq(e);

	/* XXX: handle watchpoints toos */
	if (debug_PC_is_breakpoint(e)) {
		e->state = STOPPED;
		e->stopreason = BREAKPOINT;
		return;
	}

	if (e->runtime_disassembly)
		disassemble(e->bus, e->regs.PC);

	i = instruction_fetch(e->bus, e->regs.PC);
	id = instruction_decode(i.opcode);

	if (id.emul != NULL) {
		id.emul(e, &id, &i);

		if (!instruction_modify_pc(&id)) 
			program_counter_increment(e, &id);
	} else {
		/*
		 * Technically, on a real 65C02, all invalid opcodes
		 * are NOPs, but until rk65c02 reaches some level of
		 * maturity, let's catch them here to help iron out the
		 * bugs.
		 */
		rk65c02_panic(e, "invalid opcode %X @ %X\n",
		    i.opcode, e->regs.PC);
	}

	if (e->trace)
		debug_trace_savestate(e, tpc, &id, &i);

}

void
rk65c02_start(rk65c02emu_t *e) {

	assert(e != NULL);

	e->state = RUNNING;
	while (e->state == RUNNING) {
		rk65c02_exec(e);
	}
}

void
rk65c02_step(rk65c02emu_t *e, uint16_t steps) {

	uint16_t i = 0;

	assert(e != NULL);

	e->state = STEPPING;
	while ((e->state == STEPPING) && (i < steps)) {
		rk65c02_exec(e);
		i++;
	}

	e->state = STOPPED;
	e->stopreason = STEPPED;
}

void
rk65c02_dump_stack(rk65c02emu_t *e, uint8_t n)
{
	uint16_t stackaddr;

	stackaddr = STACK_END-n;

	while (stackaddr <= STACK_END) {

		if ((stackaddr == STACK_END-n) || !((stackaddr % 0x10)))
			printf("stack %#02x: ", stackaddr);

		printf("%#02x ", bus_read_1(e->bus, stackaddr));

		stackaddr++;

		if (!(stackaddr % 0x10))
			printf("\n");
	}
}

void
rk65c02_dump_regs(reg_state_t regs)
{
	char *str;

	str = rk65c02_regs_string_get(regs);

	printf ("%s\n", str);

}

char *
rk65c02_regs_string_get(reg_state_t regs)
{
#define REGS_STR_LEN 50
	char *str;

	/* XXX: string allocation to a separate utility function? */
	str = GC_MALLOC(REGS_STR_LEN);
	if (str == NULL) {
		rk65c02_log(LOG_CRIT, "Error allocating memory for buffer: %s.",
		    strerror(errno));
		return NULL;
	}
	memset(str, 0, REGS_STR_LEN);

	snprintf(str, REGS_STR_LEN, "A: %X X: %X Y: %X PC: %X SP: %X P: %c%c%c%c%c%c%c%c", 
	    regs.A, regs.X, regs.Y, regs.PC, regs.SP, 
	    (regs.P & P_NEGATIVE) ? 'N' : '-',
	    (regs.P & P_SIGN_OVERFLOW) ? 'V' : '-',
	    (regs.P & P_UNDEFINED) ? '1' : '-',
	    (regs.P & P_BREAK) ? 'B' : '-',
	    (regs.P & P_DECIMAL) ? 'D' : '-',
	    (regs.P & P_IRQ_DISABLE) ? 'I' : '-',
	    (regs.P & P_ZERO) ? 'Z' : '-',
	    (regs.P & P_CARRY) ? 'C' : '-');

	return str;
}

void
rk65c02_panic(rk65c02emu_t *e, const char* fmt, ...) 
{
	va_list args;

	va_start(args, fmt);
	rk65c02_log(LOG_CRIT, fmt, args);
	va_end(args);

	e->state = STOPPED;
	e->stopreason = EMUERROR;

	/* TODO: run some UI callback. */
}

