#include <atf-c.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bus.h"
#include "device_ram.h"
#include "log.h"
#include "rk65c02.h"
#include "utils.h"

/*
 * Success trap addresses come from generated functional_tests listings:
 * - 6502_decimal_test.lst: DONE/end_of_test self-loop at $C04B
 * - 6502_functional_test.lst: success self-loop at $F069
 * - 65C02_extended_opcodes_test.lst: success self-loop at $E0F1
 */
#define PC_SUCCESS_6502_DECIMAL 0xC04B
#define PC_SUCCESS_6502_FUNCTIONAL 0xF089
#define PC_SUCCESS_65C02_EXTENDED 0xE111

enum functional_outcome {
	FUNCTIONAL_OUTCOME_UNKNOWN = 0,
	FUNCTIONAL_OUTCOME_PASS,
	FUNCTIONAL_OUTCOME_FAIL,
	FUNCTIONAL_OUTCOME_TIMEOUT
};

struct functional_monitor {
	uint16_t success_pc;
	uint16_t last_pc;
	uint16_t terminal_pc;
	uint8_t test_case_value;
	uint32_t stable_pc_count;
	uint32_t stable_threshold;
	uint64_t poll_count;
	uint64_t poll_limit;
	bool initialized;
	enum functional_outcome outcome;
};

struct functional_case {
	const char *rom_name;
	uint16_t success_pc;
	uint64_t poll_limit;
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
		m->outcome = (pc == m->success_pc) ? FUNCTIONAL_OUTCOME_PASS
		    : FUNCTIONAL_OUTCOME_FAIL;
		m->terminal_pc = pc;
		m->test_case_value = bus_read_1(e->bus, 0x0200);
		rk65c02_request_stop(e);
		return;
	}

	if (m->poll_count >= m->poll_limit) {
		m->outcome = FUNCTIONAL_OUTCOME_TIMEOUT;
		m->terminal_pc = pc;
		rk65c02_request_stop(e);
	}
}

static void
run_functional_case(const atf_tc_t *tc, const struct functional_case *fcase,
    bool use_jit)
{
	rk65c02emu_t e;
	bus_t b;
	const char *path;
	struct functional_monitor monitor;

	b = bus_init();
	bus_device_add(&b, device_ram_init(0xDFFF), 0x0000);
	bus_device_add(&b, device_ram_init(0x2001), 0xDFFF);
	e = rk65c02_init(&b);
	rk65c02_loglevel_set(LOG_NOTHING);
	rk65c02_jit_enable(&e, use_jit);
	rk65c02_jit_flush(&e);

	monitor.success_pc = fcase->success_pc;
	monitor.last_pc = 0;
	monitor.terminal_pc = 0;
	monitor.test_case_value = 0;
	monitor.stable_pc_count = 0;
	monitor.stable_threshold = 256;
	monitor.poll_count = 0;
	monitor.poll_limit = fcase->poll_limit;
	monitor.initialized = false;
	monitor.outcome = FUNCTIONAL_OUTCOME_UNKNOWN;

	path = rom_path(fcase->rom_name, tc);
	ATF_REQUIRE_MSG(bus_load_file(&b, 0x0000, path),
	    "Failed to load functional ROM %s", path);
	ATF_CHECK_MSG(bus_read_1(&b, 0x0200) == 0x00,
	    "Expected test_case at $0200 to start at zero, got %#x",
	    bus_read_1(&b, 0x0200));

	e.regs.PC = ROM_LOAD_ADDR;
	e.regs.SP = 0xFF;
	e.regs.A = 0x00;
	e.regs.X = 0x00;
	e.regs.Y = 0x00;
	rk65c02_tick_set(&e, functional_tick, 0, &monitor);
	rk65c02_start(&e);
	rk65c02_tick_clear(&e);

	ATF_CHECK_MSG(e.stopreason == HOST,
	    "Unexpected stop reason for %s: %s",
	    fcase->rom_name, rk65c02_stop_reason_string(e.stopreason));
	ATF_CHECK_MSG(monitor.outcome == FUNCTIONAL_OUTCOME_PASS,
	    "Functional test %s did not pass (outcome=%d, terminal_pc=$%04x, polls=%llu, "
	    "success_pc=$%04x, test_case=$%02x, A=$%02x X=$%02x Y=$%02x P=$%02x SP=$%02x)",
	    fcase->rom_name, monitor.outcome,
	    monitor.terminal_pc, (unsigned long long)monitor.poll_count,
	    monitor.success_pc, monitor.test_case_value,
	    e.regs.A, e.regs.X, e.regs.Y, e.regs.P, e.regs.SP);

	bus_finish(&b);
}

static void
do_functional_decimal(const atf_tc_t *tc, bool use_jit)
{
	const struct functional_case fcase = {
		.rom_name = "functional_tests/6502_decimal_test.bin",
		.success_pc = PC_SUCCESS_6502_DECIMAL,
		.poll_limit = 600000000ULL,
	};

	run_functional_case(tc, &fcase, use_jit);
}
ATF_TC_JIT_VARIANTS(functional_decimal, do_functional_decimal)

static void
do_functional_6502(const atf_tc_t *tc, bool use_jit)
{
	const struct functional_case fcase = {
		.rom_name = "functional_tests/6502_functional_test.bin",
		.success_pc = PC_SUCCESS_6502_FUNCTIONAL,
		.poll_limit = 6000000000ULL,
	};

	run_functional_case(tc, &fcase, use_jit);
}
ATF_TC_JIT_VARIANTS(functional_6502, do_functional_6502)

static void
do_functional_65c02_extended(const atf_tc_t *tc, bool use_jit)
{
	const struct functional_case fcase = {
		.rom_name = "functional_tests/65C02_extended_opcodes_test.bin",
		.success_pc = PC_SUCCESS_65C02_EXTENDED,
		.poll_limit = 6000000000ULL,
	};

	run_functional_case(tc, &fcase, use_jit);
}
ATF_TC_JIT_VARIANTS(functional_65c02_extended, do_functional_65c02_extended)

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, functional_decimal);
	ATF_TP_ADD_TC(tp, functional_decimal_jit);
	ATF_TP_ADD_TC(tp, functional_6502);
	ATF_TP_ADD_TC(tp, functional_6502_jit);
	ATF_TP_ADD_TC(tp, functional_65c02_extended);
	ATF_TP_ADD_TC(tp, functional_65c02_extended_jit);
	return (atf_no_error());
}
