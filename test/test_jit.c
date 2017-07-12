#include <atf-c.h>

#include <stdio.h>

#include "bus.h"
#include "rk65c02.h"
#include "utils.h"
#include "jit_lightning.h"

ATF_TC_WITHOUT_HEAD(jit_1);
ATF_TC_BODY(jit_1, tc)
{
//	rk65c02emu_t e;
	bus_t b;
	rkjit_t rj;
	rkjit_block_t jb;

	b = bus_init_with_default_devs();
	rj = rkjit_init(&b);
//	e = rk65c02_init(&b);
//	e.regs.PC = ROM_LOAD_ADDR;

	ATF_REQUIRE(bus_load_file(&b, ROM_LOAD_ADDR,
	    rom_path("test_emulation_inc_a.rom", tc)));

	jb = rkjit_block_recompile(&rj, ROM_LOAD_ADDR);

	jb.generated();

	//rkjit_finish(&rj);

}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, jit_1);

	return (atf_no_error());
}

