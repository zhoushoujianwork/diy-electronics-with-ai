#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TELEMETRY_QUEUE_CAPACITY 120
#define TELEMETRY_PAYLOAD_MAX 640

typedef struct {
    char payload[TELEMETRY_PAYLOAD_MAX];
    uint64_t sequence;
    int message_id;
    bool in_flight;
} telemetry_item_t;

typedef struct {
    telemetry_item_t items[TELEMETRY_QUEUE_CAPACITY];
    size_t head;
    size_t count;
    uint32_t dropped;
} telemetry_queue_t;

void telemetry_queue_init(telemetry_queue_t *queue);
void telemetry_queue_push(telemetry_queue_t *queue, uint64_t sequence, const char *payload);
telemetry_item_t *telemetry_queue_head(telemetry_queue_t *queue);
bool telemetry_queue_mark_published(telemetry_queue_t *queue, int message_id);
bool telemetry_queue_ack(telemetry_queue_t *queue, int message_id);
void telemetry_queue_retry_inflight(telemetry_queue_t *queue);
size_t telemetry_queue_count(const telemetry_queue_t *queue);
uint32_t telemetry_queue_dropped(const telemetry_queue_t *queue);
