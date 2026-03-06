/*
 * Standalone benchmark: run a compute-intensive ROM with interpreter
 * and with JIT, report elapsed time for each to compare efficiency.
 *
 * Build: make bench_emulation test_emulation_bench_loop.rom
 * Run:   ./bench_emulation [rom_path [runs]]
 *
 * Default ROM: ./test_emulation_bench_loop.rom
 * Default runs: 20 (increase for more stable timings)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __linux__
#include <time.h>
static double
now_sec(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}
#else
#include <sys/time.h>
static double
now_sec(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec / 1e6;
}
#endif

#include <gc/gc.h>

#include "bus.h"
#include "rk65c02.h"
#include "log.h"

#define ROM_LOAD_ADDR 0xC000
#define DEFAULT_ROM   "test_emulation_bench_loop.rom"
#define N_RUNS_DEF    20

int
main(int argc, char **argv)
{
	rk65c02emu_t e;
	bus_t b;
	const char *rom_path;
	double t0, t1, t_interp, t_jit;
	int i, n_runs = N_RUNS_DEF;

	rom_path = argv[1] != NULL ? argv[1] : DEFAULT_ROM;
	if (argv[2] != NULL) {
		n_runs = atoi(argv[2]);
		if (n_runs < 1)
			n_runs = N_RUNS_DEF;
	}

	rk65c02_loglevel_set(LOG_WARN);

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	e.regs.PC = ROM_LOAD_ADDR;

	if (!bus_load_file(e.bus, ROM_LOAD_ADDR, rom_path)) {
		fprintf(stderr, "bench_emulation: cannot load ROM: %s\n", rom_path);
		bus_finish(&b);
		return 1;
	}

	/* Run with interpreter (JIT disabled), n_runs times */
	rk65c02_jit_enable(&e, false);
	rk65c02_jit_flush(&e);
	t0 = now_sec();
	for (i = 0; i < n_runs; i++) {
		e.regs.PC = ROM_LOAD_ADDR;
		e.state = RUNNING;
		rk65c02_start(&e);
		if (e.state != STOPPED) {
			fprintf(stderr, "bench_emulation: interpreter run %d did not stop (state=%d)\n",
			    i, (int)e.state);
			bus_finish(&b);
			return 1;
		}
	}
	t1 = now_sec();
	t_interp = t1 - t0;

	/* Run with JIT, n_runs times */
	rk65c02_jit_enable(&e, true);
	rk65c02_jit_flush(&e);
	t0 = now_sec();
	for (i = 0; i < n_runs; i++) {
		e.regs.PC = ROM_LOAD_ADDR;
		e.state = RUNNING;
		rk65c02_start(&e);
		if (e.state != STOPPED) {
			fprintf(stderr, "bench_emulation: JIT run %d did not stop (state=%d)\n",
			    i, (int)e.state);
			bus_finish(&b);
			return 1;
		}
	}
	t1 = now_sec();
	t_jit = t1 - t0;

	bus_finish(&b);

	printf("ROM: %s (%d runs each)\n", rom_path, n_runs);
	printf("interpreter: %.3f s (%.3f s/run)\n", t_interp, t_interp / n_runs);
	printf("JIT:         %.3f s (%.3f s/run)\n", t_jit, t_jit / n_runs);
	if (t_jit > 0)
		printf("speedup:     %.2f x\n", t_interp / t_jit);

	return 0;
}
