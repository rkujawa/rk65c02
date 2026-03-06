#include <atf-c.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "bus.h"
#include "rk65c02.h"
#include "assembler.h"
#include "instruction.h"
#include "utils.h"

static void do_assemble_single_buf(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;	
	bus_t b;
	uint8_t *asmbuf;
	uint8_t bsize;
	uint16_t caddr;

	b = bus_init_with_default_devs();
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);

	caddr = ROM_LOAD_ADDR;	
	e.regs.PC = ROM_LOAD_ADDR;

	ATF_REQUIRE(assemble_single_buf_implied(&asmbuf, &bsize, "nop"));
	ATF_CHECK(asmbuf[0] == 0xEA);	/* check if nop really */
	ATF_REQUIRE(bus_load_buf(&b, caddr, asmbuf, bsize));
	caddr += bsize;

	ATF_REQUIRE(assemble_single_buf(&asmbuf, &bsize, "lda", IMMEDIATE, 0xAA, 0));
	ATF_CHECK(asmbuf[0] == 0xA9);	/* check if lda really */
	ATF_CHECK(asmbuf[1] == 0xAA);	/* check the operand */
	ATF_REQUIRE(bus_load_buf(&b, caddr, asmbuf, bsize));
	caddr += bsize;

	ATF_REQUIRE(assemble_single_buf_implied(&asmbuf, &bsize, "stp"));
	ATF_CHECK(asmbuf[0] == 0xDB);	/* check if stp really */
	ATF_REQUIRE(bus_load_buf(&b, caddr, asmbuf, bsize));
	caddr += bsize;

	rk65c02_start(&e);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(assemble_single_buf, do_assemble_single_buf)

static void do_assemble_single(const atf_tc_t *tc, bool use_jit)
{
	rk65c02emu_t e;	
	bus_t b;
	assembler_t a;

	b = bus_init_with_default_devs();
	a = assemble_init(&b, ROM_LOAD_ADDR);
	e = rk65c02_init(&b);
	rk65c02_jit_enable(&e, use_jit);

	e.regs.PC = ROM_LOAD_ADDR;

	ATF_REQUIRE(assemble_single_implied(&a, "nop"));
	ATF_REQUIRE(assemble_single(&a, "stp", IMPLIED, 0, 0));

	ATF_CHECK(bus_read_1(&b, ROM_LOAD_ADDR) == 0xEA);
	ATF_CHECK(bus_read_1(&b, ROM_LOAD_ADDR + 1) == 0xDB);	

	rk65c02_start(&e);
	bus_finish(&b);
}
ATF_TC_JIT_VARIANTS(assemble_single, do_assemble_single)

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, assemble_single_buf);
	ATF_TP_ADD_TC(tp, assemble_single_buf_jit);
	ATF_TP_ADD_TC(tp, assemble_single);
	ATF_TP_ADD_TC(tp, assemble_single_jit);

	return (atf_no_error());
}

