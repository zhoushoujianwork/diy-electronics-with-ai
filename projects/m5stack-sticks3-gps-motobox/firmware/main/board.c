#include "board.h"

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define M5PM1_ADDRESS 0x6e
#define ES8311_ADDRESS 0x18
#define M5PM1_REG_POWER_CONFIG 0x06
#define M5PM1_REG_I2C_CONFIG 0x09
#define M5PM1_REG_GPIO_MODE 0x10
#define M5PM1_REG_GPIO_OUTPUT 0x11
#define M5PM1_REG_GPIO_DRIVE 0x13
#define M5PM1_REG_GPIO_FUNCTION0 0x16
#define M5PM1_BOOST_ENABLE (1U << 3)
#define M5PM1_GPIO2 (1U << 2)
#define M5PM1_GPIO3 (1U << 3)

static const char *TAG = "board";
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t pm1;
static i2c_master_dev_handle_t codec;
static bool audio_ready;
static bool amp_enabled;

static esp_err_t device_read(i2c_master_dev_handle_t device, uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(device, &reg, 1, value, 1, 100);
}

static esp_err_t device_write(i2c_master_dev_handle_t device, uint8_t reg, uint8_t value)
{
    uint8_t bytes[] = {reg, value};
    return i2c_master_transmit(device, bytes, sizeof(bytes), 100);
}

static esp_err_t add_device(uint8_t address, i2c_master_dev_handle_t *handle)
{
    ESP_RETURN_ON_ERROR(i2c_master_probe(bus, address, 100), TAG, "probe addr=0x%02x", address);
    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 100000,
    };
    return i2c_master_bus_add_device(bus, &config, handle);
}

static esp_err_t update_bits(uint8_t reg, uint8_t mask, bool set)
{
    uint8_t value = 0;
    ESP_RETURN_ON_ERROR(device_read(pm1, reg, &value), TAG, "read PM1 reg 0x%02x", reg);
    value = set ? (uint8_t)(value | mask) : (uint8_t)(value & ~mask);
    ESP_RETURN_ON_ERROR(device_write(pm1, reg, value), TAG, "write PM1 reg 0x%02x", reg);
    uint8_t check = 0;
    ESP_RETURN_ON_ERROR(device_read(pm1, reg, &check), TAG, "read back PM1 reg 0x%02x", reg);
    return ((check & mask) == (value & mask)) ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

esp_err_t board_power_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = 47,
        .scl_io_num = 48,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), TAG, "create PM1 I2C bus");
    ESP_RETURN_ON_ERROR(add_device(M5PM1_ADDRESS, &pm1), TAG, "add M5PM1");

    ESP_RETURN_ON_ERROR(device_write(pm1, M5PM1_REG_I2C_CONFIG, 0), TAG, "disable PM1 I2C sleep");
    /* StickS3 LCD rail is PM1 GPIO2. Set the level before changing direction. */
    ESP_RETURN_ON_ERROR(update_bits(M5PM1_REG_GPIO_OUTPUT, M5PM1_GPIO2, true), TAG, "LCD rail high");
    ESP_RETURN_ON_ERROR(update_bits(M5PM1_REG_GPIO_FUNCTION0, M5PM1_GPIO2, false), TAG, "GPIO2 function");
    ESP_RETURN_ON_ERROR(update_bits(M5PM1_REG_GPIO_MODE, M5PM1_GPIO2, true), TAG, "GPIO2 output");
    /* Official M5PM1 setExtOutput(true): POWER_CONFIG.BOOST_EN. */
    ESP_RETURN_ON_ERROR(update_bits(M5PM1_REG_POWER_CONFIG, M5PM1_BOOST_ENABLE, true), TAG, "Grove 5V boost");

    ESP_LOGI(TAG, "POWER_READY lcd=on grove_5v=on pm1=0x%02x", M5PM1_ADDRESS);
    return ESP_OK;
}

esp_err_t board_audio_prepare(void)
{
    ESP_RETURN_ON_FALSE(bus && pm1, ESP_ERR_INVALID_STATE, TAG, "power not initialized");
    /* Set the amplifier level before making PM1 GPIO3 an output. */
    ESP_RETURN_ON_ERROR(update_bits(M5PM1_REG_GPIO_OUTPUT, M5PM1_GPIO3, false), TAG, "amp mute");
    ESP_RETURN_ON_ERROR(update_bits(M5PM1_REG_GPIO_FUNCTION0, M5PM1_GPIO3, false), TAG, "GPIO3 function");
    ESP_RETURN_ON_ERROR(update_bits(M5PM1_REG_GPIO_DRIVE, M5PM1_GPIO3, false), TAG, "GPIO3 push-pull");
    ESP_RETURN_ON_ERROR(update_bits(M5PM1_REG_GPIO_MODE, M5PM1_GPIO3, true), TAG, "GPIO3 output");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(add_device(ES8311_ADDRESS, &codec), TAG, "add ES8311");

    /* Validated StickS3 setup: 16 kHz, MCLK=256 Fs, 16-bit stereo I2S slots. */
    static const uint8_t sequence[][2] = {
        {0x00, 0x1f}, {0x00, 0x00}, {0x00, 0x80}, {0x44, 0x08}, {0x44, 0x08},
        {0x01, 0x30}, {0x02, 0x00}, {0x03, 0x10}, {0x04, 0x10}, {0x05, 0x00},
        {0x06, 0x07}, {0x07, 0x00}, {0x08, 0xff}, {0x09, 0x0c}, {0x0a, 0x0c},
        {0x0b, 0x00}, {0x0c, 0x00}, {0x10, 0x1f}, {0x11, 0x7f}, {0x13, 0x10},
        {0x14, 0x1a}, {0x15, 0x40}, {0x16, 0x24}, {0x17, 0xbf}, {0x1b, 0x0a},
        {0x1c, 0x6a}, {0x31, 0x00}, {0x32, 0xbf}, {0x37, 0x08}, {0x44, 0x58},
        {0x0e, 0x02}, {0x12, 0x00}, {0x0d, 0x01}, {0x45, 0x00},
    };
    for (size_t i = 0; i < sizeof(sequence) / sizeof(sequence[0]); ++i) {
        ESP_RETURN_ON_ERROR(device_write(codec, sequence[i][0], sequence[i][1]), TAG,
                            "ES8311 write reg=0x%02x", sequence[i][0]);
    }
    uint8_t format = 0;
    uint8_t volume = 0;
    ESP_RETURN_ON_ERROR(device_read(codec, 0x09, &format), TAG, "ES8311 format readback");
    ESP_RETURN_ON_ERROR(device_read(codec, 0x32, &volume), TAG, "ES8311 volume readback");
    ESP_RETURN_ON_FALSE(format == 0x0c && volume == 0xbf, ESP_ERR_INVALID_RESPONSE, TAG,
                        "ES8311 readback format=0x%02x volume=0x%02x", format, volume);
    audio_ready = true;
    ESP_LOGI(TAG, "AUDIO_READY amp=off format=0x%02x volume=0x%02x", format, volume);
    return ESP_OK;
}

esp_err_t board_audio_start(void)
{
    ESP_RETURN_ON_FALSE(audio_ready, ESP_ERR_INVALID_STATE, TAG, "audio not prepared");
    ESP_RETURN_ON_ERROR(device_write(codec, 0x01, 0x3f), TAG, "ES8311 clocks");
    uint8_t gates = 0;
    ESP_RETURN_ON_ERROR(device_read(codec, 0x01, &gates), TAG, "ES8311 clock readback");
    ESP_RETURN_ON_FALSE(gates == 0x3f, ESP_ERR_INVALID_RESPONSE, TAG, "ES8311 clocks=0x%02x", gates);
    ESP_LOGI(TAG, "AUDIO_CLOCKS clocks=0x%02x mclk_hz=%d", gates, 16000 * 256);
    return ESP_OK;
}

esp_err_t board_audio_set_amp(bool enabled)
{
    ESP_RETURN_ON_FALSE(audio_ready, ESP_ERR_INVALID_STATE, TAG, "audio not prepared");
    if (enabled == amp_enabled) return ESP_OK;
    ESP_RETURN_ON_ERROR(update_bits(M5PM1_REG_GPIO_OUTPUT, M5PM1_GPIO3, enabled), TAG,
                        "amp enable=%d", enabled);
    amp_enabled = enabled;
    ESP_LOGI(TAG, "AUDIO_AMP %s", enabled ? "on" : "off");
    return ESP_OK;
}

esp_err_t board_audio_report(void)
{
    ESP_RETURN_ON_FALSE(audio_ready, ESP_ERR_INVALID_STATE, TAG, "audio not prepared");
    uint8_t output = 0, mode = 0, format = 0, volume = 0, gates = 0;
    ESP_RETURN_ON_ERROR(device_read(pm1, M5PM1_REG_GPIO_OUTPUT, &output), TAG, "PM1 output readback");
    ESP_RETURN_ON_ERROR(device_read(pm1, M5PM1_REG_GPIO_MODE, &mode), TAG, "PM1 mode readback");
    ESP_RETURN_ON_ERROR(device_read(codec, 0x09, &format), TAG, "ES8311 format report");
    ESP_RETURN_ON_ERROR(device_read(codec, 0x32, &volume), TAG, "ES8311 volume report");
    ESP_RETURN_ON_ERROR(device_read(codec, 0x01, &gates), TAG, "ES8311 clocks report");
    ESP_LOGI(TAG,
             "AUDIO_READBACK pm1_out=0x%02x pm1_mode=0x%02x format=0x%02x volume=0x%02x clocks=0x%02x",
             output, mode, format, volume, gates);
    return ESP_OK;
}
