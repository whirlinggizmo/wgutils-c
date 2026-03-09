#include "path.h"
#include "../uri/uri.h"
#include "../vendor/cwalk/cwalk.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

size_t _path_join(const char *base, const char *path, char *buffer, size_t buffer_size, ...)
{
    va_list args;
    va_start(args, buffer_size);
    size_t total_len = cwk_path_join(base, path, buffer, buffer_size);
    const char *next_path;
    while ((next_path = va_arg(args, const char *)) != NULL)
    {
        total_len = cwk_path_join(buffer, next_path, buffer, buffer_size);
    }
    va_end(args);
    return total_len;
}

size_t path_normalize(const char *path, char *buffer, size_t buffer_size)
{
    if (uri_is_url(path)) {
        return uri_normalize(path, buffer, buffer_size);
    }

    return cwk_path_normalize(path, buffer, buffer_size);
}

// Calculate total length needed for joined paths
size_t _path_length(const char *base, ...)
{
    if (!base)
        return 0;

    va_list args;
    va_start(args, base);

    size_t total_len = strlen(base);
    const char *arg;
    while ((arg = va_arg(args, const char *)) != NULL)
    {
        total_len += strlen(arg) + 1; // +1 for path separator
    }
    va_end(args);
    return total_len;
}

// joins multiple paths into one.
// User must free the returned buffer
// Paths are passed as variadic arguments, terminated by NULL
char *_path_join_alloc(const char *base, ...)
{
    const char *next_path = NULL;
    size_t buffer_size = 0;
    size_t total_len = 0;
    if (!base)
        return NULL;

    va_list args;
    va_start(args, base);
    buffer_size = strlen(base) + 1;
    while ((next_path = va_arg(args, const char *)) != NULL)
    {
        buffer_size += strlen(next_path) + 1;
    }
    va_end(args);

    char *buf = malloc(buffer_size);
    if (!buf)
    {
        return NULL;
    }
    strcpy(buf, base);

    // Second pass: join all path components in order.
    va_start(args, base);
    while ((next_path = va_arg(args, const char *)) != NULL)
    {
        total_len = cwk_path_join(buf, next_path, buf, buffer_size);
        if (total_len >= buffer_size)
        {
            break;
        }
    }
    va_end(args);

    return buf;
}

char* path_get_filename(const char *path) {
    char *last_slash = strrchr(path, '/');
    if (last_slash)
        return last_slash + 1;
    else
        return (char*)path;
}

char* path_get_extension(const char *path) {
    char *last_dot = strrchr(path, '.');
    if (last_dot)
        return last_dot + 1;
    else
        return NULL;
}

/*  moved to header to allow other files to use
// helper with joining paths, no null terminator required by caller
#define path_join_alloc(...) _path_join_alloc(__VA_ARGS__, (char*)NULL)

// Macro that automatically adds NULL terminator
#define path_length(...) \
    _path_length(__VA_ARGS__, (char*)NULL)

// Macro that automatically adds NULL terminator
#define path_join(base, path, buffer, buffer_size, ...) \
    _path_join(base, path, buffer, buffer_size, ##__VA_ARGS__, (char*)NULL)
*/
