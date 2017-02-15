#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdint.h>
#include <stdbool.h>

#include "rk65c02.h" 
#include "instruction.h"

bool debug_PC_is_breakpoint(rk65c02emu_t *);
bool debug_breakpoint_add(rk65c02emu_t *, uint16_t);
bool debug_breakpoint_remove(rk65c02emu_t *, uint16_t);

void debug_trace_set(rk65c02emu_t *, bool);
void debug_trace_savestate(rk65c02emu_t *, uint16_t, instrdef_t *, instruction_t *);
void debug_trace_print_all(rk65c02emu_t *);

#endif /* _DEBUG_H_ */

