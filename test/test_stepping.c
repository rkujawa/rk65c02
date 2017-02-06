#include <atf-c.h>

#include <stdio.h>

#include "bus.h"
#include "rk65c02.h"
#include "utils.h"

ATF_TC_WITHOUT_HEAD(step1);
ATF_TC_BODY(step1, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	e.regs.PC = ROM_LOAD_ADDR;

	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR,
	    rom_path("test_stepping_step1.rom", tc)));

	rk65c02_step(&e, 1);
	ATF_CHECK(e.regs.PC == ROM_LOAD_ADDR+1);
	rk65c02_step(&e, 1);
	ATF_CHECK(e.regs.PC == ROM_LOAD_ADDR+2);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, step1);

	return (atf_no_error());

}

