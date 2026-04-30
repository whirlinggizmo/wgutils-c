#include "fetch_url/fetch_url.h"

#include <stdlib.h>
#include <string.h>

fetch_url_op_t *fetch_url_async(const char *url, int timeout_ms)
{
    fetch_url_op_t *op = (fetch_url_op_t *)calloc(1, sizeof(fetch_url_op_t));

    if (!op)
    {
        return NULL;
    }

    wg_op_init(&op->op, WG_OP_KIND_FETCH_URL, NULL, NULL);
    (void)url;
    (void)timeout_ms;
    op->result.code = 501;
    wg_op_complete(&op->op, op->result.code);
    return op;
}

int fetch_url_sync(const char *url, int timeout_ms, fetch_url_result_t *result)
{
    if (!result)
    {
        return -1;
    }

    (void)url;
    (void)timeout_ms;
    memset(result, 0, sizeof(*result));
    result->code = 501;
    return result->code;
}

int fetch_url_with_path_sync(const char *host_url,
                             const char *relative_path,
                             int timeout_ms,
                             fetch_url_result_t *result)
{
    (void)host_url;
    (void)relative_path;
    return fetch_url_sync(NULL, timeout_ms, result);
}

fetch_url_op_t *fetch_url_with_path_async(const char *host_url,
                                          const char *relative_path,
                                          int timeout_ms)
{
    fetch_url_op_t *op = (fetch_url_op_t *)calloc(1, sizeof(fetch_url_op_t));

    if (!op)
    {
        return NULL;
    }

    wg_op_init(&op->op, WG_OP_KIND_FETCH_URL, NULL, NULL);
    (void)host_url;
    (void)relative_path;
    (void)timeout_ms;
    op->result.code = 503;
    wg_op_complete(&op->op, op->result.code);
    return op;
}

bool fetch_url_poll(fetch_url_op_t *op)
{
    return wg_op_is_done(op ? &op->op : NULL);
}

int fetch_url_finish(fetch_url_op_t *op, fetch_url_result_t *result)
{
    if (!op || !result)
    {
        return -1;
    }

    if (!wg_op_is_done(&op->op))
    {
        return 1;
    }

    *result = op->result;
    op->result.data = NULL;
    op->result.size = 0;
    op->result.code = 0;
    op->result.url[0] = '\0';
    return 0;
}

void fetch_url_op_free(fetch_url_op_t *op)
{
    if (!op)
    {
        return;
    }

    free(op->result.data);
    wg_op_deinit(&op->op);
    free(op);
}
