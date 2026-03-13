 #ifndef EMSCRIPTEN

#include "fetch_url.h"
#include <curl/curl.h>
#include "logger/logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
    char *data;
    size_t size;
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

static void fetch_url_op_destroy_impl(void *impl)
{
    fetch_url_result_t *result = (fetch_url_result_t *)impl;

    if (!result)
    {
        return;
    }

    fetch_url_result_reset(result);
    free(result);
}

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total_size = size * nmemb;
    fetch_context_t *context = (fetch_context_t *)userp;
    size_t new_size = 0;

    if (size != 0 && nmemb > (SIZE_MAX / size))
    {
        return 0;
    }

    if (context->size > (SIZE_MAX - total_size))
    {
        return 0;
    }
    new_size = context->size + total_size;

    char *ptr = realloc(context->data, new_size);
    if (!ptr)
    {
        return 0; // realloc failed
    }

    context->data = ptr;
    memcpy(&(context->data[context->size]), contents, total_size);
    context->size += total_size;

    return total_size;
}

fetch_url_op_t *fetch_url_async(const char *url, int timeout_ms)
{
    fetch_url_op_t *op = (fetch_url_op_t *)calloc(1, sizeof(fetch_url_op_t));
    fetch_url_result_t *stored_result = NULL;
    CURL *curl = NULL;
    fetch_context_t context = {0};
    CURLcode res;

    if (!op)
    {
        return NULL;
    }

    stored_result = (fetch_url_result_t *)calloc(1, sizeof(fetch_url_result_t));
    if (!stored_result)
    {
        free(op);
        return NULL;
    }

    wg_op_init(&op->op, WG_OP_KIND_FETCH_URL, stored_result, fetch_url_op_destroy_impl);
    snprintf(stored_result->url, sizeof(stored_result->url), "%s", url ? url : "");

    curl = curl_easy_init();
    if (!curl)
    {
        log_error("FETCH_URL: Error initializing curl");
        stored_result->code = -1;
        op->result = *stored_result;
        wg_op_complete(&op->op, stored_result->code);
        return op;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);

    res = curl_easy_perform(curl);
    if (res == CURLE_OK)
    {
        long http_code = 0;

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        stored_result->code = (int)http_code;
        if (http_code == 200)
        {
            char *ptr = NULL;

            stored_result->size = context.size;
            if (context.size > (SIZE_MAX - 1))
            {
                stored_result->code = -1;
                stored_result->size = 0;
                free(context.data);
                context.data = NULL;
            }
            else
            {
                ptr = realloc(context.data, context.size + 1);
                if (!ptr)
                {
                    stored_result->code = -1;
                    stored_result->size = 0;
                    free(context.data);
                    context.data = NULL;
                }
                else
                {
                    ptr[context.size] = '\0';
                    stored_result->data = ptr;
                }
            }
        }
        else
        {
            free(context.data);
            context.data = NULL;
        }
    }
    else
    {
        log_warn("FETCH_URL: curl_easy_perform() failed for '%s': %s", url, curl_easy_strerror(res));
        stored_result->data = NULL;
        stored_result->size = 0;
        stored_result->code = 500;
        free(context.data);
    }

    curl_easy_cleanup(curl);
    op->result = *stored_result;
    wg_op_complete(&op->op, stored_result->code);
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
    fetch_url_result_t *stored_result = NULL;

    if (!op || !result)
    {
        return -1;
    }

    if (!wg_op_is_done(&op->op))
    {
        return 1;
    }

    stored_result = (fetch_url_result_t *)wg_op_impl(&op->op);
    if (!stored_result)
    {
        return -1;
    }

    *result = *stored_result;
    op->result = *stored_result;
    stored_result->data = NULL;
    stored_result->size = 0;
    stored_result->code = 0;
    stored_result->url[0] = '\0';
    return 0;
}

void fetch_url_op_free(fetch_url_op_t *op)
{
    if (!op)
    {
        return;
    }

    wg_op_deinit(&op->op);
    free(op);
}

#endif // EMSCRIPTEN
