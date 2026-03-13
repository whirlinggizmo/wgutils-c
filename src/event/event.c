#include "event/event.h"

#include "vendor/ee/ee.h"
#include "vendor/list/list.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct event_listener_entry_t {
    event_listener_fn listener;
    void *user_data;
    bool once;
} event_listener_entry_t;

typedef struct event_name_binding_t {
    char *event_name;
    list_t *listeners; /*<event_listener_entry_t *>*/
} event_name_binding_t;

typedef struct event_bridge_payload_t {
    event_bus_t *bus;
    const char *event_name;
    void *payload;
} event_bridge_payload_t;

struct event_bus_t {
    ee_t *emitter;
    list_t *bindings; /*<event_name_binding_t *>*/
};

/* ee callback bridge: receives event_bridge_payload_t */
static void event_bridge_dispatch(void *arg);

static void free_listener(void *value)
{
    event_listener_entry_t *entry = (event_listener_entry_t *)value;
    free(entry);
}

static void free_binding(void *value)
{
    event_name_binding_t *binding = (event_name_binding_t *)value;
    if (binding == NULL) {
        return;
    }
    free(binding->event_name);
    if (binding->listeners != NULL) {
        list_destroy(binding->listeners);
    }
    free(binding);
}

static event_name_binding_t *find_binding(event_bus_t *bus, const char *event_name)
{
    list_iterator_t *it = NULL;
    list_node_t *node = NULL;

    if (bus == NULL || event_name == NULL) {
        return NULL;
    }

    it = list_iterator_new(bus->bindings, LIST_HEAD);
    if (it == NULL) {
        return NULL;
    }

    while ((node = list_iterator_next(it)) != NULL) {
        event_name_binding_t *binding = (event_name_binding_t *)node->val;
        if (binding != NULL && binding->event_name != NULL &&
            strcmp(binding->event_name, event_name) == 0) {
            free(it);
            return binding;
        }
    }

    free(it);
    return NULL;
}

static event_name_binding_t *ensure_binding(event_bus_t *bus, const char *event_name)
{
    event_name_binding_t *binding = find_binding(bus, event_name);
    list_node_t *node = NULL;

    if (binding != NULL) {
        return binding;
    }

    binding = (event_name_binding_t *)calloc(1, sizeof(*binding));
    if (binding == NULL) {
        return NULL;
    }

    binding->event_name = strdup(event_name);
    binding->listeners = list_new();
    if (binding->event_name == NULL || binding->listeners == NULL) {
        free_binding(binding);
        return NULL;
    }
    binding->listeners->free = free_listener;

    node = list_node_new(binding);
    if (node == NULL) {
        free_binding(binding);
        return NULL;
    }
    list_rpush(bus->bindings, node);

    /* One bridge callback per event name. */
    ee_on(bus->emitter, event_name, event_bridge_dispatch);
    return binding;
}

static void event_bridge_dispatch(void *arg)
{
    event_bridge_payload_t *bridge = (event_bridge_payload_t *)arg;
    event_name_binding_t *binding = NULL;
    list_node_t *node = NULL;
    list_node_t *next = NULL;

    if (bridge == NULL || bridge->bus == NULL || bridge->event_name == NULL) {
        return;
    }

    binding = find_binding(bridge->bus, bridge->event_name);
    if (binding == NULL) {
        return;
    }

    node = binding->listeners != NULL ? binding->listeners->head : NULL;
    while (node != NULL) {
        event_listener_entry_t *entry = (event_listener_entry_t *)node->val;
        next = node->next;
        if (entry != NULL && entry->listener != NULL) {
            entry->listener(bridge->payload, entry->user_data);
        }
        if (entry != NULL && entry->once) {
            list_remove(binding->listeners, node);
        }
        node = next;
    }
}

event_bus_t *event_bus_create(void)
{
    event_bus_t *bus = (event_bus_t *)calloc(1, sizeof(*bus));
    if (bus == NULL) {
        return NULL;
    }

    bus->emitter = ee_new();
    bus->bindings = list_new();
    if (bus->emitter == NULL || bus->bindings == NULL) {
        event_bus_destroy(bus);
        return NULL;
    }

    bus->bindings->free = free_binding;
    return bus;
}

void event_bus_destroy(event_bus_t *bus)
{
    if (bus == NULL) {
        return;
    }
    if (bus->bindings != NULL) {
        list_destroy(bus->bindings);
    }
    if (bus->emitter != NULL) {
        ee_destroy(bus->emitter);
    }
    free(bus);
}

static int add_listener_internal(event_bus_t *bus, const char *event_name, event_listener_fn listener,
                                 void *user_data, bool once)
{
    event_name_binding_t *binding = NULL;
    event_listener_entry_t *entry = NULL;
    list_node_t *node = NULL;

    if (bus == NULL || event_name == NULL || event_name[0] == '\0' || listener == NULL) {
        return -1;
    }

    binding = ensure_binding(bus, event_name);
    if (binding == NULL) {
        return -1;
    }

    entry = (event_listener_entry_t *)calloc(1, sizeof(*entry));
    if (entry == NULL) {
        return -1;
    }
    entry->listener = listener;
    entry->user_data = user_data;
    entry->once = once;

    node = list_node_new(entry);
    if (node == NULL) {
        free(entry);
        return -1;
    }
    list_rpush(binding->listeners, node);
    return 0;
}

int event_bus_on(event_bus_t *bus, const char *event_name, event_listener_fn listener, void *user_data)
{
    return add_listener_internal(bus, event_name, listener, user_data, false);
}

int event_bus_once(event_bus_t *bus, const char *event_name, event_listener_fn listener, void *user_data)
{
    return add_listener_internal(bus, event_name, listener, user_data, true);
}

int event_bus_off(event_bus_t *bus, const char *event_name, event_listener_fn listener, void *user_data)
{
    event_name_binding_t *binding = NULL;
    list_node_t *node = NULL;
    list_node_t *next = NULL;

    if (bus == NULL || event_name == NULL || listener == NULL) {
        return -1;
    }

    binding = find_binding(bus, event_name);
    if (binding == NULL) {
        return 0;
    }

    node = binding->listeners != NULL ? binding->listeners->head : NULL;
    while (node != NULL) {
        event_listener_entry_t *entry = (event_listener_entry_t *)node->val;
        next = node->next;
        if (entry != NULL && entry->listener == listener && entry->user_data == user_data) {
            list_remove(binding->listeners, node);
            break;
        }
        node = next;
    }
    return 0;
}

int event_bus_off_all(event_bus_t *bus, const char *event_name)
{
    event_name_binding_t *binding = NULL;
    list_node_t *node = NULL;
    list_node_t *next = NULL;

    if (bus == NULL || event_name == NULL) {
        return -1;
    }

    binding = find_binding(bus, event_name);
    if (binding == NULL) {
        return 0;
    }

    /* Keep vendor ee behavior safe: remove the single bridge callback directly. */
    ee_remove_listener(bus->emitter, event_name, event_bridge_dispatch);

    node = bus->bindings != NULL ? bus->bindings->head : NULL;
    while (node != NULL) {
        event_name_binding_t *iter_binding = (event_name_binding_t *)node->val;
        next = node->next;
        if (iter_binding != NULL && iter_binding->event_name != NULL &&
            strcmp(iter_binding->event_name, event_name) == 0) {
            list_remove(bus->bindings, node);
            break;
        }
        node = next;
    }
    return 0;
}

int event_bus_emit(event_bus_t *bus, const char *event_name, void *payload)
{
    event_bridge_payload_t bridge = {0};

    if (bus == NULL || event_name == NULL) {
        return -1;
    }

    bridge.bus = bus;
    bridge.event_name = event_name;
    bridge.payload = payload;
    ee_emit(bus->emitter, event_name, &bridge);
    return 0;
}

int event_bus_listener_count(event_bus_t *bus, const char *event_name)
{
    event_name_binding_t *binding = NULL;
    if (bus == NULL || event_name == NULL) {
        return -1;
    }
    binding = find_binding(bus, event_name);
    if (binding == NULL || binding->listeners == NULL) {
        return 0;
    }
    return binding->listeners->len;
}
