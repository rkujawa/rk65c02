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

