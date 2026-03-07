/**
 * @file bus.h
 * @brief 64K address bus abstraction and device mapping API.
 */
#ifndef _BUS_H_
#define _BUS_H_

#include <stdint.h>
#include <stdbool.h>

#include "device.h"

#define RK65C02_BUS_SIZE	64*1024

struct bus_tag {
	device_mapping_t *dm_head;

	bool access_debug;
};

typedef struct bus_tag bus_t;

/** Read one byte from bus address. */
uint8_t bus_read_1(bus_t *, uint16_t);
/** Write one byte to bus address. */
void bus_write_1(bus_t *, uint16_t, uint8_t);
/** Initialize an empty bus. */
bus_t bus_init(void);
/** Initialize bus with default memory/device layout. */
bus_t bus_init_with_default_devs(void);
/** Release bus mappings and attached devices. */
void bus_finish(bus_t *);
/** Load binary file into bus memory at address. */
bool bus_load_file(bus_t *, uint16_t, const char *);
/** Load in-memory buffer into bus memory at address. */
bool bus_load_buf(bus_t *, uint16_t, uint8_t *, uint16_t);

/** Map device at bus base address. */
void bus_device_add(bus_t *, device_t *, uint16_t);
/** Dump currently mapped devices (debug helper). */
void bus_device_dump(bus_t *);

#endif /* _BUS_H_ */

