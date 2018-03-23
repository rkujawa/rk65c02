#include <stdio.h>
#include <stdlib.h>
#include <utlist.h>

#include "rk65c02.h"
#include "instruction.h"
#include "log.h"
#include "debug.h"

void
debug_trace_set(rk65c02emu_t *e, bool state)
{
	e->trace = state;
}

void
debug_trace_print_all(rk65c02emu_t *e)
{
	trace_t *tr;
	instruction_t i;
	char *instrstr;
	char *regsstr;

	if (e->trace_head == NULL)
		return;

	DL_FOREACH(e->trace_head, tr) {
		i.opcode = tr->opcode;
		i.op1 = tr->op1;
		i.op2 = tr->op2;
		instrstr = instruction_string_get(&i);
		regsstr = rk65c02_regs_string_get(tr->regs);

		rk65c02_log(LOG_TRACE, "%X: %s\t%s", tr->address, instrstr,
		    regsstr);

		free(instrstr);
		free(regsstr);
	}

}

void
debug_trace_savestate(rk65c02emu_t *e, uint16_t address, instrdef_t *id,
    instruction_t *i)
{
	trace_t *tr;

	tr = (trace_t *) malloc(sizeof(trace_t));
	if (tr == NULL) {
		fprintf(stderr, "Error allocating trace structure.\n");
		return;
	}

	tr->address = address;

	tr->opcode = i->opcode;
	tr->op1 = i->op1;
	tr->op2 = i->op2; 

	tr->regs = e->regs;

	DL_APPEND((e->trace_head), tr);
}

bool
debug_breakpoint_remove(rk65c02emu_t *e, uint16_t address)
{
	breakpoint_t *bp, *tmp;

	if (e->bps_head == NULL)
		return false;

	LL_FOREACH_SAFE(e->bps_head, bp, tmp) {
		if (bp->address == address) {
			LL_DELETE(e->bps_head, bp);
			return true;
		}
	}

	return false;
}

bool
debug_breakpoint_add(rk65c02emu_t *e, uint16_t address)
{
	breakpoint_t *bp;

	bp = (breakpoint_t *) malloc(sizeof(breakpoint_t));
	if (bp == NULL)
		return false;

	bp->address = address;

	LL_APPEND((e->bps_head), bp);

	return true;
}

bool
debug_PC_is_breakpoint(rk65c02emu_t *e)
{
	breakpoint_t *bp;

	if (e->bps_head == NULL)
		return false;

	LL_FOREACH(e->bps_head, bp) {
		if (bp->address == e->regs.PC)
			return true;
	}


	return false;
}
