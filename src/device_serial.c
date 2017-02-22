#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
//#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "bus.h"
#include "device.h"

const static char *pipepath = "/tmp/serial_tx"; /* should really be configurable */

struct device_serial_priv {
	int pipefd;
};

uint8_t device_serial_read_1(void *, uint16_t);
void device_serial_write_1(void *, uint16_t, uint8_t);

uint8_t
device_serial_read_1(void *vd, uint16_t offset)
{
	device_t *d;
	struct device_serial_priv *dp;

	d = (device_t *) vd;
	dp = d->aux;

	// XXX: TODO

	return 0xAA;
}

void
device_serial_write_1(void *vd, uint16_t offset, uint8_t val)
{
	device_t *d;
	struct device_serial_priv *dp;

	d = (device_t *) vd;
	dp = d->aux;

	/*fprintf(stderr, "writing to fd %d val %x", dp->pipefd, val);*/

	write(dp->pipefd, &val, 1);
	fsync(dp->pipefd);
}

device_t *
device_serial_init()
{
	device_t *d;
	struct device_serial_priv *dp;

	d = (device_t *) malloc(sizeof(device_t));

	assert(d != NULL);

	d->name = "Serial";
	d->size = 4;

	d->read_1 = device_serial_read_1;
	d->write_1 = device_serial_write_1;

	dp = (struct device_serial_priv *) malloc(sizeof(struct device_serial_priv));
	d->aux = dp; 
		
	if (mkfifo(pipepath, S_IRUSR | S_IWUSR) != 0) {
		fprintf(stderr, "Creating FIFO for serial port failed!\n");
		/* perror, handle this failure... */
	}

	dp->pipefd = open(pipepath, O_RDWR);

	return d;
}

void
device_serial_finish(device_t *d)
{
	free(d->aux);
	//close pipe etc.
}

