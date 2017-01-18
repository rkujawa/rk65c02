#include <atf-c.h>

#include <stdio.h>
#include <string.h>

#include "bus.h"

ATF_TC_WITHOUT_HEAD(bus_init);
ATF_TC_BODY(bus_init, tc)
{
	bus_t b;

	b = bus_init();

	ATF_CHECK(b.space != NULL);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(bus_load_file);
ATF_TC_BODY(bus_load_file, tc)
{
	bus_t b;

	b = bus_init();

	ATF_REQUIRE(bus_load_file(&b, 0xC000, "test_emulation_nop.rom"));

	ATF_CHECK(b.space[0xC000] == 0xEA);

	bus_finish(&b);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, bus_init);
	ATF_TP_ADD_TC(tp, bus_load_file);

	return (atf_no_error());
}

