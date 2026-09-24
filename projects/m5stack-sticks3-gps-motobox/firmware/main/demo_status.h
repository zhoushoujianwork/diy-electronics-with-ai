#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool fix_valid;
    bool gnss_online;
    bool wifi_connected;
    bool mqtt_connected;
    bool time_trusted;
    bool binding_ready;
    bool binding_known;
    bool binding_bound;
    char binding_code[7];
    int64_t binding_expires_ms;
    char wifi_ssid[33];
    char wifi_ip[16];
    int wifi_rssi;
    int satellites;
    float hdop;
    float speed_kmh;
    uint32_t fix_age_ms;
    unsigned queue_depth;
    unsigned queue_dropped;
} demo_status_t;
