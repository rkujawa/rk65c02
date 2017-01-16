#include <atf-c.h>

#include <stdio.h>
#include <string.h>

#include "bus.h"

ATF_TC_WITHOUT_HEAD(bus__init);
ATF_TC_BODY(bus__init, tc)
{
	bus_t b;

	b = bus_init();

	ATF_CHECK(b.space != NULL);

	bus_finish(&b);
}

ATF_TC_WITHOUT_HEAD(bus__foo);
ATF_TC_BODY(bus__foo, tc)
{
	bus_t b;
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, bus__init);
	ATF_TP_ADD_TC(tp, bus__foo);

	return (atf_no_error());
}

