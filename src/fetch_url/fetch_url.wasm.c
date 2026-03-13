#ifdef EMSCRIPTEN

#include "fetch_url.h"
#include "logger/logger.h"

#include <emscripten/fetch.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    fetch_url_op_t *owner;
} fetch_context_t;

static void fetch_url_result_reset(fetch_url_result_t *result)
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

static void fetch_url_context_destroy(void *impl)
{
    fetch_context_t *context = (fetch_context_t *)impl;

    free(context);
}

static void fetch_success(emscripten_fetch_t *fetch)
{
    fetch_context_t *context = (fetch_context_t *)fetch->userData;
    fetch_url_op_t *op = context ? context->owner : NULL;

    if (!op)
    {
        emscripten_fetch_close(fetch);
        return;
    }

    op->result.data = (char *)malloc(fetch->numBytes + 1);
    if (op->result.data == NULL)
    {
        log_error("FETCH_URL: Failed to allocate memory for fetched content.");
        op->result.code = 500;
        wg_op_complete(&op->op, op->result.code);
        emscripten_fetch_close(fetch);
        return;
    }

    memcpy(op->result.data, fetch->data, fetch->numBytes);
    op->result.data[fetch->numBytes] = '\0';
    op->result.size = fetch->numBytes;
    op->result.code = fetch->status;
    log_debug("FETCH_URL: Fetched %llu bytes from %s", fetch->numBytes, fetch->url);
    wg_op_complete(&op->op, op->result.code);
    emscripten_fetch_close(fetch);
}

static void fetch_error(emscripten_fetch_t *fetch)
{
    fetch_context_t *context = (fetch_context_t *)fetch->userData;
    fetch_url_op_t *op = context ? context->owner : NULL;

    log_warn("FETCH_URL: Fetch failed: HTTP status %d, URL: %s", fetch->status, fetch->url);
    if (op)
    {
        op->result.data = NULL;
        op->result.size = 0;
        op->result.code = fetch->status > 0 ? fetch->status : 500;
        wg_op_complete(&op->op, op->result.code);
    }

    emscripten_fetch_close(fetch);
}

fetch_url_op_t *fetch_url_async(const char *url, int timeout_ms)
{
    fetch_url_op_t *op = NULL;
    fetch_context_t *context = NULL;
    emscripten_fetch_attr_t attr;

    op = (fetch_url_op_t *)calloc(1, sizeof(fetch_url_op_t));
    if (!op)
    {
        return NULL;
    }

    context = (fetch_context_t *)calloc(1, sizeof(fetch_context_t));
    if (!context)
    {
        free(op);
        return NULL;
    }

    wg_op_init(&op->op, WG_OP_KIND_FETCH_URL, context, fetch_url_context_destroy);
    context->owner = op;
    snprintf(op->result.url, sizeof(op->result.url), "%s", url ? url : "");

    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.timeoutMSecs = (unsigned int)((timeout_ms > 0) ? timeout_ms : 0);
    attr.onsuccess = fetch_success;
    attr.onerror = fetch_error;
    attr.userData = context;

    log_debug("FETCH_URL: Fetching URL: %s", url ? url : "");
    emscripten_fetch(&attr, url ? url : "");
    return op;
}

fetch_url_op_t *fetch_url_with_path_async(const char *host_url, const char *relative_path, int timeout_ms)
{
    char full_url[FETCH_URL_MAX_PATH_LENGTH];

    snprintf(full_url, sizeof(full_url), "%s/%s", host_url ? host_url : "", relative_path ? relative_path : "");
    return fetch_url_async(full_url, timeout_ms);
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

    fetch_url_result_reset(&op->result);
    wg_op_deinit(&op->op);
    free(op);
}

#endif // EMSCRIPTEN
