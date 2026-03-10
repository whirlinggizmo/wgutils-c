#ifndef WGUTILS_EVENT_H
#define WGUTILS_EVENT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct event_bus_t event_bus_t;
typedef void (*event_listener_fn)(void *payload, void *user_data);

event_bus_t *event_bus_create(void);
void event_bus_destroy(event_bus_t *bus);

int event_bus_on(event_bus_t *bus, const char *event_name, event_listener_fn listener, void *user_data);
int event_bus_once(event_bus_t *bus, const char *event_name, event_listener_fn listener, void *user_data);
int event_bus_off(event_bus_t *bus, const char *event_name, event_listener_fn listener, void *user_data);
int event_bus_off_all(event_bus_t *bus, const char *event_name);

int event_bus_emit(event_bus_t *bus, const char *event_name, void *payload);
int event_bus_listener_count(event_bus_t *bus, const char *event_name);

#ifdef __cplusplus
}
#endif

#endif // WGUTILS_EVENT_H
