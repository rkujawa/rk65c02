#include <stdint.h>

#define LOG_TRACE	5
#define LOG_DEBUG	4
#define LOG_INFO	3
#define LOG_WARN	4
#define LOG_ERROR	2
#define LOG_CRIT	1
#define LOG_NOTHING	0	/* At 0 nothing will get logged, can be set as
				   current level, but not when creating new log
				   messages. */

void rk6502_loglevel_set(uint8_t);
void rk6502_log(uint8_t, const char *, ...);

