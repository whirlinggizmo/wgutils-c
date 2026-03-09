#ifndef FETCH_URL_H
#define FETCH_URL_H

#include <stddef.h>

#define FETCH_URL_MAX_PATH_LENGTH 256
typedef struct
{
    char *data;
    size_t size;
    int code;
    char url[FETCH_URL_MAX_PATH_LENGTH];
} fetch_url_result_t;

// Fetch from a URL and return the content as a string (or NULL on failure).
// The caller is responsible for freeing the returned result.
fetch_url_result_t fetch_url(const char *url, int timeout_ms);

fetch_url_result_t fetch_url_with_path(const char* host_url, const char* relative_path, int timeout_ms);

#endif // FETCH_URL_H
