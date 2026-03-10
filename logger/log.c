#include "logger/log.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#undef log_trace
#undef log_debug
#undef log_info
#undef log_warn
#undef log_error
#undef log_fatal
#include "../vendor/logger-c/log.h"

static int wg_path_is_absolute_local(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    if (path[0] == '/') {
        return 1;
    }

    if (path[0] == '\\' && path[1] == '\\') {
        return 1;
    }

    if (isalpha((unsigned char)path[0]) && path[1] == ':' &&
        (path[2] == '\\' || path[2] == '/')) {
        return 1;
    }

    return strstr(path, "://") != NULL;
}

static const char *wg_format_source_path(const char *path, char *buffer,
                                         size_t buffer_size)
{
    if (path == NULL) {
        return "";
    }

    if (wg_path_is_absolute_local(path)) {
        return path;
    }

    if (buffer_size < 3) {
        return path;
    }

    snprintf(buffer, buffer_size, "./%s", path);
    return buffer;
}

static void wg_color_fmt1(record_t *rec, const char *time_buf)
{
    static const char *fmt_with_source = "%s %s%-5s\x1b[0m \x1b[90m[%s:%d]:\x1b[0m ";
    static const char *fmt_with_file = "%s %s%-5s\x1b[0m \x1b[90m[%s]:\x1b[0m ";
    static const char *fmt_simple = "%s %s%-5s\x1b[0m ";
    char display_path[1024];
    const char *source_path = "";
    int has_file = 0;
    int has_line = 0;

    if (rec->file != NULL && rec->file[0] != '\0') {
        has_file = 1;
        has_line = rec->line > 0;
        if (has_line) {
            source_path = wg_format_source_path(rec->file, display_path,
                                                sizeof(display_path));
        } else {
            source_path = rec->file;
        }
    }

    if (has_file && has_line) {
        fprintf(rec->hd_fp, fmt_with_source, time_buf, level_colors[rec->level],
                level_strings[rec->level], source_path, rec->line);
    } else if (has_file) {
        fprintf(rec->hd_fp, fmt_with_file, time_buf, level_colors[rec->level],
                level_strings[rec->level], source_path);
    } else {
        fprintf(rec->hd_fp, fmt_simple, time_buf, level_colors[rec->level],
                level_strings[rec->level]);
    }
}

static void log_install_formatter(void)
{
    static int installed = 0;
    if (installed) {
        return;
    }

    log_set_fmt_fn(ROOT_HANDLER_NAME, wg_color_fmt1);
    installed = 1;
}

void log_set_log_level(size_t level)
{
    log_install_formatter();
    log_set_level(ROOT_HANDLER_NAME, level);
}

void log_set_quiet_mode(bool quiet)
{
    log_install_formatter();
    log_set_quiet(ROOT_HANDLER_NAME, quiet);
}

void log_message(int level, const char *file, int line, const char *msg_fmt, ...)
{
    char message[2048];
    va_list args;
    const char *source_file = NULL;
    int source_line = -1;

    log_install_formatter();
    if (file != NULL && file[0] != '\0') {
        source_file = file;
        source_line = (line > 0) ? line : -1;
    }

    va_start(args, msg_fmt);
    vsnprintf(message, sizeof(message), msg_fmt, args);
    va_end(args);

    _log_message(level, source_file, source_line, "%s", message);
}

void log_message_simple(int level, const char *msg_fmt, ...)
{
    va_list args;
    char message[2048];

    log_install_formatter();
    va_start(args, msg_fmt);
    vsnprintf(message, sizeof(message), msg_fmt, args);
    va_end(args);
    log_message(level, NULL, -1, "%s", message);
}
