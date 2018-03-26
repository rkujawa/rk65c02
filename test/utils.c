#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include <atf-c.h>
#include <gc/gc.h>

#include "bus.h"
#include "rk65c02.h"

#include "utils.h"

const char *
rom_path(const char *name, const atf_tc_t *tc)
{
	char *rompath;
	const char *srcdir;

	rompath = GC_MALLOC(PATH_MAX);
	srcdir = atf_tc_get_config_var(tc, "srcdir");

	strcpy(rompath, srcdir);
	strcat(rompath, "/");
	strcat(rompath, name);

	return rompath;
}

bool
rom_start(rk65c02emu_t *e, const char *name, const atf_tc_t *tc)
{
	const char *path;

	path = rom_path(name, tc);
	printf("%s\n", path);
        e->regs.PC = ROM_LOAD_ADDR;
        if(!bus_load_file(e->bus, ROM_LOAD_ADDR, path))
                return false;
        rk65c02_start(e);

        return true;
}
