#ifndef _DEVICE_SERIAL_H_
#define _DEVICE_SERIAL_H_

#include "device.h"

device_t * device_serial_init();
void device_serial_finish(device_t *);

#endif /* _DEVICE_SERIAL_H_ */

