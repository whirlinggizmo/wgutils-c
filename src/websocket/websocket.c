#ifndef EMSCRIPTEN

#include "websocket.h"
#include "logger/logger.h"

#include <arpa/inet.h>
#include <curl/curl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define WEBSOCKET_RECV_BUFFER_SIZE (64 * 1024)
#define WEBSOCKET_DEFAULT_CLOSE_CODE 1000

struct websocket
{
    CURL *curl;
    pthread_t worker;
    pthread_mutex_t mutex;
    char url[WEBSOCKET_MAX_URL_LENGTH];
    char error_buffer[CURL_ERROR_SIZE];
    char requested_close_reason[128];
    bool connected;
    bool worker_started;
    bool close_requested;
    bool destroy_requested;
    bool close_emitted;
    bool error_emitted;
    int requested_close_code;
    websocket_callbacks_t callbacks;
    void *user_data;
    struct websocket_event *event_head;
    struct websocket_event *event_tail;
};

static pthread_once_t websocket_curl_once = PTHREAD_ONCE_INIT;

typedef enum websocket_event_type
{
    WEBSOCKET_EVENT_OPEN = 1,
    WEBSOCKET_EVENT_CLOSE = 2,
    WEBSOCKET_EVENT_ERROR = 3,
    WEBSOCKET_EVENT_MESSAGE = 4
} websocket_event_type_t;

typedef struct websocket_event
{
    websocket_event_type_t type;
    int code;
    bool is_text;
    int len;
    char reason[128];
    char *message;
    struct websocket_event *next;
} websocket_event_t;

static void websocket_curl_global_init(void)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

static void websocket_enqueue_event_locked(websocket_t *ws, websocket_event_type_t type,
                                           int code, const char *reason,
                                           const char *message, int len,
                                           bool is_text)
{
    websocket_event_t *event = NULL;

    if (ws == NULL) {
        return;
    }

    event = (websocket_event_t *)calloc(1, sizeof(*event));
    if (event == NULL) {
        return;
    }

    event->type = type;
    event->code = code;
    event->is_text = is_text;
    event->len = len;

    if (reason != NULL) {
        snprintf(event->reason, sizeof(event->reason), "%s", reason);
    }

    if (message != NULL && len > 0) {
        event->message = (char *)malloc((size_t)len);
        if (event->message == NULL) {
            free(event);
            return;
        }
        memcpy(event->message, message, (size_t)len);
    }

    if (ws->event_tail == NULL) {
        ws->event_head = event;
        ws->event_tail = event;
    } else {
        ws->event_tail->next = event;
        ws->event_tail = event;
    }
}

static void websocket_emit_error_locked(websocket_t *ws)
{
    if (ws == NULL || ws->error_emitted) {
        return;
    }

    ws->error_emitted = true;
    websocket_enqueue_event_locked(ws, WEBSOCKET_EVENT_ERROR, 0, NULL, NULL, 0, false);
}

static void websocket_emit_close_locked(websocket_t *ws, int code, const char *reason)
{
    if (ws == NULL || ws->close_emitted) {
        return;
    }

    (void)code;
    (void)reason;
    ws->close_emitted = true;
    ws->connected = false;
    websocket_enqueue_event_locked(ws, WEBSOCKET_EVENT_CLOSE, code, reason, NULL, 0, false);
}

static int websocket_get_socket_fd(websocket_t *ws)
{
    curl_socket_t sockfd = CURL_SOCKET_BAD;

    if (ws == NULL || ws->curl == NULL) {
        return -1;
    }

    if (curl_easy_getinfo(ws->curl, CURLINFO_ACTIVESOCKET, &sockfd) != CURLE_OK ||
        sockfd == CURL_SOCKET_BAD) {
        return -1;
    }

    return (int)sockfd;
}

static int websocket_wait_socket(websocket_t *ws, bool want_read, bool want_write)
{
    int sockfd = websocket_get_socket_fd(ws);
    fd_set readfds;
    fd_set writefds;

    if (sockfd < 0) {
        return -1;
    }

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    if (want_read) {
        FD_SET(sockfd, &readfds);
    }

    if (want_write) {
        FD_SET(sockfd, &writefds);
    }

    return select(sockfd + 1,
                  want_read ? &readfds : NULL,
                  want_write ? &writefds : NULL,
                  NULL,
                  NULL);
}

static int websocket_send_frame_locked(websocket_t *ws, const void *data, size_t len,
                                       unsigned int flags)
{
    CURLcode result = CURLE_OK;
    const unsigned char *cursor = (const unsigned char *)data;
    size_t remaining = len;

    if (ws == NULL || ws->curl == NULL || !ws->connected) {
        return -1;
    }

    if (data == NULL && len != 0) {
        return -1;
    }

    while (remaining > 0 || len == 0) {
        size_t sent = 0;
        result = curl_ws_send(ws->curl, cursor, remaining, &sent, 0, flags);
        if (result == CURLE_OK) {
            if (len == 0) {
                return 0;
            }

            cursor += sent;
            remaining -= sent;
            if (remaining == 0) {
                return 0;
            }
            continue;
        }

        if (result == CURLE_AGAIN) {
            if (websocket_wait_socket(ws, false, true) <= 0) {
                return -1;
            }
            continue;
        }

        log_error("WEBSOCKET: send failed on %s: %s",
                  ws->url, curl_easy_strerror(result));
        return -1;
    }

    return 0;
}

static void websocket_send_requested_close_locked(websocket_t *ws)
{
    unsigned char payload[130] = {0};
    size_t payload_len = 0;
    uint16_t net_code = 0;

    if (ws == NULL || !ws->close_requested || !ws->connected) {
        return;
    }

    net_code = htons((uint16_t)ws->requested_close_code);
    memcpy(payload, &net_code, sizeof(net_code));
    payload_len = sizeof(net_code);

    if (ws->requested_close_reason[0] != '\0') {
        size_t reason_len = strlen(ws->requested_close_reason);
        if (reason_len > sizeof(payload) - payload_len - 1) {
            reason_len = sizeof(payload) - payload_len - 1;
        }
        memcpy(payload + payload_len, ws->requested_close_reason, reason_len);
        payload_len += reason_len;
    }

    (void)websocket_send_frame_locked(ws, payload, payload_len, CURLWS_CLOSE);
}

static void websocket_handle_close_frame(websocket_t *ws, const unsigned char *data,
                                         size_t len)
{
    int close_code = WEBSOCKET_DEFAULT_CLOSE_CODE;
    char reason[128] = {0};

    if (len >= 2) {
        uint16_t net_code = 0;
        memcpy(&net_code, data, sizeof(net_code));
        close_code = (int)ntohs(net_code);
        if (len > 2) {
            size_t reason_len = len - 2;
            if (reason_len >= sizeof(reason)) {
                reason_len = sizeof(reason) - 1;
            }
            memcpy(reason, data + 2, reason_len);
            reason[reason_len] = '\0';
        }
    } else if (ws->close_requested) {
        close_code = ws->requested_close_code;
        snprintf(reason, sizeof(reason), "%s", ws->requested_close_reason);
    }

    log_debug("WEBSOCKET: closed by peer %s code=%d reason=%s",
              ws->url, close_code, reason);
    pthread_mutex_lock(&ws->mutex);
    websocket_emit_close_locked(ws, close_code, reason);
    pthread_mutex_unlock(&ws->mutex);
    if (ws->callbacks.on_close != NULL) {
        ws->callbacks.on_close(ws, close_code, reason, ws->user_data);
    }
}

static void *websocket_worker_main(void *user_data)
{
    websocket_t *ws = (websocket_t *)user_data;
    CURLcode result = CURLE_OK;

    pthread_mutex_lock(&ws->mutex);
    result = curl_easy_perform(ws->curl);
    if (result != CURLE_OK) {
        log_error("WEBSOCKET: connect failed for %s: %s",
                  ws->url,
                  ws->error_buffer[0] ? ws->error_buffer : curl_easy_strerror(result));
        websocket_emit_error_locked(ws);
        websocket_emit_close_locked(ws, 1006, "connect failed");
        pthread_mutex_unlock(&ws->mutex);
        return NULL;
    }

    ws->connected = true;
    websocket_enqueue_event_locked(ws, WEBSOCKET_EVENT_OPEN, 0, NULL, NULL, 0, false);
    if (ws->close_requested) {
        websocket_send_requested_close_locked(ws);
    }
    pthread_mutex_unlock(&ws->mutex);

    log_debug("WEBSOCKET: Connected to %s", ws->url);

    while (true) {
        unsigned char buffer[WEBSOCKET_RECV_BUFFER_SIZE];
        const struct curl_ws_frame *meta = NULL;
        size_t recv_len = 0;

        pthread_mutex_lock(&ws->mutex);
        if (ws->destroy_requested) {
            pthread_mutex_unlock(&ws->mutex);
            break;
        }

        result = curl_ws_recv(ws->curl, buffer, sizeof(buffer), &recv_len, &meta);
        if (result == CURLE_OK) {
            if (meta != NULL) {
                if (meta->flags & CURLWS_CLOSE) {
                    pthread_mutex_unlock(&ws->mutex);
                    websocket_handle_close_frame(ws, buffer, recv_len);
                    break;
                }

                if ((meta->flags & CURLWS_TEXT) || (meta->flags & CURLWS_BINARY)) {
                    bool is_text = (meta->flags & CURLWS_TEXT) != 0;
                    websocket_enqueue_event_locked(ws, WEBSOCKET_EVENT_MESSAGE, 0, NULL,
                                                   (const char *)buffer, (int)recv_len,
                                                   is_text);
                    pthread_mutex_unlock(&ws->mutex);
                    continue;
                }
            }
            pthread_mutex_unlock(&ws->mutex);

            continue;
        }

        if (result == CURLE_AGAIN) {
            pthread_mutex_unlock(&ws->mutex);
            if (websocket_wait_socket(ws, true, false) <= 0) {
                pthread_mutex_lock(&ws->mutex);
                websocket_emit_error_locked(ws);
                websocket_emit_close_locked(ws, 1006, "socket wait failed");
                pthread_mutex_unlock(&ws->mutex);
                break;
            }
            continue;
        }

        pthread_mutex_unlock(&ws->mutex);

        if (result == CURLE_GOT_NOTHING) {
            int code = 1006;
            const char *reason = "connection closed";
            pthread_mutex_lock(&ws->mutex);
            if (!ws->close_emitted) {
                code = ws->close_requested ? ws->requested_close_code : 1006;
                reason =
                    ws->close_requested ? ws->requested_close_reason : "connection closed";
                websocket_emit_close_locked(ws, code, reason);
            }
            pthread_mutex_unlock(&ws->mutex);
            break;
        }

        log_error("WEBSOCKET: receive failed on %s: %s",
                  ws->url, curl_easy_strerror(result));
        pthread_mutex_lock(&ws->mutex);
        websocket_emit_error_locked(ws);
        websocket_emit_close_locked(ws, 1006, "receive failed");
        pthread_mutex_unlock(&ws->mutex);
        break;
    }

    ws->connected = false;
    return NULL;
}

websocket_t *websocket_create(const char *url,
                              const websocket_callbacks_t *callbacks,
                              void *user_data)
{
    websocket_t *ws = NULL;

    if (url == NULL) {
        return NULL;
    }

    pthread_once(&websocket_curl_once, websocket_curl_global_init);

    ws = (websocket_t *)calloc(1, sizeof(websocket_t));
    if (ws == NULL) {
        return NULL;
    }

    if (pthread_mutex_init(&ws->mutex, NULL) != 0) {
        free(ws);
        return NULL;
    }

    ws->curl = curl_easy_init();
    if (ws->curl == NULL) {
        pthread_mutex_destroy(&ws->mutex);
        free(ws);
        return NULL;
    }

    snprintf(ws->url, sizeof(ws->url), "%s", url);
    ws->user_data = user_data;
    ws->requested_close_code = WEBSOCKET_DEFAULT_CLOSE_CODE;
    snprintf(ws->requested_close_reason, sizeof(ws->requested_close_reason), "%s", "client closing");

    if (callbacks != NULL) {
        ws->callbacks = *callbacks;
    }

    {
        const long connect_only_mode = 2L;
        curl_easy_setopt(ws->curl, CURLOPT_URL, ws->url);
        curl_easy_setopt(ws->curl, CURLOPT_CONNECT_ONLY, connect_only_mode);
    }
    curl_easy_setopt(ws->curl, CURLOPT_ERRORBUFFER, ws->error_buffer);

    if (pthread_create(&ws->worker, NULL, websocket_worker_main, ws) != 0) {
        curl_easy_cleanup(ws->curl);
        pthread_mutex_destroy(&ws->mutex);
        free(ws);
        return NULL;
    }

    ws->worker_started = true;
    log_debug("WEBSOCKET: Connecting to %s...", ws->url);
    return ws;
}

void websocket_poll(websocket_t *ws)
{
    websocket_event_t *event = NULL;

    if (ws == NULL) {
        return;
    }

    while (true) {
        pthread_mutex_lock(&ws->mutex);
        event = ws->event_head;
        if (event != NULL) {
            ws->event_head = event->next;
            if (ws->event_head == NULL) {
                ws->event_tail = NULL;
            }
        }
        pthread_mutex_unlock(&ws->mutex);

        if (event == NULL) {
            break;
        }

        switch (event->type) {
        case WEBSOCKET_EVENT_OPEN:
            if (ws->callbacks.on_open != NULL) {
                ws->callbacks.on_open(ws, ws->user_data);
            }
            break;
        case WEBSOCKET_EVENT_CLOSE:
            if (ws->callbacks.on_close != NULL) {
                ws->callbacks.on_close(ws, event->code, event->reason, ws->user_data);
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            if (ws->callbacks.on_error != NULL) {
                ws->callbacks.on_error(ws, ws->user_data);
            }
            break;
        case WEBSOCKET_EVENT_MESSAGE:
            if (ws->callbacks.on_message != NULL) {
                ws->callbacks.on_message(ws, event->message, event->len, event->is_text,
                                         ws->user_data);
            }
            break;
        default:
            break;
        }

        free(event->message);
        free(event);
    }
}

void websocket_destroy(websocket_t *ws)
{
    websocket_event_t *event = NULL;

    if (ws == NULL) {
        return;
    }

    websocket_close(ws, WEBSOCKET_DEFAULT_CLOSE_CODE, "Client closing");

    pthread_mutex_lock(&ws->mutex);
    ws->destroy_requested = true;
    {
        int sockfd = websocket_get_socket_fd(ws);
        if (sockfd >= 0) {
            shutdown(sockfd, SHUT_RDWR);
        }
    }
    pthread_mutex_unlock(&ws->mutex);

    if (ws->worker_started) {
        pthread_join(ws->worker, NULL);
    }

    pthread_mutex_lock(&ws->mutex);
    event = ws->event_head;
    ws->event_head = NULL;
    ws->event_tail = NULL;
    pthread_mutex_unlock(&ws->mutex);

    while (event != NULL) {
        websocket_event_t *next = event->next;
        free(event->message);
        free(event);
        event = next;
    }

    if (ws->curl != NULL) {
        curl_easy_cleanup(ws->curl);
    }

    pthread_mutex_destroy(&ws->mutex);
    free(ws);
}

int websocket_send_text(websocket_t *ws, const char *text)
{
    int rc = -1;

    if (ws == NULL || text == NULL) {
        return -1;
    }

    pthread_mutex_lock(&ws->mutex);
    rc = websocket_send_frame_locked(ws, text, strlen(text), CURLWS_TEXT);
    pthread_mutex_unlock(&ws->mutex);
    return rc;
}

int websocket_send_binary(websocket_t *ws, const void *data, size_t len)
{
    int rc = -1;

    if (ws == NULL || data == NULL || len == 0) {
        return -1;
    }

    pthread_mutex_lock(&ws->mutex);
    rc = websocket_send_frame_locked(ws, data, len, CURLWS_BINARY);
    pthread_mutex_unlock(&ws->mutex);
    return rc;
}

void websocket_close(websocket_t *ws, int code, const char *reason)
{
    if (ws == NULL) {
        return;
    }

    pthread_mutex_lock(&ws->mutex);
    ws->close_requested = true;
    ws->requested_close_code = code > 0 ? code : WEBSOCKET_DEFAULT_CLOSE_CODE;
    snprintf(ws->requested_close_reason, sizeof(ws->requested_close_reason), "%s",
             reason ? reason : "");

    if (ws->connected) {
        websocket_send_requested_close_locked(ws);
    }
    pthread_mutex_unlock(&ws->mutex);
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
