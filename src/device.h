#ifndef _DEVICE_H_
#define _DEVICE_H_

//#include "bus.h"

typedef struct device_space_t {
	//uint8_t id;
	uint16_t address;
	uint16_t size;

	uint8_t (*read_1)(uint16_t addr);
	void (*write_1)(uint16_t addr, uint8_t val);

	void *aux;	/* any additional space-specific data */

	struct device_space_t *next;
} device_space_t;

typedef struct device_t {
	const char *name;
	device_space_t *space_head;

	struct device_t *next;
} device_t;

#endif /* _DEVICE_H_ */

