#include <atf-c.h>

#include <stdio.h>
#include <string.h>

#include <utlist.h>

#include "rk65c02.h"
#include "bus.h"
#include "utils.h"
#include "device_serial.h"

ATF_TC_WITHOUT_HEAD(device_serial_init);
ATF_TC_BODY(device_serial_init, tc)
{
	bus_t b;
	device_mapping_t *dm;
	device_t *d, *d_ser;

	b = bus_init_with_default_devs();

	bus_device_add(&b, device_serial_init(), 0xE000);

	bus_device_dump(&b);

	LL_FOREACH(b.dm_head, dm) {
		d = dm->dev;

		if (dm->addr == 0xE000)
			d_ser = d;
	}

	ATF_CHECK(strcmp("Serial", d_ser->name) == 0);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(device_serial_write1);
ATF_TC_BODY(device_serial_write1, tc)
{
	bus_t b;
	rk65c02emu_t e;

	b = bus_init_with_default_devs();

	bus_device_add(&b, device_serial_init(), 0xE000);
	bus_device_dump(&b);

	e = rk65c02_init(&b);

	ATF_REQUIRE(rom_start(&e, "test_device_serial_write1.rom", tc));

	// start thread reading from txpipe

	/* clean up serial etc. */
	bus_finish(&b);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, device_serial_init);
	ATF_TP_ADD_TC(tp, device_serial_write1);

	return (atf_no_error());
}

