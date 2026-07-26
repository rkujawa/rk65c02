/*
 * Standalone benchmark: run the Klaus Dormann functional ROMs with
 * interpreter and with JIT, report elapsed time and JIT statistics.
 * The functional ROMs contain real self-modifying code, so this is the
 * primary workload for evaluating SMC handling under JIT.
 *
 * Build: make bench_functional (functional_tests must be built)
 * Run:   ./bench_functional   (from the test/ directory)
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

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

#include "bus.h"
#include "device_ram.h"
#include "rk65c02.h"
#include "log.h"

#define ROM_LOAD_ADDR 0xC000

/* Success trap addresses, same as test_functional.c. */
#define PC_SUCCESS_6502_DECIMAL 0xC04B
#define PC_SUCCESS_6502_FUNCTIONAL 0xF089
#define PC_SUCCESS_65C02_EXTENDED 0xE111

struct functional_case {
	const char *rom_name;
	uint16_t success_pc;
	uint64_t poll_limit;
};

static const struct functional_case cases[] = {
	{ "functional_tests/6502_decimal_test.bin",
	    PC_SUCCESS_6502_DECIMAL, 600000000ULL },
	{ "functional_tests/6502_functional_test.bin",
	    PC_SUCCESS_6502_FUNCTIONAL, 6000000000ULL },
	{ "functional_tests/65C02_extended_opcodes_test.bin",
	    PC_SUCCESS_65C02_EXTENDED, 6000000000ULL },
};

struct functional_monitor {
	uint16_t success_pc;
	uint16_t last_pc;
	uint32_t stable_pc_count;
	uint32_t stable_threshold;
	uint64_t poll_count;
	uint64_t poll_limit;
	bool initialized;
	bool passed;
	bool finished;
};

static void
functional_tick(rk65c02emu_t *e, void *ctx)
{
	struct functional_monitor *m = ctx;
	uint16_t pc;

	pc = e->regs.PC;
	m->poll_count++;

	if (!(m->initialized)) {
		m->initialized = true;
		m->last_pc = pc;
		m->stable_pc_count = 1;
	} else if (pc == m->last_pc) {
		m->stable_pc_count++;
	} else {
		m->last_pc = pc;
		m->stable_pc_count = 1;
	}

	if (m->stable_pc_count >= m->stable_threshold) {
		m->finished = true;
		m->passed = (pc == m->success_pc);
		rk65c02_request_stop(e);
		return;
	}

	if (m->poll_count >= m->poll_limit) {
		m->finished = true;
		m->passed = false;
		rk65c02_request_stop(e);
	}
}

/* Tick interval for rk65c02_tick_set (0 = every check); argv[1]. */
static uint32_t tick_interval;

/* Returns elapsed seconds, negative on failure. */
static double
run_case(const struct functional_case *fcase, bool use_jit,
    rk65c02_jit_stats_t *stats, bool *stats_valid)
{
	rk65c02emu_t e;
	bus_t b;
	struct functional_monitor monitor = { 0 };
	double t0, t1;
	bool ok;

	b = bus_init();
	bus_device_add(&b, device_ram_init(0xDFFF), 0x0000);
	bus_device_add(&b, device_ram_init(0x2001), 0xDFFF);
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);
	rk65c02_jit_flush(&e);
	rk65c02_jit_stats_reset(&e);

	monitor.success_pc = fcase->success_pc;
	monitor.stable_threshold = 256;
	monitor.poll_limit = fcase->poll_limit;

	if (!bus_load_file(&b, 0x0000, fcase->rom_name)) {
		fprintf(stderr, "cannot load ROM: %s\n", fcase->rom_name);
		bus_finish(&b);
		return -1.0;
	}

	e.regs.PC = ROM_LOAD_ADDR;
	e.regs.SP = 0xFF;
	e.regs.A = 0x00;
	e.regs.X = 0x00;
	e.regs.Y = 0x00;
	rk65c02_tick_set(&e, functional_tick, tick_interval, &monitor);

	t0 = now_sec();
	rk65c02_start(&e);
	t1 = now_sec();

	rk65c02_tick_clear(&e);
	ok = (e.stopreason == HOST) && monitor.finished && monitor.passed;
	if (stats != NULL)
		*stats_valid = rk65c02_jit_stats_get(&e, stats);
	bus_finish(&b);

	if (!ok) {
		fprintf(stderr, "%s (%s): did not pass (terminal pc=$%04x)\n",
		    fcase->rom_name, use_jit ? "jit" : "interp",
		    monitor.last_pc);
		return -1.0;
	}
	return t1 - t0;
}

int
main(int argc, char **argv)
{
	size_t i;
	int rv = 0;

	if ((argc > 1) && (argv[1] != NULL))
		tick_interval = (uint32_t)strtoul(argv[1], NULL, 0);

	rk65c02_loglevel_set(LOG_ERROR);
	printf("tick interval: %u\n", (unsigned)tick_interval);

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		const struct functional_case *fc = &cases[i];
		rk65c02_jit_stats_t st = { 0 };
		bool st_valid = false;
		double t_interp, t_jit;

		t_interp = run_case(fc, false, NULL, NULL);
		t_jit = run_case(fc, true, &st, &st_valid);
		if ((t_interp < 0) || (t_jit < 0)) {
			rv = 1;
			continue;
		}
		printf("%s\n", fc->rom_name);
		printf("  interpreter: %.3f s\n", t_interp);
		printf("  JIT:         %.3f s (speedup %.2f x)\n", t_jit,
		    (t_jit > 0) ? t_interp / t_jit : 0.0);
		if (st_valid) {
			printf("  jit stats: write_events=%" PRIu64
			    " blocks_invalidated=%" PRIu64
			    " blocks_compiled=%" PRIu64 "\n"
			    "             blocks_executed=%" PRIu64
			    " pages_demoted=%" PRIu64
			    " run_jit_disables=%" PRIu64 "\n",
			    st.write_events, st.blocks_invalidated,
			    st.blocks_compiled, st.blocks_executed,
			    st.pages_demoted, st.run_jit_disables);
			if (st.blocks_executed > 0)
				printf("             insns_executed=%" PRIu64
				    " avg insns/block=%.2f jit MIPS=%.1f\n",
				    st.insns_executed,
				    (double)st.insns_executed / (double)st.blocks_executed,
				    (t_jit > 0) ? (double)st.insns_executed / t_jit / 1e6 : 0.0);
		}
	}

	return rv;
}
