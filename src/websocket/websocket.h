#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WEBSOCKET_MAX_URL_LENGTH 256

typedef struct websocket websocket_t;

typedef struct
{
    void (*on_open)(websocket_t *ws, void *user_data);
    void (*on_close)(websocket_t *ws, int code, const char *reason, void *user_data);
    void (*on_error)(websocket_t *ws, void *user_data);
    void (*on_message)(websocket_t *ws, const char *data, int len, bool is_text, void *user_data);
} websocket_callbacks_t;

websocket_t *websocket_create(const char *url,
                               const websocket_callbacks_t *callbacks,
                               void *user_data);
void websocket_destroy(websocket_t *ws);
int websocket_send_text(websocket_t *ws, const char *text);
int websocket_send_binary(websocket_t *ws, const void *data, size_t len);
void websocket_close(websocket_t *ws, int code, const char *reason);
bool websocket_is_connected(const websocket_t *ws);
const char *websocket_get_url(const websocket_t *ws);

#ifdef __cplusplus
}
#endif

#endif // WEBSOCKET_H
