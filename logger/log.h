#ifndef WGUTILS_LOGGER_H
#define WGUTILS_LOGGER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum log_level {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level_t;

/* Configure root logger handler level/quiet state without exposing
 * vendor logger symbols that can collide with other libraries.
 */
void log_set_log_level(size_t level);
void log_set_quiet_mode(bool quiet);
void log_message(int level, const char *file, int line, const char *msg_fmt, ...);
void log_message_simple(int level, const char *msg_fmt, ...);

/* Vendor logger entrypoint, intentionally re-declared here so consumers
 * can log without including vendor headers (avoids enum-name collisions).
 */
void _log_message(int level, const char *file, int line, const char *msg_fmt, ...);

/* Keep call sites readable while maintaining prefixed level constants. */
#define log_trace(...) log_message(LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) log_message(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_message(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif
