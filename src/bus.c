#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
//#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
//#include <string.h>
//#include <fcntl.h>

#include <sys/types.h>

#include "bus.h"

#define RK65C02_BUS_SIZE	64*1024

uint8_t
bus_read_1(bus_t *t, uint16_t addr)
{
	uint8_t val;
//	val = t->space[addr];
/*	printf("bus READ @ %x value %x\n", addr, val); */
	return val;
}

void
bus_write_1(bus_t *t, uint16_t addr, uint8_t val)
{
/*	printf("bus WRITE @ %x value %x\n", addr, val); */
//	t->space[addr] = val;
}

bus_t
bus_init()
{
	bus_t t;

//	t.space = (uint8_t *) malloc(RK65C02_BUS_SIZE);

//	assert(t.space != NULL);

//	memset(t.space, 0, RK65C02_BUS_SIZE);

	return t;	
}
/*
bool
bus_load_buf(bus_t *t, uint16_t addr, uint8_t *buf, uint16_t bufsize)
{
	uint16_t i;

	i = 0;

	// XXX: add sanity checks 

	while (i < bufsize) {
		t->space[addr+i] = buf[i]; // XXX: overflow addr
		i++;
	}

	return true;
}

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
		t->space[addr++] = data; // XXX: overflow addr
	}

	close(fd);

	return true;
}
*/
void
bus_finish(bus_t *t)
{
	assert(t != NULL);
//	assert(t->space != NULL);

//	free(t->space);
}

