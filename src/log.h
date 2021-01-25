/**
 * @file log.h
 * @brief Logging-related functions.
 */
#include <stdint.h>

#define LOG_TRACE	5	/**< Most verbose log level. Used for tracing. */
#define LOG_DEBUG	4	/**< Debug-level messages. */
#define LOG_INFO	3	/**< Informational messages. */
#define LOG_WARN	4	/**< Warning messages. */
#define LOG_ERROR	2	/**< Errors. */
#define LOG_CRIT	1	/**< Critical errors. */
#define LOG_NOTHING	0	/**< At 0 nothing will get logged, can be set as
				   current level, but not when creating new log
				   messages. */

/**
 * @brief Set the logging verbosity level.
 * @param level Desired log verbosity level.
 */
void rk65c02_loglevel_set(uint8_t level);

/**
 * @brief Send a message to log.
 * @param level Log level at which message should be logged.
 * @param fmt Message in a printf-like format.
 */
void rk65c02_log(uint8_t level, const char *fmt, ...);

