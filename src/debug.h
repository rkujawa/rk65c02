/**
 * @file debug.h
 * @brief Breakpoint and trace helper API.
 */
#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdint.h>
#include <stdbool.h>

#include "rk65c02.h" 
#include "instruction.h"

/** Check whether current PC matches any configured breakpoint. */
bool debug_PC_is_breakpoint(rk65c02emu_t *);
/** Add breakpoint at address. */
bool debug_breakpoint_add(rk65c02emu_t *, uint16_t);
/** Remove breakpoint at address. */
bool debug_breakpoint_remove(rk65c02emu_t *, uint16_t);

/** Enable or disable trace collection. */
void debug_trace_set(rk65c02emu_t *, bool);
/** Save one trace entry (usually called after instruction execution). */
void debug_trace_savestate(rk65c02emu_t *, uint16_t, instrdef_t *, instruction_t *);
/** Print all collected trace entries. */
void debug_trace_print_all(rk65c02emu_t *);

#endif /* _DEBUG_H_ */

