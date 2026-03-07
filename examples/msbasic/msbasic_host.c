/*
 * MS BASIC host — run Microsoft BASIC (mist64/msbasic) in the emulator.
 *
 * Build: make (in examples/msbasic) or make msbasic (from examples/).
 * Run:   ./run_msbasic
 *
 * Loads BASIC ROM at $B000 and I/O stub at $F002. I/O device at $F000:
 *   write $F000 = putchar, read $F000 = getchar (blocking), read $F001 = status (bit0 = Ctrl-C).
 * Uses JIT when available. Entry point COLD_START from basic.lbl.
 */
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bus.h"
#include "device.h"
#include "device_ram.h"
#include "rk65c02.h"

#define BASROM_START	0xB000
#define IOSTUB_START	0xF002
#define IO_BASE		0xF000
#define IO_SIZE		16

#define LBL_COLD_START	".COLD_START"

static volatile sig_atomic_t break_requested;

struct io_config {
	uint8_t stub[IO_SIZE];  /* doff 2..15 = stub code; 0,1 = I/O ports */
};

static void
sigint_handler(int sig)
{
	(void)sig;
	break_requested = 1;
}

static void
tick_break(rk65c02emu_t *emu, void *ctx)
{
	(void)ctx;
	if (break_requested)
		rk65c02_request_stop(emu);
}

/* Parse basic.lbl for symbol address (e.g. "al 00CEBC .COLD_START" -> 0xCEBC). */
static bool
parse_lbl_address(const char *lbl_path, const char *symbol, uint16_t *out_addr)
{
	FILE *f;
	char line[256];
	unsigned int addr;

	f = fopen(lbl_path, "r");
	if (f == NULL)
		return false;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strncmp(line, "al ", 3) != 0)
			continue;
		if (strstr(line + 3, symbol) == NULL)
			continue;
		if (sscanf(line + 3, "%06x", &addr) != 1)
			continue;
		*out_addr = (uint16_t)addr;
		fclose(f);
		return true;
	}
	fclose(f);
	return false;
}

/* BASIC expects CR ($0D) as line terminator; Unix terminals often send LF ($0A). */
#define CR	0x0D
#define LF	0x0A

static uint8_t
io_read_1(void *dev, uint16_t doff)
{
	struct io_config *cfg = (struct io_config *)((device_t *)dev)->config;
	int c;

	if (doff == 0) {
		c = getchar();
		if (c == EOF) {
			break_requested = 1;	/* stop after this line so we don't loop on EOF */
			return CR;	/* end-of-line so BASIC doesn't loop on NUL */
		}
		c &= 0xFF;
		if (c == LF)
			c = CR;
		return (uint8_t)c;
	}
	if (doff == 1)
		return break_requested ? 1 : 0;
	return cfg->stub[doff];
}

static void
io_write_1(void *dev, uint16_t doff, uint8_t val)
{
	struct io_config *cfg = (struct io_config *)((device_t *)dev)->config;

	if (doff == 0) {
		putchar((char)val);
		fflush(stdout);
		return;
	}
	cfg->stub[doff] = val;
}

static struct io_config io_config;
static device_t io_device = {
	.name = "msbasic_io",
	.size = IO_SIZE,
	.read_1 = io_read_1,
	.write_1 = io_write_1,
	.finish = NULL,
	.config = &io_config,
	.aux = NULL
};

int
main(int argc, char **argv)
{
	bus_t bus;
	rk65c02emu_t e;
	uint16_t cold_start;
	const char *lbl_path = "basic.lbl";
	const char *basic_path = "basic.bin";
	const char *stub_path = "iostub.bin";
	struct sigaction sa, sa_old;

	(void)argc;
	(void)argv;

	memset(&io_config, 0, sizeof(io_config));

	if (!parse_lbl_address(lbl_path, LBL_COLD_START, &cold_start)) {
		fprintf(stderr, "msbasic: could not find %s in %s\n",
		    LBL_COLD_START, lbl_path);
		return 1;
	}

	/* RAM from 0: 32KB usable; BASIC ROM at $B000. */
	bus = bus_init();
	bus_device_add(&bus, device_ram_init(0x8000), 0x0000);
	bus_device_add(&bus, device_ram_init(0x3000), 0xB000);
	bus_device_add(&bus, &io_device, IO_BASE);

	if (!bus_load_file(&bus, BASROM_START, basic_path)) {
		fprintf(stderr, "msbasic: could not load %s at $%04X\n",
		    basic_path, BASROM_START);
		bus_finish(&bus);
		return 1;
	}
	/* Load stub into device buffer at doff 2.. (ports 0,1 stay zero). */
	if (!bus_load_file(&bus, IOSTUB_START, stub_path)) {
		fprintf(stderr, "msbasic: could not load %s at $%04X\n",
		    stub_path, IOSTUB_START);
		bus_finish(&bus);
		return 1;
	}

	e = rk65c02_init(&bus);
	e.regs.SP = 0xFF;
	e.regs.PC = cold_start;

	break_requested = 0;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, &sa_old) != 0)
		(void)sa_old; /* not critical */

	rk65c02_tick_set(&e, tick_break, 1024, NULL);

	rk65c02_jit_enable(&e, true);
	rk65c02_start(&e);

	sigaction(SIGINT, &sa_old, NULL);
	bus_finish(&bus);

	if (e.stopreason == STP)
		return 0;
	if (break_requested)
		return 130;
	fprintf(stderr, "msbasic: stopped (%s)\n",
	    rk65c02_stop_reason_string(e.stopreason));
	return 1;
}
