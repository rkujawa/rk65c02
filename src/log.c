#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "log.h"

static const char *level_str[] = {
	"NONE",		/* should never appear in log */
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
	struct timespec t;

	if (l > level)
		return;

	clock_gettime(CLOCK_REALTIME, &t);

	fprintf(stderr, "%lld.%lld %s:\t", (long long int) t.tv_sec,
	    (long long int) t.tv_nsec, level_str[l]);

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, "\n");
}

