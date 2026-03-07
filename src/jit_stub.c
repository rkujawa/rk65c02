/*
 * JIT stub — no-op implementations when built without GNU Lightning.
 * Linked instead of jit.c when HAVE_LIGHTNING is not defined.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rk65c02.h"
#include "jit.h"

extern void rk65c02_exec(rk65c02emu_t *e);

void
rk65c02_jit_enable(rk65c02emu_t *e, bool enable)
{
	(void)enable;
	if (e == NULL)
		return;
	e->jit_requested = false;
	e->use_jit = false;
}

void
rk65c02_jit_flush(rk65c02emu_t *e)
{
	(void)e;
}

void
rk65c02_jit_invalidate_all(rk65c02emu_t *e)
{
	(void)e;
}

void
rk65c02_jit_invalidate_vpage(rk65c02emu_t *e, uint8_t vpage)
{
	(void)e;
	(void)vpage;
}

void
rk65c02_jit_invalidate_code_vpage(rk65c02emu_t *e, uint8_t vpage)
{
	(void)e;
	(void)vpage;
}

void
rk65c02_run_jit(rk65c02emu_t *e)
{
	/* Fall back to interpreter loop (same as in jit.c when !use_jit). */
	if (e == NULL)
		return;
	e->state = RUNNING;
	while (e->state == RUNNING) {
		rk65c02_poll_host_controls(e);
		if (e->state != RUNNING) {
			if (rk65c02_maybe_wait_on_idle(e))
				continue;
			break;
		}
		rk65c02_exec(e);
		rk65c02_poll_host_controls(e);
		if (e->state != RUNNING) {
			if (rk65c02_maybe_wait_on_idle(e) && e->state == RUNNING)
				continue;
			break;
		}
	}
}
