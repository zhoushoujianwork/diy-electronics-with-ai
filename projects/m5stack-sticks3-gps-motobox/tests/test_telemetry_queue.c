#include <assert.h>
#include <stdio.h>

#include "telemetry_queue.h"

int main(void)
{
    telemetry_queue_t queue;
    telemetry_queue_init(&queue);
    for (unsigned i = 1; i <= TELEMETRY_QUEUE_CAPACITY; ++i) {
        char payload[32];
        snprintf(payload, sizeof(payload), "{\"seq\":%u}", i);
        telemetry_queue_push(&queue, i, payload);
    }
    assert(telemetry_queue_count(&queue) == TELEMETRY_QUEUE_CAPACITY);
    telemetry_queue_push(&queue, 121, "{\"seq\":121}");
    assert(telemetry_queue_count(&queue) == TELEMETRY_QUEUE_CAPACITY);
    assert(telemetry_queue_dropped(&queue) == 1);
    assert(telemetry_queue_head(&queue)->sequence == 2);

    assert(telemetry_queue_mark_published(&queue, 42));
    telemetry_queue_push(&queue, 122, "{\"seq\":122}");
    assert(telemetry_queue_count(&queue) == TELEMETRY_QUEUE_CAPACITY);
    assert(telemetry_queue_dropped(&queue) == 2);
    assert(telemetry_queue_head(&queue)->sequence == 2);
    assert(telemetry_queue_head(&queue)->in_flight);
    assert(!telemetry_queue_ack(&queue, 41));
    assert(telemetry_queue_ack(&queue, 42));
    assert(telemetry_queue_head(&queue)->sequence == 4);
    assert(queue.items[(queue.head + queue.count - 1) % TELEMETRY_QUEUE_CAPACITY].sequence == 122);
    telemetry_queue_retry_inflight(&queue);

    puts("telemetry queue tests passed");
    return 0;
}
