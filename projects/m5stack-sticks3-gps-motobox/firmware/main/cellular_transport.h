#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool sim_ready;
    bool registered;
    bool data_ready;
    int csq;
    uint32_t reconnects;
    int64_t sampled_ms;
    char model[40];
    char carrier[24];
    char error[40];
} cellular_status_t;

typedef void (*cellular_status_callback_t)(const cellular_status_t *status);
typedef void (*cellular_time_callback_t)(int64_t utc_ms);

esp_err_t cellular_start(cellular_status_callback_t status_cb, cellular_time_callback_t time_cb);
esp_transport_handle_t cellular_tls_transport_create(void);

#ifdef __cplusplus
}
#endif
