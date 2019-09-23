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
	assert(f != NULL);
	d->aux = f;

	f->cram = GC_MALLOC(FB_AS_SIZE);
	assert(f->cram != NULL);
	memset(d->aux, 0, FB_AS_SIZE);

	return d;
}

void
device_fb_finish(device_t *d)
{
}

