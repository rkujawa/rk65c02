#include <atf-c.h>

#include <stdio.h>
#include <string.h>

#include "rk65c02.h"
#include "bus.h"
#include "utils.h"

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
	const char *rompath;

	b = bus_init();

	rompath = rom_path("test_emulation_nop.rom", tc);

	ATF_REQUIRE(bus_load_file(&b, 0xC000, rompath));

	ATF_CHECK(b.space[0xC000] == 0xEA);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(bus_load_buf);
ATF_TC_BODY(bus_load_buf, tc)
{
	bus_t b;

	uint8_t buf[] = { 0xEA, 0xDB };

	b = bus_init();

	ATF_REQUIRE(bus_load_buf(&b, 0xC000, buf, 2));

	ATF_CHECK(b.space[0xC000] == 0xEA);
	ATF_CHECK(b.space[0xC001] == 0xDB);

	bus_finish(&b);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, bus_init);
	ATF_TP_ADD_TC(tp, bus_load_file);
	ATF_TP_ADD_TC(tp, bus_load_buf);

	return (atf_no_error());
}

