#include <atf-c.h>

#include <stdio.h>
#include <string.h>

#include "bus.h"
#include "rk65c02.h"

ATF_TC_WITHOUT_HEAD(emul_lda);
ATF_TC_BODY(emul_lda, tc)
{
	rk65c02emu_t e;
	bus_t b;

	b = bus_init();
	e = rk65c02_init(&b);

	e.regs.PC = 0;

	bus_write_1(&b, 0, 0xA9);
	bus_write_1(&b, 1, 0xAF);
	bus_write_1(&b, 2, 0xDB);

	rk65c02_start(&e);

	ATF_CHECK(e.regs.PC == 3);
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

	e.regs.PC = 0xC000;

	bus_write_1(&b, 0xC000, 0xEA);
	bus_write_1(&b, 0xC001, 0xDB);

	rk65c02_start(&e);

	ATF_CHECK(e.regs.PC == 0xC002);

	bus_finish(&b);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, emul_lda);
	ATF_TP_ADD_TC(tp, emul_nop);

	return (atf_no_error());
}

