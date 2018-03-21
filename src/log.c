#include <stdio.h>
#include <stdarg.h>

#include "log.h"

static const char *level_str[] = {
	"",
	"CRITICAL",
	"ERROR",
	"INFO",
	"DEBUG",
	"TRACE"
};

static uint8_t level = LOG_INFO;

void rk6502_loglevel_set(uint8_t l)
{
	level = l;
}

void rk6502_log(uint8_t l, const char* fmt, ...)
{
	va_list args;

	if (l > level)
		return;

	fprintf(stderr, "%s:\t", level_str[l]);

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, "\n");
}

