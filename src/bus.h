#ifndef _BUS_H_
#define _BUS_H_

#include <stdint.h>
#include <stdbool.h>

#include "device.h"

#define RK65C02_BUS_SIZE	64*1024

struct bus_tag {
	device_mapping_t *dm_head;
};

typedef struct bus_tag bus_t;

uint8_t bus_read_1(bus_t *, uint16_t);
void bus_write_1(bus_t *, uint16_t, uint8_t);
bus_t bus_init();
bus_t bus_init_with_default_devs();
void bus_finish(bus_t *);
bool bus_load_file(bus_t *, uint16_t, const char *);
bool bus_load_buf(bus_t *, uint16_t, uint8_t *, uint16_t);

void bus_device_add(bus_t *, device_t *, uint16_t);
void bus_device_dump(bus_t *);

#endif /* _BUS_H_ */

