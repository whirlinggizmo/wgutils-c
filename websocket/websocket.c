#ifndef EMSCRIPTEN

#include "websocket.h"
#include "logger/log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct websocket
{
    char url[WEBSOCKET_MAX_URL_LENGTH];
    bool connected;
    websocket_callbacks_t callbacks;
    void *user_data;
};

websocket_t *websocket_create(const char *url,
                                     const websocket_callbacks_t *callbacks,
                                     void *user_data)
{
    websocket_t *ws = NULL;

    if (url == NULL) {
        return NULL;
    }

    ws = (websocket_t *)calloc(1, sizeof(websocket_t));
    if (ws == NULL) {
        return NULL;
    }

    snprintf(ws->url, sizeof(ws->url), "%s", url);
    ws->connected = false;
    ws->user_data = user_data;

    if (callbacks != NULL) {
        ws->callbacks = *callbacks;
    }

    log_warn("WEBSOCKET: Desktop websocket not implemented yet (url: %s)", url);
    return ws;
}

void websocket_destroy(websocket_t *ws)
{
    if (ws == NULL) {
        return;
    }
    free(ws);
}

int websocket_send_text(websocket_t *ws, const char *text)
{
    (void)text;
    if (ws == NULL || !ws->connected) {
        return -1;
    }
    return -1;
}

int websocket_send_binary(websocket_t *ws, const void *data, size_t len)
{
    (void)data;
    (void)len;
    if (ws == NULL || !ws->connected) {
        return -1;
    }
    return -1;
}

void websocket_close(websocket_t *ws, int code, const char *reason)
{
    (void)code;
    (void)reason;
    if (ws == NULL) {
        return;
    }
    ws->connected = false;
}

bool websocket_is_connected(const websocket_t *ws)
{
    if (ws == NULL) {
        return false;
    }
    return ws->connected;
}

const char *websocket_get_url(const websocket_t *ws)
{
    if (ws == NULL) {
        return NULL;
    }
    return ws->url;
}

#endif // EMSCRIPTEN
