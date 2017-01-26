#include "bus.h"
#include "rk65c02.h"

#include "utils.h"

bool
rom_start(rk65c02emu_t *e, const char *name)
{
        e->regs.PC = ROM_LOAD_ADDR;
        if(!bus_load_file(e->bus, ROM_LOAD_ADDR, name))
                return false;
        rk65c02_start(e);

        return true;
}

