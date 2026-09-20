#pragma once

/*
 * LCKFB / JLC "Shi Zhan Pai" ESP32-S3 N16R8 board profile.
 * Pin map verified against the connected 320x240 development board.
 */

#define EV_BOARD_NAME "lichuang_szp"
#define EV_AUDIO_SAMPLE_RATE 16000
#define EV_AUDIO_I2S_SLOT_BITS 16

/* ES8311 playback I2S. */
#define EV_AUDIO_I2S_MCLK_GPIO 38
#define EV_AUDIO_I2S_BCLK_GPIO 14
#define EV_AUDIO_I2S_WS_GPIO   13
#define EV_AUDIO_I2S_DOUT_GPIO 45
#define EV_AUDIO_I2S_DIN_GPIO  12

/* ES8311 and PCA9557 share I2C0. */
#define EV_AUDIO_I2C_PORT     0
#define EV_AUDIO_I2C_SDA_GPIO 1
#define EV_AUDIO_I2C_SCL_GPIO 2
#define EV_ES8311_I2C_ADDR    0x18
#define EV_PCA9557_I2C_ADDR   0x19
#define EV_PCA9557_LCD_CS_BIT 0
#define EV_PCA9557_PA_EN_BIT  1

/* Power amplifier is controlled through PCA9557 bit 1, not a direct GPIO. */
#define EV_AMP_ENABLE_GPIO  (-1)
#define EV_AMP_ACTIVE_LEVEL 1

/* Remaining board I/O, retained for future peripheral integration. */
#define EV_PTT_GPIO         0
#define EV_PTT_ACTIVE_LEVEL 0
#define EV_PTT_PULL_UP      1
#define EV_DISPLAY_SCLK_GPIO 41
#define EV_DISPLAY_MOSI_GPIO 40
#define EV_DISPLAY_CS_GPIO   (-1)
#define EV_DISPLAY_DC_GPIO   39
#define EV_DISPLAY_RST_GPIO  (-1)
#define EV_DISPLAY_BL_GPIO   42
#define EV_STATUS_LED_GPIO   48

/* 2.0-inch ST7789 320x240 display. SPI mode 2 and PWM backlight are
 * requirements verified on the LCKFB board. */
#define EV_UI_ENABLE               1
#define EV_DISPLAY_WIDTH           320
#define EV_DISPLAY_HEIGHT          240
#define EV_DISPLAY_SPI_HOST        2
#define EV_DISPLAY_PCLK_HZ         (40 * 1000 * 1000)
#define EV_DISPLAY_SPI_MODE        2
#define EV_DISPLAY_SWAP_XY         1
#define EV_DISPLAY_MIRROR_X        1
#define EV_DISPLAY_MIRROR_Y        0
#define EV_DISPLAY_INVERT_COLOR    1
#define EV_DISPLAY_BUFFER_LINES    40
#define EV_DISPLAY_BL_PWM_HZ       5000
#define EV_DISPLAY_BL_PWM_DUTY     512

/* FT6336 is FT5x06 protocol-compatible and shares I2C0 with the codecs. */
#define EV_TOUCH_I2C_ADDR          0x38
#define EV_TOUCH_RST_GPIO          (-1)
#define EV_TOUCH_INT_GPIO          (-1)
#define EV_TOUCH_SWAP_XY           1
#define EV_TOUCH_MIRROR_X          1
#define EV_TOUCH_MIRROR_Y          0
