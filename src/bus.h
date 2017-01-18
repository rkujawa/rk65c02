#ifndef _BUS_H_
#define _BUS_H_

#include <stdint.h>

#define RK65C02_BUS_SIZE	64*1024

struct bus_tag {
	uint8_t *space;
};

typedef struct bus_tag bus_t;

uint8_t bus_read_1(bus_t *, uint16_t);
void bus_write_1(bus_t *, uint16_t, uint8_t);
bus_t bus_init();
void bus_finish(bus_t *);

#endif /* _BUS_H_ */
