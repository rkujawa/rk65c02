#include <stdio.h>
#include <stdarg.h>

#include <sys/time.h>

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

void rk65c02_loglevel_set(uint8_t l)
{
	level = l;
}

void rk65c02_log(uint8_t l, const char* fmt, ...)
{
	va_list args;
	struct timeval t;
	
	gettimeofday(&t, NULL);

	if (l > level)
		return;

	fprintf(stderr, "%ld %s:\t", (t.tv_sec * 1000000 + t.tv_usec),
	    level_str[l]);

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, "\n");
}

