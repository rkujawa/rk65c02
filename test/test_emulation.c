#include <atf-c.h>

#include <stdio.h>
#include <string.h>

#include "bus.h"
#include "rk65c02.h"

ATF_TC_WITHOUT_HEAD(emulation_nop);
ATF_TC_BODY(emulation_nop, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	e.regs.PC = 0;

	bus_write_1(&b, 0, 0xEA);
	bus_write_1(&b, 1, 0xDB);

	rk65c02_start(&e);

	ATF_CHECK(e.regs.PC == 2);

	bus_finish(&b);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, emulation_nop);

	return (atf_no_error());
}

