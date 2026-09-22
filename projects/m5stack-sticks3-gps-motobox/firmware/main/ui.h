#pragma once

#include "demo_status.h"
#include "esp_err.h"

esp_err_t ui_init(const char *device_id);
void ui_set_status(const demo_status_t *status);
unsigned ui_stack_high_water_mark(void);
