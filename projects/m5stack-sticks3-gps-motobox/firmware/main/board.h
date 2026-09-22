#pragma once

#include <stdbool.h>

#include "esp_err.h"

#define STICKS3_GPS_UART_RX_GPIO 10
#define STICKS3_GPS_UART_TX_GPIO 9
#define STICKS3_BUTTON_A_GPIO 11
#define STICKS3_AUDIO_MCLK_GPIO 18
#define STICKS3_AUDIO_BCLK_GPIO 17
#define STICKS3_AUDIO_WS_GPIO 15
#define STICKS3_AUDIO_DOUT_GPIO 14

esp_err_t board_power_init(void);
esp_err_t board_audio_prepare(void);
esp_err_t board_audio_start(void);
esp_err_t board_audio_set_amp(bool enabled);
esp_err_t board_audio_report(void);
