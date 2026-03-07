/*
 *      SPDX-License-Identifier: GPL-3.0-only
 *
 *      rk65c02 - JIT support (internal header)
 */
/**
 * @file jit.h
 * @brief Internal JIT helpers used by core execution loops.
 */
#ifndef _JIT_H_
#define _JIT_H_

#include "rk65c02.h"

/*
 * JIT-related entry points are declared in rk65c02.h for public use.
 * This header exists mostly to satisfy the generic %.o: %.c %.h rule
 * in the Makefile and to provide internal prototypes.
 */

void rk65c02_run_jit(rk65c02emu_t *e);
void rk65c02_poll_host_controls(rk65c02emu_t *e);
bool rk65c02_maybe_wait_on_idle(rk65c02emu_t *e);
void rk65c02_jit_invalidate_all(rk65c02emu_t *e);
void rk65c02_jit_invalidate_vpage(rk65c02emu_t *e, uint8_t vpage);
void rk65c02_jit_invalidate_code_vpage(rk65c02emu_t *e, uint8_t vpage);

#ifdef HAVE_LIGHTNING
/* BCD ADC/SBC helpers: JIT calls these when P_DECIMAL is set. */
void rk65c02_do_adc_bcd(rk65c02emu_t *e, uint8_t operand);
void rk65c02_do_sbc_bcd(rk65c02emu_t *e, uint8_t operand);
#endif

#endif /* _JIT_H_ */

