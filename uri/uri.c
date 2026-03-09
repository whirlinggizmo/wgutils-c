#include "uri.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../vendor/cwalk/cwalk.h"

static bool uri_has_valid_scheme(const char *value, const char **scheme_end_out)
{
    const char *p = value;

    if (value == NULL || value[0] == '\0' || !isalpha((unsigned char)value[0])) {
        return false;
    }

    p++;
    while (*p != '\0' && (isalnum((unsigned char)*p) || *p == '+' || *p == '-' || *p == '.')) {
        p++;
    }

    if (p[0] == ':' && p[1] == '/' && p[2] == '/') {
        if (scheme_end_out != NULL) {
            *scheme_end_out = p;
        }
        return true;
    }

    return false;
}

bool uri_is_url(const char *value)
{
    return uri_has_valid_scheme(value, NULL);
}

size_t uri_normalize(const char *uri, char *buffer, size_t buffer_size)
{
    const char *scheme_end = NULL;
    const char *authority_start = NULL;
    const char *authority_end = NULL;
    const char *path_start = NULL;
    const char *path_end = NULL;
    const char *query_start = NULL;
    const char *fragment_start = NULL;
    size_t scheme_len = 0;
    size_t authority_len = 0;
    size_t path_len = 0;
    size_t normalized_path_len = 0;
    size_t total_len = 0;
    char *path_buf = NULL;
    char *normalized_path_buf = NULL;

    if (uri == NULL) {
        if (buffer != NULL && buffer_size > 0) {
            buffer[0] = '\0';
        }
        return 0;
    }

    if (!uri_has_valid_scheme(uri, &scheme_end)) {
        size_t len = strlen(uri);
        if (buffer != NULL && buffer_size > 0) {
            size_t copy_len = (len < (buffer_size - 1)) ? len : (buffer_size - 1);
            memcpy(buffer, uri, copy_len);
            buffer[copy_len] = '\0';
        }
        return len;
    }

    authority_start = scheme_end + 3; // skip ://
    authority_end = authority_start;
    while (*authority_end != '\0' && *authority_end != '/' && *authority_end != '?' && *authority_end != '#') {
        authority_end++;
    }

    path_start = authority_end;
    path_end = path_start;
    while (*path_end != '\0' && *path_end != '?' && *path_end != '#') {
        path_end++;
    }

    if (*path_end == '?') {
        query_start = path_end;
        fragment_start = strchr(query_start, '#');
    } else if (*path_end == '#') {
        fragment_start = path_end;
    }

    scheme_len = (size_t)(scheme_end - uri);
    authority_len = (size_t)(authority_end - authority_start);
    path_len = (size_t)(path_end - path_start);

    if (path_len > 0) {
        path_buf = (char *)malloc(path_len + 1);
        normalized_path_buf = (char *)malloc(path_len + 2);
        if (path_buf == NULL || normalized_path_buf == NULL) {
            free(path_buf);
            free(normalized_path_buf);
            if (buffer != NULL && buffer_size > 0) {
                buffer[0] = '\0';
            }
            return 0;
        }

        memcpy(path_buf, path_start, path_len);
        path_buf[path_len] = '\0';
        normalized_path_len = cwk_path_normalize(path_buf, normalized_path_buf, path_len + 2);
    }

    total_len = scheme_len + 3 + authority_len + normalized_path_len;
    if (query_start != NULL) {
        total_len += strlen(query_start);
    }
    if (query_start == NULL && fragment_start != NULL) {
        total_len += strlen(fragment_start);
    }

    if (buffer != NULL && buffer_size > 0) {
        int written = snprintf(buffer, buffer_size, "%.*s://%.*s%s%s%s",
                               (int)scheme_len, uri,
                               (int)authority_len, authority_start,
                               (normalized_path_len > 0) ? normalized_path_buf : "",
                               (query_start != NULL) ? query_start : "",
                               (fragment_start != NULL && query_start == NULL) ? fragment_start : "");
        (void)written;
    }

    free(path_buf);
    free(normalized_path_buf);
    return total_len;
}
