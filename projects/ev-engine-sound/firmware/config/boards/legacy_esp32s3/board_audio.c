#include "board_audio.h"

#include "esp_log.h"

static const char *TAG = "board_audio";

esp_err_t board_audio_prepare(void) {
    ESP_LOGI(TAG,"codec=MAX98357A control=none (module hardware enable)");
    return ESP_OK;
}

esp_err_t board_audio_start(void) {
    return ESP_OK;
}

esp_err_t board_audio_set_amp(bool enabled) {
    (void)enabled;
    return ESP_OK;
}

esp_err_t board_audio_set_lcd_selected(bool selected) {
    (void)selected;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t board_audio_report(void) {
    ESP_LOGI(TAG,"AUDIO_READBACK codec=MAX98357A control=none");
    return ESP_OK;
}

i2c_master_bus_handle_t board_audio_i2c_bus(void) {
    return NULL;
}
