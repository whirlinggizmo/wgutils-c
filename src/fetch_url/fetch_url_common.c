#include "fetch_url_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fetch_url_result_reset_common(fetch_url_result_t *result)
{
    if (!result)
    {
        return;
    }

    free(result->data);
    result->data = NULL;
    result->size = 0;
    result->code = 0;
    result->url[0] = '\0';
}

int fetch_url_join_path_common(char *buffer,
                               size_t buffer_size,
                               const char *host_url,
                               const char *relative_path)
{
    int written = 0;
    size_t host_len = 0;

    if (!buffer || buffer_size == 0)
    {
        return -1;
    }

    host_len = host_url ? strlen(host_url) : 0;
    while (host_len > 0 && host_url[host_len - 1] == '/')
    {
        host_len--;
    }

    written = snprintf(buffer,
                       buffer_size,
                       "%.*s/%s",
                       (int)host_len,
                       host_url ? host_url : "",
                       relative_path ? relative_path : "");
    return (written < 0 || (size_t)written >= buffer_size) ? -1 : 0;
}

fetch_url_op_t *fetch_url_with_path_async_common(const char *host_url,
                                                 const char *relative_path,
                                                 int timeout_ms)
{
    char full_url[FETCH_URL_MAX_PATH_LENGTH];

    if (fetch_url_join_path_common(full_url, sizeof(full_url), host_url, relative_path) != 0)
    {
        return NULL;
    }

    return fetch_url_async(full_url, timeout_ms);
}

int fetch_url_with_path_sync_common(const char *host_url,
                                    const char *relative_path,
                                    int timeout_ms,
                                    fetch_url_result_t *result)
{
    char full_url[FETCH_URL_MAX_PATH_LENGTH];

    if (fetch_url_join_path_common(full_url, sizeof(full_url), host_url, relative_path) != 0)
    {
        if (result)
        {
            fetch_url_result_reset_common(result);
            result->code = -1;
        }
        return -1;
    }

    return fetch_url_sync(full_url, timeout_ms, result);
}

bool fetch_url_poll_common(fetch_url_op_t *op)
{
    return wg_op_is_done(op ? &op->op : NULL);
}
