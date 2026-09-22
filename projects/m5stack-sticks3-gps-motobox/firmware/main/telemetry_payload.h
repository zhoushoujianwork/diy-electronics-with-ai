#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gnss_parser.h"

typedef struct {
    const char *device_id;
    const char *firmware;
    int64_t now_ms;
    uint64_t sequence;
    uint32_t uptime_s;
    uint32_t free_heap;
    bool wifi_connected;
    bool has_fix;
    uint32_t fix_age_ms;
    gnss_fix_t fix;
} telemetry_payload_input_t;

size_t telemetry_payload_build(char *buffer, size_t capacity,
                               const telemetry_payload_input_t *input);
