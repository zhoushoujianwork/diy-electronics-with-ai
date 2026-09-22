#pragma once

#include "esp_err.h"

esp_err_t binding_announcement_init(void);
esp_err_t binding_announcement_enqueue(const char *code);
unsigned binding_announcement_stack_high_water_mark(void);
