#include "board_audio.h"
#include "board_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG="stick_audio";
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t pm1,codec;
static bool ready,amp_enabled;

static esp_err_t rd(i2c_master_dev_handle_t dev,uint8_t reg,uint8_t *v) {
    return i2c_master_transmit_receive(dev,&reg,1,v,1,100);
}
static esp_err_t wr(i2c_master_dev_handle_t dev,uint8_t reg,uint8_t v) {
    uint8_t bytes[]={reg,v};
    return i2c_master_transmit(dev,bytes,2,100);
}
/* Preserve charge status, IR, Grove and all unrelated PM1 functions. */
static esp_err_t mask(uint8_t reg,uint8_t bits,bool set) {
    uint8_t value,check;
    ESP_RETURN_ON_ERROR(rd(pm1,reg,&value),TAG,"PM1 read reg=%02x",reg);
    value=set?(value|bits):(value&~bits);
    ESP_RETURN_ON_ERROR(wr(pm1,reg,value),TAG,"PM1 write reg=%02x",reg);
    ESP_RETURN_ON_ERROR(rd(pm1,reg,&check),TAG,"PM1 readback reg=%02x",reg);
    ESP_RETURN_ON_FALSE((check&bits)==(value&bits),ESP_ERR_INVALID_RESPONSE,TAG,
                        "PM1 reg=%02x got=%02x expected=%02x",reg,check,value);
    return ESP_OK;
}
static esp_err_t add_device(uint8_t address,i2c_master_dev_handle_t *handle) {
    ESP_RETURN_ON_ERROR(i2c_master_probe(bus,address,100),TAG,"probe addr=%02x",address);
    i2c_device_config_t cfg={.dev_addr_length=I2C_ADDR_BIT_LEN_7,
        .device_address=address,.scl_speed_hz=100000};
    return i2c_master_bus_add_device(bus,&cfg,handle);
}

esp_err_t board_audio_prepare(void) {
    i2c_master_bus_config_t cfg={.i2c_port=EV_AUDIO_I2C_PORT,
        .sda_io_num=EV_AUDIO_I2C_SDA_GPIO,.scl_io_num=EV_AUDIO_I2C_SCL_GPIO,
        .clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,
        .flags.enable_internal_pullup=true};
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&cfg,&bus),TAG,"I2C create");
    ESP_RETURN_ON_ERROR(add_device(EV_PM1_I2C_ADDR,&pm1),TAG,"PM1 add");
    /* M5GFX disables PM1 I2C idle sleep on every boot (PM1 survives power off). */
    ESP_RETURN_ON_ERROR(wr(pm1,0x09,0),TAG,"PM1 disable I2C sleep");
    /* Levels before directions: amp muted, LCD rail on. GPIO2/3 push-pull. */
    ESP_RETURN_ON_ERROR(mask(0x11,0x08,false),TAG,"amp mute");
    ESP_RETURN_ON_ERROR(mask(0x11,0x04,true),TAG,"LCD rail level");
    ESP_RETURN_ON_ERROR(mask(0x16,0x0c,false),TAG,"PM1 GPIO function");
    ESP_RETURN_ON_ERROR(mask(0x13,0x0c,false),TAG,"PM1 push-pull");
    ESP_RETURN_ON_ERROR(mask(0x10,0x0c,true),TAG,"PM1 output direction");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(add_device(EV_ES8311_I2C_ADDR,&codec),TAG,"codec add");
    /* Same 16kHz/256Fs MCLK/16-bit stereo-slot playback as the LCKFB port.
     * Deliberately not M5Unified's BCLK-derived 22050Hz default sequence. */
    static const uint8_t sequence[][2]={
        {0x00,0x1f},{0x00,0x00},{0x00,0x80},{0x44,0x08},{0x44,0x08},
        {0x01,0x30},{0x02,0x00},{0x03,0x10},{0x04,0x10},{0x05,0x00},
        {0x06,0x07},{0x07,0x00},{0x08,0xff},{0x09,0x0c},{0x0a,0x0c},
        {0x0b,0x00},{0x0c,0x00},{0x10,0x1f},{0x11,0x7f},{0x13,0x10},
        {0x14,0x1a},{0x15,0x40},{0x16,0x24},{0x17,0xbf},{0x1b,0x0a},
        {0x1c,0x6a},{0x31,0x00},{0x32,0xbf},{0x37,0x08},{0x44,0x58},
        {0x0e,0x02},{0x12,0x00},{0x0d,0x01},{0x45,0x00}
    };
    for(unsigned i=0;i<sizeof(sequence)/sizeof(sequence[0]);i++)
        ESP_RETURN_ON_ERROR(wr(codec,sequence[i][0],sequence[i][1]),TAG,
                            "codec reg=%02x",sequence[i][0]);
    uint8_t format,volume;
    ESP_RETURN_ON_ERROR(rd(codec,0x09,&format),TAG,"codec format readback");
    ESP_RETURN_ON_ERROR(rd(codec,0x32,&volume),TAG,"codec volume readback");
    ESP_RETURN_ON_FALSE(format==0x0c && volume==0xbf,ESP_ERR_INVALID_RESPONSE,TAG,
                        "codec readback format=%02x volume=%02x",format,volume);
    ready=true;
    ESP_LOGI(TAG,"PM1_READY lcd=ON amp=OFF format=%02x volume=%02x",format,volume);
    return ESP_OK;
}
esp_err_t board_audio_start(void) {
    ESP_RETURN_ON_FALSE(ready,ESP_ERR_INVALID_STATE,TAG,"not prepared");
    ESP_RETURN_ON_ERROR(wr(codec,0x01,0x3f),TAG,"codec clocks");
    uint8_t gates;
    ESP_RETURN_ON_ERROR(rd(codec,0x01,&gates),TAG,"codec clock readback");
    ESP_RETURN_ON_FALSE(gates==0x3f,ESP_ERR_INVALID_RESPONSE,TAG,"clocks=%02x",gates);
    ESP_LOGI(TAG,"ES8311 clocks=%02x MCLK=%dHz",gates,EV_AUDIO_SAMPLE_RATE*256);
    return ESP_OK;
}
esp_err_t board_audio_set_amp(bool enabled) {
    ESP_RETURN_ON_FALSE(ready,ESP_ERR_INVALID_STATE,TAG,"not prepared");
    if(enabled==amp_enabled) return ESP_OK;
    ESP_RETURN_ON_ERROR(mask(0x11,0x08,enabled),TAG,"amp enable=%d",enabled);
    amp_enabled=enabled;
    ESP_LOGI(TAG,"STATE_TRANSITION: AMP -> %s",enabled?"ON":"OFF");
    return ESP_OK;
}
esp_err_t board_audio_set_lcd_selected(bool selected) {
    (void)selected; /* Dedicated SPI CS41, not a PMIC chip select. */
    return ready?ESP_OK:ESP_ERR_INVALID_STATE;
}
esp_err_t board_audio_report(void) {
    ESP_RETURN_ON_FALSE(ready,ESP_ERR_INVALID_STATE,TAG,"not prepared");
    uint8_t out,dir,format,volume,gates;
    ESP_RETURN_ON_ERROR(rd(pm1,0x11,&out),TAG,"report PM1 output");
    ESP_RETURN_ON_ERROR(rd(pm1,0x10,&dir),TAG,"report PM1 direction");
    ESP_RETURN_ON_ERROR(rd(codec,0x09,&format),TAG,"report codec format");
    ESP_RETURN_ON_ERROR(rd(codec,0x32,&volume),TAG,"report codec volume");
    ESP_RETURN_ON_ERROR(rd(codec,0x01,&gates),TAG,"report codec clocks");
    ESP_LOGI(TAG,"AUDIO_READBACK pm1_out=0x%02x pm1_dir=0x%02x es8311_format=0x%02x volume=0x%02x clocks=0x%02x",
             out,dir,format,volume,gates);
    return ESP_OK;
}
i2c_master_bus_handle_t board_audio_i2c_bus(void) { return bus; }
