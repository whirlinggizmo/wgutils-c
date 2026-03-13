#ifdef EMSCRIPTEN

#include "websocket.h"
#include "logger/logger.h"

#include <emscripten/websocket.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct websocket
{
    EMSCRIPTEN_WEBSOCKET_T socket;
    char url[WEBSOCKET_MAX_URL_LENGTH];
    bool connected;
    websocket_callbacks_t callbacks;
    void *user_data;
};

static EM_BOOL ws_cb_on_open(int event_type, const EmscriptenWebSocketOpenEvent *event, void *user_data)
{
    websocket_t *ws = (websocket_t *)user_data;
    (void)event_type;
    (void)event;

    ws->connected = true;
    log_debug("WEBSOCKET: Connected to %s", ws->url);

    if (ws->callbacks.on_open != NULL) {
        ws->callbacks.on_open(ws, ws->user_data);
    }

    return EM_TRUE;
}

static EM_BOOL ws_cb_on_error(int event_type, const EmscriptenWebSocketErrorEvent *event, void *user_data)
{
    websocket_t *ws = (websocket_t *)user_data;
    (void)event_type;
    (void)event;

    log_warn("WEBSOCKET: Error on %s", ws->url);
    ws->connected = false;

    if (ws->callbacks.on_error != NULL) {
        ws->callbacks.on_error(ws, ws->user_data);
    }

    return EM_TRUE;
}

static EM_BOOL ws_cb_on_close(int event_type, const EmscriptenWebSocketCloseEvent *event, void *user_data)
{
    websocket_t *ws = (websocket_t *)user_data;
    (void)event_type;

    log_debug("WEBSOCKET: Closed: code=%d, reason=%s", event->code, event->reason);
    ws->connected = false;

    if (ws->callbacks.on_close != NULL) {
        ws->callbacks.on_close(ws, event->code, event->reason, ws->user_data);
    }

    return EM_TRUE;
}

static EM_BOOL ws_cb_on_message(int event_type, const EmscriptenWebSocketMessageEvent *event, void *user_data)
{
    websocket_t *ws = (websocket_t *)user_data;
    (void)event_type;

    if (ws->callbacks.on_message != NULL) {
        ws->callbacks.on_message(ws, (const char *)event->data, event->numBytes,
                                  event->isText, ws->user_data);
    }

    return EM_TRUE;
}

websocket_t *websocket_create(const char *url,
                              const websocket_callbacks_t *callbacks,
                              void *user_data)
{
    websocket_t *ws = NULL;
    EmscriptenWebSocketCreateAttributes attrs;

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

    emscripten_websocket_init_create_attributes(&attrs);
    attrs.url = ws->url;

    ws->socket = emscripten_websocket_new(&attrs);
    if (ws->socket <= 0) {
        log_error("WEBSOCKET: Failed to create websocket for %s", url);
        free(ws);
        return NULL;
    }

    emscripten_websocket_set_onopen_callback(ws->socket, ws, ws_cb_on_open);
    emscripten_websocket_set_onerror_callback(ws->socket, ws, ws_cb_on_error);
    emscripten_websocket_set_onclose_callback(ws->socket, ws, ws_cb_on_close);
    emscripten_websocket_set_onmessage_callback(ws->socket, ws, ws_cb_on_message);

    log_debug("WEBSOCKET: Connecting to %s...", url);
    return ws;
}

void websocket_poll(websocket_t *ws)
{
    (void)ws;
}

void websocket_destroy(websocket_t *ws)
{
    if (ws == NULL) {
        return;
    }

    if (ws->socket > 0) {
        emscripten_websocket_close(ws->socket, 1000, "Client closing");
        emscripten_websocket_delete(ws->socket);
    }

    free(ws);
}

int websocket_send_text(websocket_t *ws, const char *text)
{
    EMSCRIPTEN_RESULT result;

    if (ws == NULL || text == NULL || !ws->connected) {
        return -1;
    }

    result = emscripten_websocket_send_utf8_text(ws->socket, text);
    return result < 0 ? -1 : 0;
}

int websocket_send_binary(websocket_t *ws, const void *data, size_t len)
{
    EMSCRIPTEN_RESULT result;

    if (ws == NULL || data == NULL || len == 0 || !ws->connected) {
        return -1;
    }

    result = emscripten_websocket_send_binary(ws->socket, (void *)data, (uint32_t)len);
    return result < 0 ? -1 : 0;
}

void websocket_close(websocket_t *ws, int code, const char *reason)
{
    if (ws == NULL || ws->socket <= 0) {
        return;
    }

    emscripten_websocket_close(ws->socket, (unsigned short)code, reason ? reason : "");
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
