/*
 * JIT benchmark — same workload with JIT on vs off, report wall time.
 *
 * Build: make jit_bench min3.rom
 * Run:   ./jit_bench
 *
 * Demonstrates: toggling JIT with rk65c02_jit_enable; timing interpreter vs
 * JIT for the same ROM (min3) over multiple iterations.
 * Expected: prints interpreter and JIT times; PASS.
 */
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "instruction.h"
#include "rk65c02.h"

static const uint16_t load_addr = 0xC000;
#define ITERATIONS  500

static double
elapsed_us(struct timespec *t0, struct timespec *t1)
{
	return (double)(t1->tv_sec - t0->tv_sec) * 1e6 +
	    (double)(t1->tv_nsec - t0->tv_nsec) / 1e3;
}

int
main(void)
{
	rk65c02emu_t e;
	struct timespec t0, t1;
	uint8_t a = 5, b = 9, c = 4;
	uint32_t i;
	double us_interp, us_jit;

	e = rk65c02_load_rom("min3.rom", load_addr, NULL);
	e.regs.SP = 0xFF;
	e.regs.PC = load_addr;

	/* Interpreter: JIT off, run ITERATIONS times. */
	rk65c02_jit_enable(&e, false);
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (i = 0; i < ITERATIONS; i++) {
		e.regs.SP = 0xFF;
		e.regs.PC = load_addr;
		stack_push(&e, a);
		stack_push(&e, b);
		stack_push(&e, c);
		rk65c02_start(&e);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	us_interp = elapsed_us(&t0, &t1);

	/* JIT: enable JIT, run ITERATIONS times. */
	rk65c02_jit_enable(&e, true);
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (i = 0; i < ITERATIONS; i++) {
		e.regs.SP = 0xFF;
		e.regs.PC = load_addr;
		stack_push(&e, a);
		stack_push(&e, b);
		stack_push(&e, c);
		rk65c02_start(&e);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	us_jit = elapsed_us(&t0, &t1);

	printf("min3.rom x %u iterations:\n", ITERATIONS);
	printf("  interpreter: %.0f us (%.1f us/run)\n", us_interp, us_interp / ITERATIONS);
	printf("  JIT:         %.0f us (%.1f us/run)\n", us_jit, us_jit / ITERATIONS);
	printf("PASS: JIT benchmark done.\n");
	return 0;
}
