 #ifndef EMSCRIPTEN

#include "fetch_url.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
    char *data;
    size_t size;
} fetch_context_t;

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

fetch_url_result_t fetch_url_with_path(const char *host_url, const char *relative_path, int timeout_ms)
{
    char full_url[256];
    snprintf(full_url, sizeof(full_url), "%s/%s", host_url, relative_path);
    return fetch_url(full_url, timeout_ms);
}

fetch_url_result_t fetch_url(const char *url, int timeout_ms)
{
    fetch_url_result_t result = {0};

    // save the url
    snprintf(result.url, sizeof(result.url), "%s", url);

    CURL *curl = curl_easy_init();
    if (!curl)
    {
        fprintf(stderr, "Error initializing curl\n");
        result.code = -1;
        return result;
    }

    fetch_context_t context = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);

    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK)
    {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        result.code = http_code;

        if (http_code == 200)
        {
            result.size = context.size;

            if (context.size > (SIZE_MAX - 1))
            {
                result.code = -1;
                result.size = 0;
                free(context.data);
                curl_easy_cleanup(curl);
                return result;
            }

            // add a null terminator to the end of the data
            char *ptr = realloc(context.data, context.size + 1);
            if (!ptr)
            {
                result.code = -1;
                result.size = 0;
                free(context.data);
                context.data = NULL;
            }
            else
            {
                ptr[context.size] = '\0';
            }
            result.data = ptr;
        }
    }
    else
    {
        result.data = NULL;
        result.size = 0;
        result.code = 500; // internal server error?
        free(context.data);
    }

    curl_easy_cleanup(curl);
    return result;
}

#endif // EMSCRIPTEN
