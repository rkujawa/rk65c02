#ifndef _EMULATION_H_
#define _EMULATION_H_

#include "rk65c02.h"
#include "instruction.h"

void emul_nop(rk65c02emu_t *, instruction_t *);
void emul_stp(rk65c02emu_t *, instruction_t *);

#endif /* _EMULATION_H_*/

