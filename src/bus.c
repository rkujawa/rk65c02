/*
 *      SPDX-License-Identifier: GPL-3.0-only
 *
 *      rk65c02
 *      Copyright (C) 2017-2019  Radoslaw Kujawa
 *
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, version 3 of the License.
 * 
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>

#include <gc/gc.h>
#include <utlist.h>

#include "bus.h"
#include "log.h"

#include "device_ram.h"

#define RK65C02_BUS_SIZE	64*1024

static void bus_access_device(bus_t *, uint16_t, device_t **, uint16_t *);

void
bus_device_add(bus_t *b, device_t *d, uint16_t addr)
{
	device_mapping_t *dm;

	if ((addr + d->size) > RK65C02_BUS_SIZE) {
		rk65c02_log(LOG_ERROR,
		    "Bus mapping for %s at %x, size %x exceeding bus size.",
		    d->name, addr, d->size);
		return;
	}

	dm = (device_mapping_t *) GC_MALLOC(sizeof(device_mapping_t));
	assert(dm != NULL);

	dm->dev = d;
	dm->addr = addr;

	LL_APPEND((b->dm_head), dm);

	rk65c02_log(LOG_DEBUG, "Bus mapping added: %x device %s size %x.",
	    addr, d->name, d->size);
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

/*
 * Determine which device is accessed and at what offset. The d argument is 
 * filled with pointer to accessed device, off with offset.
 * Returns device NULL when hitting unmapped space.
 */
static void
bus_access_device(bus_t *t, uint16_t addr, device_t **d, uint16_t *off)
{
	uint16_t doff;
	device_mapping_t *dm;
	device_t *dtmp;

	doff = 0;
	*d = NULL;

	LL_FOREACH(t->dm_head, dm) {
		dtmp = dm->dev;
		if ( (addr >= dm->addr) && (addr < (dm->addr + dtmp->size)) ) {
			*d = dtmp;
			doff = dm->addr;
		}
	}

	if (*d == NULL) {
		rk65c02_log(LOG_WARN, "Hitting unmapped bus space @ %x!", addr);
		return;
	}

	*off = addr - doff;
}

uint8_t
bus_read_1(bus_t *t, uint16_t addr)
{
	uint8_t val;
	uint16_t off;
	device_t *d;

	bus_access_device(t, addr, &d, &off);

	if (d == NULL)
		return 0xFF; /* simulate floting pins */
	else
		val = d->read_1(d, off);

	if (t->access_debug)
		rk65c02_log(LOG_DEBUG, "bus READ @ %x (off %x) value %x\n",
		    addr, off, val); 

	return val;
}

void
bus_write_1(bus_t *t, uint16_t addr, uint8_t val)
{
	uint16_t off;
	device_t *d;

	off = 0;

	bus_access_device(t, addr, &d, &off);

	if (d == NULL) {
		if (t->access_debug)
			rk65c02_log(LOG_DEBUG, "unmapped bus WRITE @ %x (off %x) value %x\n",
			    addr, off, val);
		return;
	}

	if (t->access_debug)
		rk65c02_log(LOG_DEBUG, "bus WRITE @ %x (off %x) value %x\n",
		    addr, off, val); 

	d->write_1(d, off, val);
}

bus_t
bus_init()
{
	bus_t t;

	t.dm_head = NULL;
	t.access_debug = false;

	return t;	
}

bus_t
bus_init_with_default_devs()
{
	bus_t t;

	t = bus_init();

	bus_device_add(&t, device_ram_init(0xDFFF), 0x0);

	return t;
}

bool
bus_load_buf(bus_t *t, uint16_t addr, uint8_t *buf, uint16_t bufsize)
{
	uint16_t i;

	i = 0;

	assert(buf != NULL);
	assert(bufsize != 0);

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

	rk65c02_log(LOG_DEBUG, "Loading file %s at %x.", filename, addr);

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		rk65c02_log(LOG_ERROR, "Problem while trying to open file: %s",
		    strerror(errno));
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
	device_mapping_t *dm;
	device_t *d;

	assert(t != NULL);

        LL_FOREACH(t->dm_head, dm) {
		d = dm->dev;
		if ((d->finish) != NULL)
			d->finish(d);
        }
}

