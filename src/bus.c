#include <stdint.h>
#include <stdlib.h>

#include <assert.h>
#include <string.h>

#define RK65C02_BUS_SIZE	64*1024

struct bus_tag {
	uint8_t *space;
};

typedef struct bus_tag bus_t;

uint8_t
bus_read_1(bus_t *t, uint16_t addr)
{
	return t->space[addr];
}

void
bus_write_1(bus_t *t, uint16_t addr, uint8_t val)
{
	t->space[addr] = val;
}

bus_t
bus_init()
{
	bus_t t;

	t.space = (uint8_t *) malloc(RK65C02_BUS_SIZE);

	assert(t.space != NULL);

	memset(t.space, 0, RK65C02_BUS_SIZE);

	return t;	
}

void
bus_finish(bus_t *t)
{
	assert(t != NULL);
	assert(t->space != NULL);

	free(t->space);
}

