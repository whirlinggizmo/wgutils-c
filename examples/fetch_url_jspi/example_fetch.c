#include "fetch_url/fetch_url.h"

#include <emscripten/emscripten.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static fetch_url_op_t *g_fetch_op = NULL;
static int g_phase = 0;
static int g_last_status = 0;
static unsigned int g_last_size = 0;
static char g_trace[512] = {0};

static void example_trace_append(const char *label)
{
    size_t len = 0;

    if (!label)
    {
        return;
    }

    len = strlen(g_trace);
    if (len >= sizeof(g_trace) - 1)
    {
        return;
    }

    snprintf(g_trace + len, sizeof(g_trace) - len, "%s%s", len ? " -> " : "", label);
}

static void example_release_op(void)
{
    if (g_fetch_op)
    {
        fetch_url_op_free(g_fetch_op);
        g_fetch_op = NULL;
    }
}

static void example_release_result(fetch_url_result_t *result)
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

EMSCRIPTEN_KEEPALIVE
void example_reset(void)
{
    example_release_op();
    g_phase = 0;
    g_last_status = 0;
    g_last_size = 0;
    g_trace[0] = '\0';
}

EMSCRIPTEN_KEEPALIVE
int example_phase(void)
{
    return g_phase;
}

EMSCRIPTEN_KEEPALIVE
int example_last_status(void)
{
    return g_last_status;
}

EMSCRIPTEN_KEEPALIVE
unsigned int example_last_size(void)
{
    return g_last_size;
}

EMSCRIPTEN_KEEPALIVE
const char *example_trace(void)
{
    return g_trace;
}

EMSCRIPTEN_KEEPALIVE
int example_run_fetch(const char *url)
{
#ifdef WGUTILS_EXAMPLE_USE_FETCH_URL_SYNC
    fetch_url_result_t result = {0};
#endif

    example_reset();
    g_phase = 1;

#ifdef WGUTILS_EXAMPLE_USE_FETCH_URL_SYNC
    example_trace_append("run:before_fetch_url_sync");
    g_phase = 2;
    g_last_status = fetch_url_sync(url, 0, &result);
    example_trace_append("run:after_fetch_url_sync");

    g_last_status = result.code;
    g_last_size = (unsigned int)result.size;
    example_release_result(&result);
    g_phase = 3;
    return g_last_status;
#else
    example_trace_append("run:before_fetch_url_async");
    g_fetch_op = fetch_url_async(url, 0);
    example_trace_append("run:after_fetch_url_async");
    if (!g_fetch_op)
    {
        g_phase = -1;
        g_last_status = -1;
        example_trace_append("run:fetch_url_async_failed");
        return -1;
    }

    return 0;
#endif
}

EMSCRIPTEN_KEEPALIVE
int example_poll_fetch(void)
{
#ifdef WGUTILS_EXAMPLE_USE_FETCH_URL_SYNC
    return g_phase == 3 ? 1 : 0;
#else
    fetch_url_result_t result = {0};

    if (!g_fetch_op)
    {
        example_trace_append("poll:no_op");
        return -1;
    }

    if (!fetch_url_poll(g_fetch_op))
    {
        example_trace_append("poll:not_ready");
        return 0;
    }

    g_phase = 2;
    example_trace_append("poll:before_fetch_url_finish");
    if (fetch_url_finish(g_fetch_op, &result) != 0)
    {
        g_phase = -1;
        g_last_status = -2;
        example_trace_append("poll:fetch_url_finish_failed");
        example_release_op();
        return -2;
    }
    example_trace_append("poll:after_fetch_url_finish");

    g_last_status = result.code;
    g_last_size = (unsigned int)result.size;
    example_release_result(&result);
    example_release_op();
    g_phase = 3;
    return 1;
#endif
}
