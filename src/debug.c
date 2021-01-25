/*
 *      SPDX-License-Identifier: GPL-3.0-only
 *
 *      rk65c02
 *      Copyright (C) 2017-2021  Radoslaw Kujawa
 *
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, version 3 of the License.
 * 
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>

#include <gc/gc.h>
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
	}

}

void
debug_trace_savestate(rk65c02emu_t *e, uint16_t address, instrdef_t *id,
    instruction_t *i)
{
	trace_t *tr;

	tr = (trace_t *) GC_MALLOC(sizeof(trace_t));
	assert(tr != NULL);

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

	bp = (breakpoint_t *) GC_MALLOC(sizeof(breakpoint_t));
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
