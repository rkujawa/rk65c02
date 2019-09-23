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
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <gc/gc.h>

#include "bus.h"
#include "device.h"

uint8_t device_ram_read_1(void *, uint16_t);
void device_ram_write_1(void *, uint16_t, uint8_t);

uint8_t
device_ram_read_1(void *vd, uint16_t offset)
{
	device_t *d;
	uint8_t *ram;

	d = (device_t *) vd;
	ram = d->aux;

	return ram[offset];
}

void
device_ram_write_1(void *vd, uint16_t offset, uint8_t val)
{
	device_t *d;
	uint8_t *ram;

	d = (device_t *) vd;
	ram = d->aux;

	ram[offset] = val;
}

device_t *
device_ram_init(uint16_t size)
{
	device_t *d;

	d = (device_t *) GC_MALLOC(sizeof(device_t));
	assert(d != NULL);

	d->name = "RAM";
	d->size = size;

	d->read_1 = device_ram_read_1;
	d->write_1 = device_ram_write_1;
	d->finish = NULL;

	d->aux = GC_MALLOC(size);
	assert(d->aux != NULL);

	memset(d->aux, 0, size);

	return d;
}


