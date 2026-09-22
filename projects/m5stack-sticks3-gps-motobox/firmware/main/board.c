#include "board.h"

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

#define M5PM1_ADDRESS 0x6e
#define M5PM1_REG_POWER_CONFIG 0x06
#define M5PM1_REG_I2C_CONFIG 0x09
#define M5PM1_REG_GPIO_MODE 0x10
#define M5PM1_REG_GPIO_OUTPUT 0x11
#define M5PM1_REG_GPIO_FUNCTION0 0x16
#define M5PM1_BOOST_ENABLE (1U << 3)
#define M5PM1_GPIO2 (1U << 2)

static const char *TAG = "board";
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t pm1;

static esp_err_t read_reg(uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(pm1, &reg, 1, value, 1, 100);
}

static esp_err_t write_reg(uint8_t reg, uint8_t value)
{
    uint8_t bytes[] = {reg, value};
    return i2c_master_transmit(pm1, bytes, sizeof(bytes), 100);
}

static esp_err_t update_bits(uint8_t reg, uint8_t mask, bool set)
{
    uint8_t value = 0;
    ESP_RETURN_ON_ERROR(read_reg(reg, &value), TAG, "read PM1 reg 0x%02x", reg);
    value = set ? (uint8_t)(value | mask) : (uint8_t)(value & ~mask);
    ESP_RETURN_ON_ERROR(write_reg(reg, value), TAG, "write PM1 reg 0x%02x", reg);
    uint8_t check = 0;
    ESP_RETURN_ON_ERROR(read_reg(reg, &check), TAG, "read back PM1 reg 0x%02x", reg);
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
    ESP_RETURN_ON_ERROR(i2c_master_probe(bus, M5PM1_ADDRESS, 100), TAG, "probe M5PM1");
    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = M5PM1_ADDRESS,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &device_config, &pm1), TAG, "add M5PM1");

    ESP_RETURN_ON_ERROR(write_reg(M5PM1_REG_I2C_CONFIG, 0), TAG, "disable PM1 I2C sleep");
    /* StickS3 LCD rail is PM1 GPIO2. Set the level before changing direction. */
    ESP_RETURN_ON_ERROR(update_bits(M5PM1_REG_GPIO_OUTPUT, M5PM1_GPIO2, true), TAG, "LCD rail high");
    ESP_RETURN_ON_ERROR(update_bits(M5PM1_REG_GPIO_FUNCTION0, M5PM1_GPIO2, false), TAG, "GPIO2 function");
    ESP_RETURN_ON_ERROR(update_bits(M5PM1_REG_GPIO_MODE, M5PM1_GPIO2, true), TAG, "GPIO2 output");
    /* Official M5PM1 setExtOutput(true): POWER_CONFIG.BOOST_EN. */
    ESP_RETURN_ON_ERROR(update_bits(M5PM1_REG_POWER_CONFIG, M5PM1_BOOST_ENABLE, true), TAG, "Grove 5V boost");

    ESP_LOGI(TAG, "POWER_READY lcd=on grove_5v=on pm1=0x%02x", M5PM1_ADDRESS);
    return ESP_OK;
}
