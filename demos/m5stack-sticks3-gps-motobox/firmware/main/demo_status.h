#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool fix_valid;
    bool wifi_connected;
    bool mqtt_connected;
    bool time_trusted;
    int satellites;
    float hdop;
    float speed_kmh;
    uint32_t fix_age_ms;
    unsigned queue_depth;
    unsigned queue_dropped;
} demo_status_t;
