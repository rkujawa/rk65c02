#include <atf-c.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "bus.h"
#include "rk65c02.h"
#include "instruction.h"
#include "utils.h"

ATF_TC_WITHOUT_HEAD(asm_single);
ATF_TC_BODY(asm_single, tc)
{
	rk65c02emu_t e;	
	bus_t b;
	uint8_t *asmbuf;
	uint8_t bsize;
	uint16_t caddr;

	b = bus_init();
	e = rk65c02_init(&b);

	caddr = ROM_LOAD_ADDR;	
	e.regs.PC = ROM_LOAD_ADDR;

	ATF_REQUIRE(assemble_single_implied(&asmbuf, &bsize, "nop"));
	ATF_CHECK(asmbuf[0] == 0xEA);	/* check if nop really */
	ATF_REQUIRE(bus_load_buf(&b, caddr, asmbuf, bsize));
	free(asmbuf);
	caddr += bsize;

	ATF_REQUIRE(assemble_single(&asmbuf, &bsize, "lda", IMMEDIATE, 0xAA, 0));
	ATF_CHECK(asmbuf[0] == 0xA9);	/* check if lda really */
	ATF_CHECK(asmbuf[1] == 0xAA);	/* check the operand */
	ATF_REQUIRE(bus_load_buf(&b, caddr, asmbuf, bsize));
	free(asmbuf);
	caddr += bsize;

	ATF_REQUIRE(assemble_single_implied(&asmbuf, &bsize, "stp"));
	ATF_CHECK(asmbuf[0] == 0xDB);	/* check if stp really */
	ATF_REQUIRE(bus_load_buf(&b, caddr, asmbuf, bsize));
	free(asmbuf);
	caddr += bsize;

	rk65c02_start(&e);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, asm_single);

	return (atf_no_error());
}

