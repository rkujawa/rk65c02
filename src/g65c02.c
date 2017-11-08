#include <stdlib.h>
#include <stdbool.h>

#include <pthread.h>

#include <libguile.h>

#include "bus.h"
#include "rk65c02.h"

static void guile_main(void *, int, char**);

static SCM wrap_bus_init(void);
static SCM wrap_rk65c02_init(void);

static rk65c02emu_t e;
static bool rk65c02emu_inited;
static bus_t b;
static bool bus_inited;

int
main(int argc, char **argv)
{
	scm_boot_guile(argc, argv, guile_main, 0);

	return 0;
}

static void
guile_main(void* data, int argc, char** argv) {
	scm_c_define_gsubr("bus-init", 0, 0, 0, wrap_bus_init);
	scm_c_define_gsubr("rk65c02-init", 0, 0, 0, wrap_rk65c02_init);

	scm_shell(argc, argv); 
}

static SCM
wrap_bus_init(void)
{
	if (bus_inited)
		return SCM_BOOL_F;

	/* XXX: make this customizable */
	b = bus_init_with_default_devs();

	bus_inited = true;

	return SCM_BOOL_T;
}

static SCM
wrap_rk65c02_init(void)
{
	if (rk65c02_inited)
		return SCM_BOOL_F;

	e = rk65c02_init(&b);

	rk65c02emu_inited = true;

	return SCM_BOOL_T;
}

