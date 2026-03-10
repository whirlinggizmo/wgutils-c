#include "event/event.h"
#include "test_common.h"

typedef struct event_test_payload_t {
    int value;
} event_test_payload_t;

static void accumulate_value(void *payload, void *user_data)
{
    event_test_payload_t *typed_payload = (event_test_payload_t *)payload;
    int *sum = (int *)user_data;
    if (typed_payload != NULL && sum != NULL) {
        *sum += typed_payload->value;
    }
}

static void increment_counter(void *payload, void *user_data)
{
    int *counter = (int *)user_data;
    (void)payload;
    if (counter != NULL) {
        *counter += 1;
    }
}

int event_test_run(void)
{
    event_bus_t *bus = NULL;
    event_test_payload_t payload = {0};
    int sum_a = 0;
    int sum_b = 0;
    int once_count = 0;
    int off_count_a = 0;
    int off_count_b = 0;
    int cleared_count = 0;

    bus = event_bus_create();
    TEST_ASSERT(bus != NULL);

    payload.value = 3;
    TEST_ASSERT(event_bus_on(bus, "tick", accumulate_value, &sum_a) == 0);
    TEST_ASSERT(event_bus_listener_count(bus, "tick") == 1);
    TEST_ASSERT(event_bus_emit(bus, "tick", &payload) == 0);
    TEST_ASSERT(sum_a == 3);

    payload.value = 2;
    TEST_ASSERT(event_bus_on(bus, "tick", accumulate_value, &sum_b) == 0);
    TEST_ASSERT(event_bus_listener_count(bus, "tick") == 2);
    TEST_ASSERT(event_bus_emit(bus, "tick", &payload) == 0);
    TEST_ASSERT(sum_a == 5);
    TEST_ASSERT(sum_b == 2);

    TEST_ASSERT(event_bus_once(bus, "once", increment_counter, &once_count) == 0);
    TEST_ASSERT(event_bus_emit(bus, "once", NULL) == 0);
    TEST_ASSERT(event_bus_emit(bus, "once", NULL) == 0);
    TEST_ASSERT(once_count == 1);
    TEST_ASSERT(event_bus_listener_count(bus, "once") == 0);

    TEST_ASSERT(event_bus_on(bus, "off", increment_counter, &off_count_a) == 0);
    TEST_ASSERT(event_bus_on(bus, "off", increment_counter, &off_count_b) == 0);
    TEST_ASSERT(event_bus_listener_count(bus, "off") == 2);
    TEST_ASSERT(event_bus_off(bus, "off", increment_counter, &off_count_a) == 0);
    TEST_ASSERT(event_bus_listener_count(bus, "off") == 1);
    TEST_ASSERT(event_bus_emit(bus, "off", NULL) == 0);
    TEST_ASSERT(off_count_a == 0);
    TEST_ASSERT(off_count_b == 1);

    TEST_ASSERT(event_bus_on(bus, "clear", increment_counter, &cleared_count) == 0);
    TEST_ASSERT(event_bus_on(bus, "clear", increment_counter, &cleared_count) == 0);
    TEST_ASSERT(event_bus_listener_count(bus, "clear") == 2);
    TEST_ASSERT(event_bus_off_all(bus, "clear") == 0);
    TEST_ASSERT(event_bus_listener_count(bus, "clear") == 0);
    TEST_ASSERT(event_bus_emit(bus, "clear", NULL) == 0);
    TEST_ASSERT(cleared_count == 0);

    TEST_ASSERT(event_bus_off_all(bus, "missing_event") == 0);

    event_bus_destroy(bus);
    return 0;
}
