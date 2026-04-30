#ifdef EMSCRIPTEN

#include "fetch_url.h"
#include "logger/logger.h"

#include <emscripten.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

EM_ASYNC_JS(int, fetch_url_jspi_download, (const char *url, uintptr_t *out_data, uint32_t *out_size, int *out_code), {
    const targetUrl = UTF8ToString(url);

    HEAPU32[out_data >> 2] = 0;
    HEAPU32[out_size >> 2] = 0;
    HEAP32[out_code >> 2] = 0;

    try {
        const response = await fetch(targetUrl);
        const bytes = new Uint8Array(await response.arrayBuffer());
        const dataPtr = _malloc(bytes.length + 1);

        if (!dataPtr) {
            HEAP32[out_code >> 2] = 500;
            return -2;
        }

        HEAPU8.set(bytes, dataPtr);
        HEAPU8[dataPtr + bytes.length] = 0;

        HEAPU32[out_data >> 2] = dataPtr;
        HEAPU32[out_size >> 2] = bytes.length;
        HEAP32[out_code >> 2] = response.status || (response.ok ? 200 : 500);
        return 0;
    } catch (error) {
        out(`FETCH_URL(JSPI): fetch failed for ${targetUrl}: ${error}`);
        HEAP32[out_code >> 2] = 500;
        return -1;
    }
});

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

int fetch_url_sync(const char *url, int timeout_ms, fetch_url_result_t *result)
{
    uintptr_t data_ptr = 0;
    uint32_t size = 0;
    int status_code = 0;
    int rc = 0;

    if (!result)
    {
        return -1;
    }

    fetch_url_result_reset(result);
    snprintf(result->url, sizeof(result->url), "%s", url ? url : "");

    log_debug("FETCH_URL(JSPI): Fetching URL: %s", url ? url : "");
    (void)timeout_ms;
    rc = fetch_url_jspi_download(url ? url : "", &data_ptr, &size, &status_code);
    if (rc == 0)
    {
        result->data = (char *)data_ptr;
        result->size = (size_t)size;
        result->code = status_code;
        log_debug("FETCH_URL(JSPI): Fetched %u bytes from %s", size, result->url);
    }
    else
    {
        result->data = NULL;
        result->size = 0;
        result->code = status_code > 0 ? status_code : 500;
        log_warn("FETCH_URL(JSPI): Fetch failed for URL: %s", result->url);
    }

    return result->code;
}

#endif // EMSCRIPTEN
