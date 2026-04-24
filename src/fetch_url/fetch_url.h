#ifndef FETCH_URL_H
#define FETCH_URL_H

#include <stddef.h>
#include <stdbool.h>
#include "async/wg_op.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FETCH_URL_MAX_PATH_LENGTH 256
typedef struct
{
    char *data;
    size_t size;
    int code;
    char url[FETCH_URL_MAX_PATH_LENGTH];
} fetch_url_result_t;

typedef struct
{
    wg_op_t op;
    fetch_url_result_t result;
} fetch_url_op_t;

fetch_url_op_t *fetch_url_async(const char *url, int timeout_ms);
fetch_url_op_t *fetch_url_with_path_async(const char *host_url, const char *relative_path, int timeout_ms);
bool fetch_url_poll(fetch_url_op_t *op);
int fetch_url_finish(fetch_url_op_t *op, fetch_url_result_t *result);
void fetch_url_op_free(fetch_url_op_t *op);
int fetch_url_head(const char *url, int timeout_ms);
float fetch_url_ping(const char *url, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // FETCH_URL_H
