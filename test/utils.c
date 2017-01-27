#include <stdio.h>
#include <limits.h>
#include <atf-c.h>

#include "bus.h"
#include "rk65c02.h"

#include "utils.h"

bool
rom_start(rk65c02emu_t *e, const char *name, const atf_tc_t *tc)
{
	char rompath[PATH_MAX];
	const char *srcdir;
       
	srcdir = atf_tc_get_config_var(tc, "srcdir");
	
	strcpy(rompath, srcdir);
	strcat(rompath, "/");
	strcat(rompath, name);

	printf("%s\n", rompath);
        e->regs.PC = ROM_LOAD_ADDR;
        if(!bus_load_file(e->bus, ROM_LOAD_ADDR, rompath))
                return false;
        rk65c02_start(e);

        return true;
}

