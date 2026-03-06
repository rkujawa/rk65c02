/*
 *      SPDX-License-Identifier: GPL-3.0-only
 *
 *      rk65c02 - JIT support (internal header)
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

#endif /* _JIT_H_ */

