#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool fix_valid;
    bool wifi_connected;
    bool mqtt_connected;
    bool time_trusted;
    bool binding_ready;
    char binding_code[7];
    int64_t binding_expires_ms;
    int satellites;
    float hdop;
    float speed_kmh;
    uint32_t fix_age_ms;
    unsigned queue_depth;
    unsigned queue_dropped;
} demo_status_t;
