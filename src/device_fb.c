#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <gc/gc.h>

#include "bus.h"
#include "device.h"

#define FB_AS_SIZE	0x1000

#define CHAR_RAM_SIZE	(80*50)	/* 0xFA0 */
#define CHAR_FONT_WIDTH	8
#define CHAR_FONT_HEIGHT 8

struct fb_state {
	uint8_t *cram;

	void (*fb_callback_screen_update)(uint8_t *);
};

uint8_t device_fb_read_1(void *, uint16_t);
void device_fb_write_1(void *, uint16_t, uint8_t);

uint8_t
device_fb_read_1(void *vd, uint16_t offset)
{
	device_t *d;
	struct fb_state *f;

	d = (device_t *) vd;
	f = d->aux;

	return f->cram[offset];
}

void
device_fb_write_1(void *vd, uint16_t offset, uint8_t val)
{
	device_t *d;

	struct fb_state *f;

	d = (device_t *) vd;
	f = d->aux;

	f->cram[offset] = val;

	if (f->fb_callback_screen_update != NULL)
		f->fb_callback_screen_update(f->cram);
}

device_t *
device_fb_init()
{
	device_t *d;
	struct fb_state *f;

	d = (device_t *) GC_MALLOC(sizeof(device_t));

	assert(d != NULL);

	d->name = "FB";
	d->size = FB_AS_SIZE;

	d->read_1 = device_fb_read_1;
	d->write_1 = device_fb_write_1;

	f = GC_MALLOC(sizeof(struct fb_state));
	d->aux = f;

	f->cram = GC_MALLOC(FB_AS_SIZE);
	memset(d->aux, 0, FB_AS_SIZE);

	return d;
}

void
device_fb_finish(device_t *d)
{
}

