#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include <atf-c.h>
#include <gc/gc.h>

#include "bus.h"
#include "rk65c02.h"
#include "log.h"

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
rom_start_with_jit(rk65c02emu_t *e, const char *name, const atf_tc_t *tc,
    bool use_jit)
{
	const char *path;

	rk65c02_jit_enable(e, use_jit);
	rk65c02_jit_flush(e);
	path = rom_path(name, tc);
	rk65c02_log(LOG_INFO, "Loading ROM: %s", path);
	e->regs.PC = ROM_LOAD_ADDR;
	if (!bus_load_file(e->bus, ROM_LOAD_ADDR, path))
		return false;
	rk65c02_start(e);
	return true;
}

bool
rom_start(rk65c02emu_t *e, const char *name, const atf_tc_t *tc)
{
	return rom_start_with_jit(e, name, tc, false);
}
