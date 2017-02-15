#include <stdio.h>
#include <stdlib.h>
#include <utlist.h>

#include "rk65c02.h"
#include "debug.h"

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
