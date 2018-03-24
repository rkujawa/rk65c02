#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

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

	d = (device_t *) malloc(sizeof(device_t));

	assert(d != NULL);

	d->name = "RAM";
	d->size = size;

	d->read_1 = device_ram_read_1;
	d->write_1 = device_ram_write_1;

	d->aux = malloc(size);
	memset(d->aux, 0, size);

	return d;
}

void
device_ram_finish(device_t *d)
{
	free(d->aux);
}

