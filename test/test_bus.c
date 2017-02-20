#include <atf-c.h>

#include <stdio.h>
#include <string.h>

#include <utlist.h>

#include "rk65c02.h"
#include "bus.h"
#include "utils.h"

ATF_TC_WITHOUT_HEAD(bus_init);
ATF_TC_BODY(bus_init, tc)
{
	bus_t b;

	b = bus_init();

	ATF_CHECK(b.dm_head == NULL);

	bus_finish(&b);
}


ATF_TC_WITHOUT_HEAD(bus_init_default);
ATF_TC_BODY(bus_init_default, tc)
{
	bus_t b;
	device_mapping_t *dm;
	device_t *d, *d_ram;

	b = bus_init_with_default_devs();

	bus_device_dump(&b);

	LL_FOREACH(b.dm_head, dm) {
		d = dm->dev;

		if (dm->addr == 0) 
			d_ram = d;
	}

	ATF_CHECK(strcmp("RAM", d_ram->name) == 0);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(bus_load_file);
ATF_TC_BODY(bus_load_file, tc)
{
	bus_t b;
	const char *rompath;

	b = bus_init_with_default_devs();

	rompath = rom_path("test_emulation_nop.rom", tc);

	ATF_REQUIRE(bus_load_file(&b, 0xC000, rompath));

	ATF_CHECK(bus_read_1(&b, 0xC000) == 0xEA);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(bus_load_buf);
ATF_TC_BODY(bus_load_buf, tc)
{
	bus_t b;

	uint8_t buf[] = { 0xEA, 0xDB };

	b = bus_init_with_default_devs();

	ATF_REQUIRE(bus_load_buf(&b, 0xC000, buf, 2));

	ATF_CHECK(bus_read_1(&b, 0xC000) == 0xEA);
	ATF_CHECK(bus_read_1(&b, 0xC001) == 0xDB);

	bus_finish(&b);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, bus_init);
	ATF_TP_ADD_TC(tp, bus_init_default);
	ATF_TP_ADD_TC(tp, bus_load_file);
	ATF_TP_ADD_TC(tp, bus_load_buf);

	return (atf_no_error());
}

