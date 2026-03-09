#ifdef EMSCRIPTEN

#include "fetch_url.h"
#include <emscripten/fetch.h>
#include <emscripten.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define FETCH_SLEEP_MS 1 // Sleep interval in milliseconds

// Context to store fetch results
typedef struct
{
    char *data;     // Pointer to the fetched content
    size_t size;    // Size of the fetched content
    bool completed; // Flag to indicate if the fetch is complete
    int error;      // Error code if any
    int code;       // http code
} fetch_context_t;

// Success callback
void fetch_success(emscripten_fetch_t *fetch)
{
    fetch_context_t *context = (fetch_context_t *)fetch->userData;

    // Allocate memory for the fetched content
    context->data = (char *)malloc(fetch->numBytes + 1);
    if (context->data == NULL)
    {
        printf("Failed to allocate memory for fetched content.\n");
        context->data = NULL; // Indicate failure
        context->size = 0;
        context->error = -1;
        context->code = 500;
        context->completed = true; // Mark the fetch as complete
        emscripten_fetch_close(fetch);
        return;
    }

    memcpy(context->data, fetch->data, fetch->numBytes);
    context->data[fetch->numBytes] = '\0'; // Null-terminate the string
    context->size = fetch->numBytes;
    context->error = 0; 
    context->code = fetch->status;
    printf("Fetched %llu bytes from %s\n", fetch->numBytes, fetch->url);

    context->completed = true; // Mark the fetch as complete

    // fetch_close does the freeing for us, so we don't need to do it here.
    emscripten_fetch_close(fetch);
}

// Error callback
void fetch_error(emscripten_fetch_t *fetch)
{
    fetch_context_t *context = (fetch_context_t *)fetch->userData;

    printf("Fetch failed: HTTP status %d, URL: %s\n", fetch->status, fetch->url);
    context->data = NULL; // Indicate failure
    context->size = 0;
    // if there was a network error, return it  (or -2 if we don't know)
    context->error = -2;
    context->code = fetch->status;
    context->completed = true; // Mark the fetch as complete

    emscripten_fetch_close(fetch);
}

fetch_url_result_t fetch_url_with_path(const char *host_url, const char *relative_path, int timeout_ms)
{
    char full_url[256];
    snprintf(full_url, sizeof(full_url), "%s/%s", host_url, relative_path);
    return fetch_url(full_url, timeout_ms);
}

// Fetch URL function with timeout support
fetch_url_result_t fetch_url(const char *url, int timeout_ms)
{
    fetch_context_t context = {0};
    fetch_url_result_t result = {0};
    context.completed = false;

    // save the url
    snprintf(result.url, sizeof(result.url), "%s", url);

    // Configure the fetch
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    // attr.destinationPath = "/scripts/fetch/data.bin"; // Store the fetched content in memory
    attr.onsuccess = fetch_success;
    attr.onerror = fetch_error;
    attr.userData = &context;

    // Start the fetch
    printf("Fetching URL: %s...\n", url);
    emscripten_fetch(&attr, url);

    // Wait for the fetch to complete with timeout
    int elapsed_time = 0;
    while (!context.completed && elapsed_time < timeout_ms) // TODO: use emscripten attributes to handle timeout
    {
        emscripten_sleep(FETCH_SLEEP_MS); // Non-blocking sleep
        elapsed_time += FETCH_SLEEP_MS;
    }

    // Check if the fetch timed out
    if (!context.completed)
    {
        printf("Fetch timed out after %d ms: %s\n", timeout_ms, url);
        result.code = 500;
        return result;
    }

    result.size = context.size;
    result.code = context.code;

    if (context.error != 0)
    {
        result.data = NULL;
        result.size = 0;
        result.code = context.error;
        return result;
    }
    else
    {
        if (context.size > (SIZE_MAX - 1))
        {
            result.code = -1;
            result.size = 0;
            free(context.data);
            return result;
        }

        // add a null terminator to the end of the data
        char *ptr = realloc(context.data, context.size + 1);
        if (!ptr)
        {
            result.code = -1;
            result.size = 0;
            free(context.data);
            return result;
        }
        ptr[context.size] = '\0';
        result.size = context.size;
        result.data = ptr;
        result.code = context.code;
    }

    return result;
}

#endif // EMSCRIPTEN
