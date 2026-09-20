#pragma once

/*
 * Legacy ESP32-S3 development board pin map.
 *
 * Source: historical board-support commit 7429f08 (2026-04-11). GPIO4 is the
 * yellow status LED on this revision; it must not be driven as a MAX98357A
 * enable pin.
 */

#define EV_BOARD_NAME "legacy_esp32s3"
#define EV_AUDIO_SAMPLE_RATE 32000
#define EV_AUDIO_I2S_SLOT_BITS 32

/* MAX98357A speaker output (ESP32-S3 I2S0 master). */
#define EV_AUDIO_I2S_BCLK_GPIO 16
#define EV_AUDIO_I2S_WS_GPIO   15
#define EV_AUDIO_I2S_DOUT_GPIO 17
#define EV_AUDIO_I2S_DIN_GPIO  18
#define EV_AUDIO_I2S_MCLK_GPIO (-1)
#define EV_AMP_ENABLE_GPIO     (-1)
#define EV_AMP_ACTIVE_LEVEL    1

/* Push-to-talk input. */
#define EV_PTT_GPIO         7
#define EV_PTT_ACTIVE_LEVEL 1
#define EV_PTT_PULL_UP      0

/* Navigation encoder. */
#define EV_NAV_ENCODER_A_GPIO 6
#define EV_NAV_ENCODER_B_GPIO 8
#define EV_NAV_KEY_GPIO       1

/* Haptic motor and battery measurement. */
#define EV_MOTOR_GPIO       21
#define EV_BATTERY_ADC_GPIO 3

/* Discrete red/yellow/green status LEDs. */
#define EV_STATUS_LED_R_GPIO 2
#define EV_STATUS_LED_Y_GPIO 4
#define EV_STATUS_LED_G_GPIO 5

/* ST7789 SPI display. */
#define EV_UI_ENABLE          0
#define EV_DISPLAY_SCLK_GPIO 12
#define EV_DISPLAY_MOSI_GPIO 11
#define EV_DISPLAY_CS_GPIO   10
#define EV_DISPLAY_DC_GPIO   9
#define EV_DISPLAY_RST_GPIO  14
#define EV_DISPLAY_BL_GPIO   13
