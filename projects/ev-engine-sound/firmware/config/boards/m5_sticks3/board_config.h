#pragma once

/* Official M5Stack StickS3 K150, not StickC/StickC Plus/Plus2.
 * Source and pending hardware checks: docs/sticks3.md. */
#define EV_BOARD_NAME "m5_sticks3"
#define EV_AUDIO_SAMPLE_RATE 16000
#define EV_AUDIO_I2S_SLOT_BITS 16
#define EV_AUDIO_I2S_MCLK_GPIO 18
#define EV_AUDIO_I2S_BCLK_GPIO 17
#define EV_AUDIO_I2S_WS_GPIO 15
#define EV_AUDIO_I2S_DOUT_GPIO 14
#define EV_AUDIO_I2S_DIN_GPIO 16
#define EV_AUDIO_I2C_PORT 0
#define EV_AUDIO_I2C_SDA_GPIO 47
#define EV_AUDIO_I2C_SCL_GPIO 48
#define EV_ES8311_I2C_ADDR 0x18
#define EV_PM1_I2C_ADDR 0x6e
#define EV_AMP_ENABLE_GPIO (-1)
#define EV_AMP_ACTIVE_LEVEL 1
#define EV_BUTTON_A_GPIO 11
#define EV_BUTTON_B_GPIO 12
#define EV_UI_ENABLE 1
#define EV_DISPLAY_WIDTH 240
#define EV_DISPLAY_HEIGHT 135
#define EV_DISPLAY_SPI_HOST 2 /* SPI3_HOST */
#define EV_DISPLAY_SCLK_GPIO 40
#define EV_DISPLAY_MOSI_GPIO 39
#define EV_DISPLAY_CS_GPIO 41
#define EV_DISPLAY_DC_GPIO 45
#define EV_DISPLAY_RST_GPIO 21
#define EV_DISPLAY_BL_GPIO 38
#define EV_DISPLAY_PCLK_HZ (40 * 1000 * 1000)
#define EV_DISPLAY_SPI_MODE 0
#define EV_DISPLAY_SWAP_XY 1
#define EV_DISPLAY_MIRROR_X 1
#define EV_DISPLAY_MIRROR_Y 0
/* Native 135x240 viewport starts at (52,40) in 240x320 RAM.
 * Landscape MV|MX: swapped axes, native X mirrored => 240-52-135=53. */
#define EV_DISPLAY_GAP_X 40
#define EV_DISPLAY_GAP_Y 53
#define EV_DISPLAY_BUFFER_LINES 20
