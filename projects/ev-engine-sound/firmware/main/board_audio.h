#pragma once

#include <stdbool.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

/* Board-specific codec and amplifier hooks. */
esp_err_t board_audio_prepare(void);
esp_err_t board_audio_start(void);
esp_err_t board_audio_set_amp(bool enabled);
esp_err_t board_audio_set_lcd_selected(bool selected);
esp_err_t board_audio_report(void);
i2c_master_bus_handle_t board_audio_i2c_bus(void);
