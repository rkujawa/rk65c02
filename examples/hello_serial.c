/*
 * Hello-serial example — custom bus device at $DE00, guest writes, host prints.
 *
 * Build: make hello_serial hello_serial.rom
 * Run:   ./hello_serial
 *
 * Demonstrates: bus_device_add with a host-defined device (no MMU); guest
 * writes bytes to $DE00, device write_1 callback prints to stdout.
 * Expected: "Hi!" printed, then PASS and exit 0.
 */
#include <stdint.h>
#include <stdio.h>

#include "bus.h"
#include "device.h"
#include "rk65c02.h"

static const uint16_t load_addr = 0xC000;
#define SERIAL_BASE  0xDE00
#define SERIAL_SIZE  16

static uint8_t serial_read_1(void *config, uint16_t doff)
{
	(void)config;
	(void)doff;
	return 0;
}

static void serial_write_1(void *config, uint16_t doff, uint8_t val)
{
	(void)config;
	(void)doff;
	putchar((char)val);
}

static device_t serial_device = {
	.name = "serial",
	.size = SERIAL_SIZE,
	.read_1 = serial_read_1,
	.write_1 = serial_write_1,
	.finish = NULL,
	.config = NULL,
	.aux = NULL
};

int
main(void)
{
	bus_t bus;
	rk65c02emu_t e;

	bus = bus_init_with_default_devs();
	bus_device_add(&bus, &serial_device, SERIAL_BASE);

	if (!bus_load_file(&bus, load_addr, "hello_serial.rom")) {
		fprintf(stderr, "FAIL: could not load hello_serial.rom\n");
		bus_finish(&bus);
		return 1;
	}

	e = rk65c02_init(&bus);
	e.regs.SP = 0xFF;
	e.regs.PC = load_addr;

	rk65c02_start(&e);

	bus_finish(&bus);

	if (e.stopreason != STP) {
		fprintf(stderr, "FAIL: expected STP, got %s\n",
		    rk65c02_stop_reason_string(e.stopreason));
		return 1;
	}
	printf("\nPASS: guest wrote to $DE00, host printed output.\n");
	return 0;
}
