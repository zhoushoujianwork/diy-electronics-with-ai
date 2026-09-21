#include "telemetry_queue.h"

#include <string.h>

void telemetry_queue_init(telemetry_queue_t *queue)
{
    memset(queue, 0, sizeof(*queue));
}

void telemetry_queue_push(telemetry_queue_t *queue, uint64_t sequence, const char *payload)
{
    if (queue->count == TELEMETRY_QUEUE_CAPACITY) {
        size_t drop_offset = 0;
        while (drop_offset < queue->count &&
               queue->items[(queue->head + drop_offset) % TELEMETRY_QUEUE_CAPACITY].in_flight) {
            drop_offset++;
        }
        if (drop_offset == queue->count) {
            queue->dropped++;
            return;
        }
        for (size_t offset = drop_offset; offset + 1 < queue->count; ++offset) {
            size_t destination = (queue->head + offset) % TELEMETRY_QUEUE_CAPACITY;
            size_t source = (queue->head + offset + 1) % TELEMETRY_QUEUE_CAPACITY;
            queue->items[destination] = queue->items[source];
        }
        size_t old_tail = (queue->head + queue->count - 1) % TELEMETRY_QUEUE_CAPACITY;
        memset(&queue->items[old_tail], 0, sizeof(queue->items[old_tail]));
        queue->count--;
        queue->dropped++;
    }
    size_t index = (queue->head + queue->count) % TELEMETRY_QUEUE_CAPACITY;
    telemetry_item_t *item = &queue->items[index];
    memset(item, 0, sizeof(*item));
    item->sequence = sequence;
    strncpy(item->payload, payload, sizeof(item->payload) - 1);
    queue->count++;
}

telemetry_item_t *telemetry_queue_head(telemetry_queue_t *queue)
{
    return queue->count ? &queue->items[queue->head] : NULL;
}

bool telemetry_queue_mark_published(telemetry_queue_t *queue, int message_id)
{
    telemetry_item_t *item = telemetry_queue_head(queue);
    if (!item || item->in_flight || message_id < 0) return false;
    item->message_id = message_id;
    item->in_flight = true;
    return true;
}

bool telemetry_queue_ack(telemetry_queue_t *queue, int message_id)
{
    telemetry_item_t *item = telemetry_queue_head(queue);
    if (!item || !item->in_flight || item->message_id != message_id) return false;
    memset(item, 0, sizeof(*item));
    queue->head = (queue->head + 1) % TELEMETRY_QUEUE_CAPACITY;
    queue->count--;
    return true;
}

void telemetry_queue_retry_inflight(telemetry_queue_t *queue)
{
    telemetry_item_t *item = telemetry_queue_head(queue);
    if (item) {
        item->in_flight = false;
        item->message_id = 0;
    }
}

size_t telemetry_queue_count(const telemetry_queue_t *queue)
{
    return queue->count;
}

uint32_t telemetry_queue_dropped(const telemetry_queue_t *queue)
{
    return queue->dropped;
}
