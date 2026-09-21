#pragma once

#include "esp_err.h"

#define STICKS3_GPS_UART_RX_GPIO 10
#define STICKS3_GPS_UART_TX_GPIO 9
#define STICKS3_BUTTON_A_GPIO 11

esp_err_t board_power_init(void);
