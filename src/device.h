#ifndef _DEVICE_H_
#define _DEVICE_H_

typedef struct device_t {
	const char *name;

	uint16_t size;

	uint8_t (*read_1)(void *, uint16_t doff);
	void (*write_1)(void *, uint16_t, uint8_t val);
	void (*finish)(void *);

	void *config;
	void *aux;	/* any dev space-specific data */
} device_t;

typedef struct device_mapping_t {
	device_t *dev;
	uint16_t addr;

	struct device_mapping_t *next;
} device_mapping_t;

#endif /* _DEVICE_H_ */

