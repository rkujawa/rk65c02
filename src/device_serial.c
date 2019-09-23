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
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <gc/gc.h>

#include "bus.h"
#include "device.h"

const static char *txpipepath = "/tmp/rk65c02_serial_tx"; /* should really be configurable */
const static char *rxpipepath = "/tmp/rk65c02_serial_rx"; /* should really be configurable */

struct device_serial_priv {
	int txpipefd;
	int rxpipefd;
};

uint8_t device_serial_read_1(void *, uint16_t);
void device_serial_write_1(void *, uint16_t, uint8_t);
void device_serial_finish(void *);

uint8_t
device_serial_read_1(void *vd, uint16_t offset)
{
	device_t *d;
	struct device_serial_priv *dp;
	ssize_t nread;
	uint8_t val;

	d = (device_t *) vd;
	dp = d->aux;

	switch (offset) {
	case 0x1:	
		nread = read(dp->rxpipefd, &val, 1);;	
		if (nread == 0)
			val = 0xFE;
		if (nread == -1)
			val = 0xFD;
	default:
		break;
	}
	// XXX: TODO

	return 0xFF;
}

void
device_serial_write_1(void *vd, uint16_t offset, uint8_t val)
{
	device_t *d;
	struct device_serial_priv *dp;

	d = (device_t *) vd;
	dp = d->aux;

	switch (offset) {
	case 0x0:
		/*fprintf(stderr, "writing to fd %d val %x\n", dp->txpipefd, val);*/
		write(dp->txpipefd, &val, 1);
		fsync(dp->txpipefd);
		break;
	default:
		/* do nothing */
		break;
	}
}

device_t *
device_serial_init()
{
	device_t *d;
	struct device_serial_priv *dp;

	d = (device_t *) GC_MALLOC(sizeof(device_t));

	assert(d != NULL);

	d->name = "Serial";
	d->size = 4;

	d->read_1 = device_serial_read_1;
	d->write_1 = device_serial_write_1;
	d->finish = device_serial_finish;

	dp = (struct device_serial_priv *) GC_MALLOC(sizeof(struct device_serial_priv));
	assert(dp != NULL);
	d->aux = dp; 
		
	if (mkfifo(txpipepath, S_IRUSR | S_IWUSR) != 0) {
		fprintf(stderr, "Creating FIFO for serial port failed!\n");
		/* perror, handle this failure... */
	}
	if (mkfifo(rxpipepath, S_IRUSR | S_IWUSR) != 0) {
		fprintf(stderr, "Creating FIFO for serial port failed!\n");
		/* perror, handle this failure... */
	}

	dp->txpipefd = open(txpipepath, O_WRONLY);
	dp->rxpipefd = open(rxpipepath, O_RDONLY | O_NONBLOCK);

	return d;
}

void
device_serial_finish(void *dv)
{
	struct device_serial_priv *dp;
	struct device_t *d;

	d = (struct device_t *) dv;
	dp = d->aux;

	close(dp->txpipefd);
	close(dp->rxpipefd);

	unlink(txpipepath);
	unlink(rxpipepath);
}

