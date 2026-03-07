/*
 *      SPDX-License-Identifier: GPL-3.0-only
 *
 *      rk65c02
 *      Copyright (C) 2017-2021  Radoslaw Kujawa
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
static void bus_access_device_phys(bus_t *, uint32_t, device_t **, uint16_t *);

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
bus_device_add_phys(bus_t *b, device_t *d, uint32_t base)
{
	device_phys_mapping_t *dm;

	assert(b != NULL);
	assert(d != NULL);

	dm = (device_phys_mapping_t *) GC_MALLOC(sizeof(device_phys_mapping_t));
	assert(dm != NULL);
	dm->dev = d;
	dm->base = base;

	LL_APPEND((b->dm_phys_head), dm);

	rk65c02_log(LOG_DEBUG, "Bus phys mapping added: %x device %s size %x.",
	    (unsigned)base, d->name, d->size);
}

void
bus_device_dump(bus_t *b)
{
	device_mapping_t *dm;
	device_t *d;

	LL_FOREACH(b->dm_head, dm) {
		d = dm->dev;
		printf("@ %x size %x - %s\n", dm->addr, d->size, d->name);
		/* Additional per-device diagnostics can be added here if needed. */
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

static void
bus_access_device_phys(bus_t *t, uint32_t addr, device_t **d, uint16_t *off)
{
	device_phys_mapping_t *dm;

	*d = NULL;
	*off = 0;

	if (addr < RK65C02_BUS_SIZE)
		return;

	LL_FOREACH(t->dm_phys_head, dm) {
		if (addr >= dm->base && addr < dm->base + dm->dev->size) {
			*d = dm->dev;
			*off = (uint16_t)(addr - dm->base);
			return;
		}
	}
}

uint8_t
bus_read_1_phys(bus_t *t, uint32_t addr)
{
	uint8_t val;
	uint16_t off;
	device_t *d;

	if (addr < RK65C02_BUS_SIZE)
		return bus_read_1(t, (uint16_t)addr);

	bus_access_device_phys(t, addr, &d, &off);
	if (d == NULL)
		return 0xFF;
	val = d->read_1(d, off);
	if (t->access_debug)
		rk65c02_log(LOG_DEBUG, "bus READ phys @ %x (off %x) value %x\n",
		    (unsigned)addr, off, val);
	return val;
}

void
bus_write_1_phys(bus_t *t, uint32_t addr, uint8_t val)
{
	uint16_t off;
	device_t *d;

	if (addr < RK65C02_BUS_SIZE) {
		bus_write_1(t, (uint16_t)addr, val);
		return;
	}
	bus_access_device_phys(t, addr, &d, &off);
	if (d == NULL) {
		if (t->access_debug)
			rk65c02_log(LOG_DEBUG, "unmapped bus WRITE phys @ %x\n",
			    (unsigned)addr);
		return;
	}
	if (t->access_debug)
		rk65c02_log(LOG_DEBUG, "bus WRITE phys @ %x (off %x) value %x\n",
		    (unsigned)addr, off, val);
	d->write_1(d, off, val);
}

uint8_t
bus_read_1(bus_t *t, uint16_t addr)
{
	uint8_t val;
	uint16_t off;
	device_t *d;

	bus_access_device(t, addr, &d, &off);

	if (d == NULL)
		return 0xFF; /* Simulate floating bus lines on unmapped reads. */
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
	t.dm_phys_head = NULL;
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
	uint32_t end;

	assert(buf != NULL);
	assert(bufsize != 0);

	end = (uint32_t) addr + bufsize;
	if (end > RK65C02_BUS_SIZE) {
		rk65c02_log(LOG_ERROR, "bus_load_buf: address range %x..%x exceeds bus size.",
		    addr, (uint16_t) end);
		return false;
	}

	for (i = 0; i < bufsize; i++)
		bus_write_1(t, addr + i, buf[i]);

	return true;
}

/* Convenience loader for RAM-backed mappings used by tests/examples. */
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
		if (addr >= RK65C02_BUS_SIZE) {
			rk65c02_log(LOG_ERROR, "bus_load_file: address overflow at %x.", addr);
			close(fd);
			return false;
		}
		bus_write_1(t, addr++, data);
	}

	close(fd);

	return true;
}

bool
bus_load_file_phys(bus_t *t, uint32_t addr, const char *filename)
{
	int fd;
	uint8_t data;

	if (addr < RK65C02_BUS_SIZE) {
		rk65c02_log(LOG_ERROR, "bus_load_file_phys: use bus_load_file for addr < 64K.");
		return false;
	}

	rk65c02_log(LOG_DEBUG, "Loading file %s at phys %x.", filename, (unsigned)addr);

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		rk65c02_log(LOG_ERROR, "Problem while trying to open file: %s",
		    strerror(errno));
		return false;
	}

	while ((read(fd, &data, 1)) > 0) {
		bus_write_1_phys(t, addr, data);
		addr++;
	}

	close(fd);

	return true;
}

void
bus_finish(bus_t *t)
{
	device_mapping_t *dm;
	device_phys_mapping_t *dm_phys;
	device_t *d;

	assert(t != NULL);

	LL_FOREACH(t->dm_head, dm) {
		d = dm->dev;
		if ((d->finish) != NULL)
			d->finish(d);
	}
	LL_FOREACH(t->dm_phys_head, dm_phys) {
		d = dm_phys->dev;
		if ((d->finish) != NULL)
			d->finish(d);
	}
}

