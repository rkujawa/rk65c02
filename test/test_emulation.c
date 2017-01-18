#include <atf-c.h>

#include <stdio.h>
#include <string.h>

#include "bus.h"
#include "rk65c02.h"

#define ROM_LOAD_ADDR 0xC000

ATF_TC_WITHOUT_HEAD(emul_lda);
ATF_TC_BODY(emul_lda, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	e.regs.PC = ROM_LOAD_ADDR;

	bus_write_1(&b, ROM_LOAD_ADDR, 0xA9);
	bus_write_1(&b, ROM_LOAD_ADDR+1, 0xAF);
	bus_write_1(&b, ROM_LOAD_ADDR+2, 0xDB);

	rk65c02_start(&e);

	ATF_CHECK(e.regs.PC == ROM_LOAD_ADDR+3);
	ATF_CHECK(e.regs.A == 0xAF);

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

	bus_load_file(&b, 0xC000, "test_emulation_nop.rom");

	rk65c02_start(&e);

	ATF_CHECK(e.regs.PC == ROM_LOAD_ADDR+2);

	bus_finish(&b);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, emul_lda);
	ATF_TP_ADD_TC(tp, emul_nop);

	return (atf_no_error());
}

