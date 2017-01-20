#include <atf-c.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "bus.h"
#include "rk65c02.h"

#define ROM_LOAD_ADDR 0xC000

bool rom_start(rk65c02emu_t *, const char *);

bool
rom_start(rk65c02emu_t *e, const char *name)
{
	e->regs.PC = ROM_LOAD_ADDR;
	if(!bus_load_file(e->bus, ROM_LOAD_ADDR, name))
		return false;
	rk65c02_start(e);

	return true;
}

ATF_TC_WITHOUT_HEAD(emul_lda);
ATF_TC_BODY(emul_lda, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	/* LDA immediate */
	ATF_REQUIRE(rom_start(&e, "test_emulation_lda_imm.rom"));
/*	ATF_CHECK(e.state == STOPPED);  // separate test case for states? */
	ATF_CHECK(e.regs.PC == ROM_LOAD_ADDR+3); // separate test case for PC? */
	ATF_CHECK(e.regs.A == 0xAF);

	/* LDA zero page */
	bus_write_1(&b, 0x10, 0xAE);
	ATF_REQUIRE(rom_start(&e, "test_emulation_lda_zp.rom"));
	ATF_CHECK(e.regs.A == 0xAE);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_and);
ATF_TC_BODY(emul_and, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	/* AND immediate */
	e.regs.A = 0xFF;
	ATF_REQUIRE(rom_start(&e, "test_emulation_and_imm.rom"));
	ATF_CHECK(e.regs.A == 0xAA);

	/* AND zero page */
/*	bus_write_1(&b, 0x10, 0xAE);
	ATF_REQUIRE(rom_start(&e, "test_emulation_and_zp.rom"));
	ATF_CHECK(e.regs.A == 0xAE);*/

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(emul_nop);
ATF_TC_BODY(emul_nop, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	e.regs.PC = ROM_LOAD_ADDR;

	bus_load_file(&b, ROM_LOAD_ADDR, "test_emulation_nop.rom");

	rk65c02_start(&e);

	ATF_CHECK(e.regs.PC == ROM_LOAD_ADDR+2);

	bus_finish(&b);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, emul_and);
	ATF_TP_ADD_TC(tp, emul_lda);
	ATF_TP_ADD_TC(tp, emul_nop);

	return (atf_no_error());
}

