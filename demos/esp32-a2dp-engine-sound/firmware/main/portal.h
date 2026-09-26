#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef bool (*portal_connect_fn)(const uint8_t address[6]);

esp_err_t portal_start(portal_connect_fn connect_fn);
void portal_record_device(const uint8_t address[6], const char *name, int rssi);
void portal_set_state(bool scanning, bool connected, bool streaming);
bool portal_scan_ready(void);
