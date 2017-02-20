#include <atf-c.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <utlist.h>

#include "bus.h"
#include "rk65c02.h"
#include "instruction.h"
#include "debug.h"
#include "utils.h"

ATF_TC_WITHOUT_HEAD(breakpoint);
ATF_TC_BODY(breakpoint, tc)
{
	rk65c02emu_t e;	
	bus_t b;
	assembler_t a;

	b = bus_init_with_default_devs();
	a = assemble_init(&b, ROM_LOAD_ADDR);
	e = rk65c02_init(&b);

	e.regs.PC = ROM_LOAD_ADDR;

	ATF_REQUIRE(assemble_single_implied(&a, "nop")); /* 0xC000 */
	ATF_REQUIRE(assemble_single_implied(&a, "nop"));
	ATF_REQUIRE(assemble_single_implied(&a, "nop")); /* 0xC002 */
	ATF_REQUIRE(assemble_single_implied(&a, "nop"));
	ATF_REQUIRE(assemble_single(&a, "stp", IMPLIED, 0, 0));

	ATF_REQUIRE(debug_breakpoint_add(&e, 0xC002));

	rk65c02_start(&e);

	ATF_CHECK(e.state == STOPPED);
	ATF_CHECK(e.stopreason == BREAKPOINT);
	ATF_CHECK(e.regs.PC == 0xC002);

	ATF_REQUIRE(debug_breakpoint_remove(&e, 0xC002));

	rk65c02_start(&e);
}

ATF_TC_WITHOUT_HEAD(trace);
ATF_TC_BODY(trace, tc)
{
	rk65c02emu_t e;	
	bus_t b;
	assembler_t a;
	trace_t *tr;
	int i;

	b = bus_init_with_default_devs();
	a = assemble_init(&b, ROM_LOAD_ADDR);
	e = rk65c02_init(&b);

	e.regs.PC = ROM_LOAD_ADDR;
	debug_trace_set(&e, true);

	ATF_REQUIRE(assemble_single_implied(&a, "nop")); /* 0xC000 */
	ATF_REQUIRE(assemble_single_implied(&a, "nop"));
	ATF_REQUIRE(assemble_single_implied(&a, "nop")); /* 0xC002 */
	ATF_REQUIRE(assemble_single_implied(&a, "nop"));
	ATF_REQUIRE(assemble_single(&a, "stp", IMPLIED, 0, 0));

	rk65c02_start(&e);

	debug_trace_print_all(&e);

	i = 0;
	DL_FOREACH(e.trace_head, tr) {
		if (i < 4)
			ATF_CHECK(tr->opcode == 0xEA);
		else
			ATF_CHECK(tr->opcode = 0xDB);
		i++;
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, breakpoint);
	ATF_TP_ADD_TC(tp, trace);

	return (atf_no_error());
}

