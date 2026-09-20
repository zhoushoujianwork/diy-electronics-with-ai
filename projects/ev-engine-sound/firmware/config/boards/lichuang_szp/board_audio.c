#include "board_audio.h"

#include <stddef.h>
#include <stdint.h>

#include "board_config.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "board_audio";
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t codec;
static i2c_master_dev_handle_t expander;
static bool ready;
static bool amp_enabled;

enum {
    PCA9557_REG_OUTPUT = 0x01,
    PCA9557_REG_CONFIG = 0x03,
};

typedef struct {
    uint8_t reg;
    uint8_t value;
} reg_pair_t;

static esp_err_t device_write(i2c_master_dev_handle_t dev,uint8_t reg,uint8_t value) {
    uint8_t payload[2]={reg,value};
    return i2c_master_transmit(dev,payload,sizeof(payload),100);
}

static esp_err_t device_read(i2c_master_dev_handle_t dev,uint8_t reg,uint8_t *value) {
    return i2c_master_transmit_receive(dev,&reg,1,value,1,100);
}

static esp_err_t init_expander(void) {
    const i2c_device_config_t config={
        .dev_addr_length=I2C_ADDR_BIT_LEN_7,
        .device_address=EV_PCA9557_I2C_ADDR,
        .scl_speed_hz=100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus,&config,&expander),TAG,"add PCA9557");

    /* Write levels before directions to avoid an amplifier-enable glitch.
     * LCD_CS=1 (inactive), PA_EN=0 (muted); all other pins remain inputs. */
    ESP_RETURN_ON_ERROR(device_write(expander,PCA9557_REG_OUTPUT,0x01),TAG,"PCA9557 output init");
    ESP_RETURN_ON_ERROR(device_write(expander,PCA9557_REG_CONFIG,0xfc),TAG,"PCA9557 direction init");
    uint8_t output=0xff,direction=0xff;
    ESP_RETURN_ON_ERROR(device_read(expander,PCA9557_REG_OUTPUT,&output),TAG,"PCA9557 output readback");
    ESP_RETURN_ON_ERROR(device_read(expander,PCA9557_REG_CONFIG,&direction),TAG,"PCA9557 direction readback");
    ESP_RETURN_ON_FALSE(output==0x01 && direction==0xfc,ESP_ERR_INVALID_RESPONSE,TAG,
                        "PCA9557 readback output=0x%02x direction=0x%02x",output,direction);
    ESP_LOGI(TAG,"PCA9557 addr=0x19 output=0x%02x direction=0x%02x pa=OFF",output,direction);
    return ESP_OK;
}

static esp_err_t init_codec(void) {
    static const reg_pair_t sequence[]={
        {0x00,0x1f},{0x00,0x00},{0x00,0x80},
        {0x44,0x08},{0x44,0x08},
        {0x01,0x30},{0x02,0x00},{0x03,0x10},{0x04,0x10},
        {0x05,0x00},{0x06,0x07},{0x07,0x00},{0x08,0xff},
        {0x09,0x0c},{0x0a,0x0c},{0x0b,0x00},{0x0c,0x00},
        {0x10,0x1f},{0x11,0x7f},{0x13,0x10},{0x14,0x1a},
        {0x15,0x40},{0x16,0x24},{0x17,0xbf},
        {0x1b,0x0a},{0x1c,0x6a},
        {0x31,0x00},{0x32,0xbf},{0x37,0x08},
        {0x44,0x58},{0x0e,0x02},{0x12,0x00},{0x0d,0x01},{0x45,0x00},
    };
    ESP_RETURN_ON_ERROR(i2c_master_probe(bus,EV_ES8311_I2C_ADDR,100),TAG,
                        "ES8311 probe addr=0x18");
    const i2c_device_config_t config={
        .dev_addr_length=I2C_ADDR_BIT_LEN_7,
        .device_address=EV_ES8311_I2C_ADDR,
        .scl_speed_hz=400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus,&config,&codec),TAG,"add ES8311");
    for(size_t i=0;i<sizeof(sequence)/sizeof(sequence[0]);++i) {
        ESP_RETURN_ON_ERROR(device_write(codec,sequence[i].reg,sequence[i].value),TAG,
                            "ES8311 write reg=0x%02x",sequence[i].reg);
    }
    uint8_t format=0,volume=0;
    ESP_RETURN_ON_ERROR(device_read(codec,0x09,&format),TAG,"ES8311 format readback");
    ESP_RETURN_ON_ERROR(device_read(codec,0x32,&volume),TAG,"ES8311 volume readback");
    ESP_RETURN_ON_FALSE(format==0x0c && volume==0xbf,ESP_ERR_INVALID_RESPONSE,TAG,
                        "ES8311 readback format=0x%02x volume=0x%02x",format,volume);
    ESP_LOGI(TAG,"ES8311 addr=0x18 format=0x%02x volume=0x%02x",format,volume);
    return ESP_OK;
}

esp_err_t board_audio_prepare(void) {
    const i2c_master_bus_config_t config={
        .i2c_port=EV_AUDIO_I2C_PORT,
        .sda_io_num=EV_AUDIO_I2C_SDA_GPIO,
        .scl_io_num=EV_AUDIO_I2C_SCL_GPIO,
        .clk_source=I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt=7,
        .flags.enable_internal_pullup=true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&config,&bus),TAG,"create I2C bus");
    ESP_RETURN_ON_ERROR(init_expander(),TAG,"init PCA9557");
    ESP_RETURN_ON_ERROR(init_codec(),TAG,"init ES8311");
    ready=true;
    return ESP_OK;
}

esp_err_t board_audio_start(void) {
    ESP_RETURN_ON_FALSE(ready,ESP_ERR_INVALID_STATE,TAG,"board audio not prepared");
    /* The clock-gate write is repeated after I2S starts and MCLK is present. */
    ESP_RETURN_ON_ERROR(device_write(codec,0x01,0x3f),TAG,"ES8311 clock gates");
    uint8_t gates=0;
    ESP_RETURN_ON_ERROR(device_read(codec,0x01,&gates),TAG,"ES8311 clock readback");
    ESP_RETURN_ON_FALSE(gates==0x3f,ESP_ERR_INVALID_RESPONSE,TAG,
                        "ES8311 clock readback=0x%02x expected=0x3f",gates);
    ESP_LOGI(TAG,"ES8311 clocks=0x%02x MCLK=%dHz",gates,
             EV_AUDIO_SAMPLE_RATE*256);
    return ESP_OK;
}

esp_err_t board_audio_set_amp(bool enabled) {
    ESP_RETURN_ON_FALSE(ready,ESP_ERR_INVALID_STATE,TAG,"board audio not prepared");
    if(enabled==amp_enabled) return ESP_OK;
    uint8_t output=0;
    ESP_RETURN_ON_ERROR(device_read(expander,PCA9557_REG_OUTPUT,&output),TAG,"PCA9557 read PA");
    output=(uint8_t)((output&~(1U<<EV_PCA9557_PA_EN_BIT)) |
                     ((enabled?1U:0U)<<EV_PCA9557_PA_EN_BIT));
    ESP_RETURN_ON_ERROR(device_write(expander,PCA9557_REG_OUTPUT,output),TAG,"PCA9557 write PA");
    uint8_t readback=0;
    ESP_RETURN_ON_ERROR(device_read(expander,PCA9557_REG_OUTPUT,&readback),TAG,"PCA9557 PA readback");
    ESP_RETURN_ON_FALSE(((readback>>EV_PCA9557_PA_EN_BIT)&1U)==(enabled?1U:0U),
                        ESP_ERR_INVALID_RESPONSE,TAG,"PCA9557 PA readback=0x%02x",readback);
    amp_enabled=enabled;
    ESP_LOGI(TAG,"STATE_TRANSITION: AMP -> %s output=0x%02x",enabled?"ON":"OFF",readback);
    return ESP_OK;
}

esp_err_t board_audio_set_lcd_selected(bool selected) {
    ESP_RETURN_ON_FALSE(ready,ESP_ERR_INVALID_STATE,TAG,"board audio not prepared");
    uint8_t output=0;
    ESP_RETURN_ON_ERROR(device_read(expander,PCA9557_REG_OUTPUT,&output),TAG,"PCA9557 read LCD CS");
    output=(uint8_t)((output&~(1U<<EV_PCA9557_LCD_CS_BIT)) |
                     ((selected?0U:1U)<<EV_PCA9557_LCD_CS_BIT));
    ESP_RETURN_ON_ERROR(device_write(expander,PCA9557_REG_OUTPUT,output),TAG,"PCA9557 write LCD CS");
    uint8_t readback=0;
    ESP_RETURN_ON_ERROR(device_read(expander,PCA9557_REG_OUTPUT,&readback),TAG,"PCA9557 LCD CS readback");
    ESP_RETURN_ON_FALSE(((readback>>EV_PCA9557_LCD_CS_BIT)&1U)==(selected?0U:1U),
                        ESP_ERR_INVALID_RESPONSE,TAG,"PCA9557 LCD CS readback=0x%02x",readback);
    ESP_LOGI(TAG,"STATE_TRANSITION: LCD_CS -> %s output=0x%02x",selected?"SELECTED":"IDLE",readback);
    return ESP_OK;
}

esp_err_t board_audio_report(void) {
    ESP_RETURN_ON_FALSE(ready,ESP_ERR_INVALID_STATE,TAG,"board audio not prepared");
    uint8_t output=0,direction=0,format=0,volume=0,gates=0;
    ESP_RETURN_ON_ERROR(device_read(expander,PCA9557_REG_OUTPUT,&output),TAG,"report PCA output");
    ESP_RETURN_ON_ERROR(device_read(expander,PCA9557_REG_CONFIG,&direction),TAG,"report PCA config");
    ESP_RETURN_ON_ERROR(device_read(codec,0x09,&format),TAG,"report ES8311 format");
    ESP_RETURN_ON_ERROR(device_read(codec,0x32,&volume),TAG,"report ES8311 volume");
    ESP_RETURN_ON_ERROR(device_read(codec,0x01,&gates),TAG,"report ES8311 clocks");
    ESP_LOGI(TAG,"AUDIO_READBACK pca_out=0x%02x pca_dir=0x%02x es8311_format=0x%02x volume=0x%02x clocks=0x%02x",
             output,direction,format,volume,gates);
    return ESP_OK;
}

i2c_master_bus_handle_t board_audio_i2c_bus(void) {
    return bus;
}
