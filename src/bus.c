#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>

#include <sys/types.h>

#include <utlist.h>

#include "bus.h"

#include "device_ram.h"

#define RK65C02_BUS_SIZE	64*1024

void
bus_device_add(bus_t *b, device_t *d, uint16_t addr)
{
	device_mapping_t *dm;

	dm = (device_mapping_t *) malloc(sizeof(device_mapping_t));

	dm->dev = d;
	/* TODO: check if addr + size is not bigger than RK65C02_BUS_SIZE */
	dm->addr = addr;

	LL_APPEND((b->dm_head), dm);
}

void
bus_device_dump(bus_t *b)
{
	device_mapping_t *dm;
	device_t *d;

	LL_FOREACH(b->dm_head, dm) {
		d = dm->dev;
		printf("@ %x size %x - %s\n", dm->addr, d->size, d->name);
		/* TODO: device specific info */
	}
}

uint8_t
bus_read_1(bus_t *t, uint16_t addr)
{
	uint8_t val;
	device_mapping_t *dm;
	device_t *d;

	LL_FOREACH(t->dm_head, dm) {
		d = dm->dev;

		if (dm->addr == 0)
			val = d->read_1(d, addr);
		/* 
		 * else
		 * Check if address is inside of given device range, calculate
		 * offset.
		 */	

	}
//	printf("bus READ @ %x value %x\n", addr, val); 
	return val;
}

void
bus_write_1(bus_t *t, uint16_t addr, uint8_t val)
{
	device_mapping_t *dm;
	device_t *d;

	LL_FOREACH(t->dm_head, dm) {
		d = dm->dev;

		if (dm->addr == 0)
			d->write_1(d, addr, val);
		/* 
		 * else
		 * Check if address is inside of given device range, calculate
		 * offset.
		 */	

	}
//	printf("bus WRITE @ %x value %x\n", addr, val); 
}

bus_t
bus_init()
{
	bus_t t;

	t.dm_head = NULL;

	return t;	
}

bus_t
bus_init_with_default_devs()
{
	bus_t t;

	t = bus_init();

	bus_device_add(&t, device_ram_init(), 0x0);

	return t;
}

bool
bus_load_buf(bus_t *t, uint16_t addr, uint8_t *buf, uint16_t bufsize)
{
	uint16_t i;

	i = 0;

	// XXX: add sanity checks 

	while (i < bufsize) {
		bus_write_1(t, addr+i, buf[i]); // XXX: overflow addr
		i++;
	}

	return true;
}

/* TODO: should be moved to ram/rom specific devs */
bool
bus_load_file(bus_t *t, uint16_t addr, const char *filename)
{
	int fd;
	uint8_t data;

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		perror("Problem while trying to open file");
		return false;
	}

	while ((read(fd, &data, 1)) > 0) {
		bus_write_1(t, addr++, data); // XXX: overflow addr
	}

	close(fd);

	return true;
}

void
bus_finish(bus_t *t)
{
	assert(t != NULL);

	/* TODO: foreach devices free 'em */
}

