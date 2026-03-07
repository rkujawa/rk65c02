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

/** Extended physical mapping: device at 32-bit base (for MMU physical space > 64K). */
typedef struct device_phys_mapping_t device_phys_mapping_t;
struct device_phys_mapping_t {
	device_t *dev;
	uint32_t base;
	device_phys_mapping_t *next;
};

struct bus_tag {
	device_mapping_t *dm_head;
	device_phys_mapping_t *dm_phys_head;

	bool access_debug;
};

typedef struct bus_tag bus_t;

/** Read one byte from bus address (16-bit, legacy/MMU paddr < 64K). */
uint8_t bus_read_1(bus_t *, uint16_t);
/** Write one byte to bus address (16-bit). */
void bus_write_1(bus_t *, uint16_t, uint8_t);
/** Read one byte from physical address (32-bit; for MMU paddr >= 64K). */
uint8_t bus_read_1_phys(bus_t *, uint32_t);
/** Write one byte to physical address (32-bit). */
void bus_write_1_phys(bus_t *, uint32_t, uint8_t);
/** Initialize an empty bus. */
bus_t bus_init(void);
/** Initialize bus with default memory/device layout. */
bus_t bus_init_with_default_devs(void);
/** Release bus mappings and attached devices. */
void bus_finish(bus_t *);
/** Load binary file into bus memory at address (16-bit). */
bool bus_load_file(bus_t *, uint16_t, const char *);
/** Load binary file into extended physical address (32-bit; for paddr >= 64K). */
bool bus_load_file_phys(bus_t *, uint32_t, const char *);
/** Load in-memory buffer into bus memory at address. */
bool bus_load_buf(bus_t *, uint16_t, uint8_t *, uint16_t);

/** Map device at bus base address (16-bit). */
void bus_device_add(bus_t *, device_t *, uint16_t);
/** Map device at extended physical base (32-bit; for MMU physical space > 64K). */
void bus_device_add_phys(bus_t *, device_t *, uint32_t base);
/** Dump currently mapped devices (debug helper). */
void bus_device_dump(bus_t *);

#endif /* _BUS_H_ */

