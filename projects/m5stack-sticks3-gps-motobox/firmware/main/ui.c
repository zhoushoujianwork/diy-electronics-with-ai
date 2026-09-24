#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#define LCD_WIDTH 240
#define LCD_HEIGHT 135

static const char *TAG = "ui";
static portMUX_TYPE status_lock = portMUX_INITIALIZER_UNLOCKED;
static demo_status_t latest_status;
static char shown_device_id[24];
static lv_obj_t *status_page;
static lv_obj_t *bind_page;
static lv_obj_t *gps_label;
static lv_obj_t *speed_label;
static lv_obj_t *network_label;
static lv_obj_t *ip_label;
static lv_obj_t *binding_label;
static lv_obj_t *binding_code_label;
static lv_obj_t *binding_expiry_label;
static bool bind_page_visible;
static bool binding_was_ready;
static bool button_previous;

static lv_obj_t *make_label(lv_obj_t *parent, int x, int y, int width,
                            const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
}

static void refresh_timer(lv_timer_t *timer)
{
    (void)timer;
    demo_status_t status;
    portENTER_CRITICAL(&status_lock);
    status = latest_status;
    portEXIT_CRITICAL(&status_lock);

    lv_label_set_text_fmt(gps_label, "GPS MODULE %s", status.gnss_online ? "ONLINE" : "NO DATA");
    if (status.fix_valid) {
        lv_label_set_text_fmt(speed_label, "FIX READY  SAT %d  %.1f km/h",
                              status.satellites, status.speed_kmh);
    } else {
        if (status.gnss_online && status.gnss_gga_seen) {
            lv_label_set_text_fmt(speed_label, "NO FIX  SAT %d", status.satellites);
        } else {
            lv_label_set_text(speed_label, status.gnss_online ? "FIX WAITING" : "CHECK GPS POWER / CABLE");
        }
    }
    lv_label_set_text_fmt(network_label, "Wi-Fi %s  %.17s",
                          status.wifi_connected ? "UP" : "DOWN", status.wifi_ssid);
    if (status.wifi_connected) {
        lv_label_set_text_fmt(ip_label, "IP %s  %d dBm", status.wifi_ip, status.wifi_rssi);
    } else {
        lv_label_set_text(ip_label, "IP --  CONNECTING");
    }
    lv_label_set_text_fmt(binding_label, "MQTT %s  BIND %s",
                          status.mqtt_connected ? "UP" : "DOWN",
                          status.binding_known ? (status.binding_bound ? "BOUND" : "UNBOUND") : "CHECK");

    if (status.binding_known && status.binding_bound) {
        lv_label_set_text(binding_code_label, "BOUND");
        lv_label_set_text(binding_expiry_label, "DEVICE BOUND TO MOTOBOX");
    } else {
        lv_label_set_text(binding_code_label, status.binding_ready ? status.binding_code : "------");
        lv_label_set_text(binding_expiry_label, status.binding_ready ? "ENTER CODE IN MOTOBOX" :
                          status.mqtt_connected ? "CHECKING BINDING" : "WAITING FOR MQTT");
    }
    if (status.binding_ready && !status.binding_bound && !binding_was_ready) {
        bind_page_visible = true;
        lv_obj_add_flag(status_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(bind_page, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "PAGE bind reason=code_ready");
    }
    binding_was_ready = status.binding_ready && !status.binding_bound;

    bool pressed = gpio_get_level(STICKS3_BUTTON_A_GPIO) == 0;
    if (pressed && !button_previous) {
        bind_page_visible = !bind_page_visible;
        lv_obj_set_flag(status_page, LV_OBJ_FLAG_HIDDEN, bind_page_visible);
        lv_obj_set_flag(bind_page, LV_OBJ_FLAG_HIDDEN, !bind_page_visible);
        ESP_LOGI(TAG, "PAGE %s", bind_page_visible ? "bind" : "status");
    }
    button_previous = pressed;
}

static void create_pages(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x090e15), 0);

    status_page = lv_obj_create(screen);
    lv_obj_remove_flag(status_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(status_page, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(status_page, 0, 0);
    lv_obj_set_style_border_width(status_page, 0, 0);
    lv_obj_set_style_radius(status_page, 0, 0);
    lv_obj_set_style_pad_all(status_page, 0, 0);
    lv_obj_set_style_bg_color(status_page, lv_color_hex(0x090e15), 0);
    lv_obj_t *title = make_label(status_page, 7, 4, 226, &lv_font_montserrat_14, 0x2de2a6);
    lv_label_set_text(title, "MotoBox GPS");
    gps_label = make_label(status_page, 7, 28, 226, &lv_font_montserrat_14, 0xe5edf5);
    speed_label = make_label(status_page, 7, 48, 226, &lv_font_montserrat_14, 0xe5edf5);
    network_label = make_label(status_page, 7, 70, 226, &lv_font_montserrat_12, 0x92a7bc);
    ip_label = make_label(status_page, 7, 88, 226, &lv_font_montserrat_12, 0x92a7bc);
    binding_label = make_label(status_page, 7, 108, 165, &lv_font_montserrat_12, 0x2de2a6);
    lv_obj_t *hint = make_label(status_page, 181, 108, 52, &lv_font_montserrat_12, 0x2de2a6);
    lv_label_set_text(hint, "A: CODE");

    bind_page = lv_obj_create(screen);
    lv_obj_remove_flag(bind_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(bind_page, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(bind_page, 0, 0);
    lv_obj_set_style_border_width(bind_page, 0, 0);
    lv_obj_set_style_radius(bind_page, 0, 0);
    lv_obj_set_style_pad_all(bind_page, 0, 0);
    lv_obj_set_style_bg_color(bind_page, lv_color_hex(0xffffff), 0);
    lv_obj_t *heading = make_label(bind_page, 8, 8, 224, &lv_font_montserrat_14, 0x18181b);
    lv_obj_set_style_text_align(heading, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(heading, "MOTOBOX BINDING CODE");
    binding_code_label = make_label(bind_page, 8, 38, 224, &lv_font_montserrat_28, 0x18181b);
    lv_obj_set_style_text_align(binding_code_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(binding_code_label, "------");
    binding_expiry_label = make_label(bind_page, 8, 79, 224, &lv_font_montserrat_12, 0x52525b);
    lv_obj_set_style_text_align(binding_expiry_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(binding_expiry_label, "WAITING FOR SERVER CODE");
    lv_obj_t *device = make_label(bind_page, 8, 101, 224, &lv_font_montserrat_12, 0x71717a);
    lv_obj_set_style_text_align(device, LV_TEXT_ALIGN_CENTER, 0);
    const char *suffix = strlen(shown_device_id) > 4 ? shown_device_id + 4 : shown_device_id;
    lv_label_set_text_fmt(device, "DEVICE %s", suffix);
    lv_obj_t *back = make_label(bind_page, 184, 120, 52, &lv_font_montserrat_12, 0x18181b);
    lv_label_set_text(back, "A: BACK");
    lv_obj_add_flag(bind_page, LV_OBJ_FLAG_HIDDEN);
    lv_timer_create(refresh_timer, 200, NULL);
}

esp_err_t ui_init(const char *device_id)
{
    if (!device_id) return ESP_ERR_INVALID_ARG;
    strncpy(shown_device_id, device_id, sizeof(shown_device_id) - 1);
    gpio_config_t button = {
        .pin_bit_mask = 1ULL << STICKS3_BUTTON_A_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&button), TAG, "button input");

    gpio_config_t backlight = {.pin_bit_mask = 1ULL << 38, .mode = GPIO_MODE_OUTPUT};
    ESP_RETURN_ON_ERROR(gpio_config(&backlight), TAG, "backlight GPIO");
    ESP_RETURN_ON_ERROR(gpio_set_level(38, 0), TAG, "backlight off");
    spi_bus_config_t spi = {
        .mosi_io_num = 39, .miso_io_num = -1, .sclk_io_num = 40,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * 20 * 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI3_HOST, &spi, SPI_DMA_CH_AUTO), TAG, "SPI init");
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = 41, .dc_gpio_num = 45, .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000, .trans_queue_depth = 10,
        .lcd_cmd_bits = 8, .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST,
                                                  &io_config, &io), TAG, "LCD IO");
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = 21,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io, &panel_config, &panel), TAG, "ST7789 create");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "LCD reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "LCD init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(panel, true), TAG, "LCD swap");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(panel, true, false), TAG, "LCD mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(panel, 40, 53), TAG, "LCD offsets");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(panel, true), TAG, "LCD inversion");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), TAG, "LCD on");

    lvgl_port_cfg_t lvgl_config = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_config.task_stack = 8192;
    lvgl_config.task_priority = 4;
    lvgl_config.task_affinity = 0;
    lvgl_config.task_max_sleep_ms = 20;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_config), TAG, "LVGL init");
    lvgl_port_display_cfg_t display_config = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = LCD_WIDTH * 20,
        .double_buffer = true,
        .hres = LCD_WIDTH,
        .vres = LCD_HEIGHT,
        .rotation = {.swap_xy = true, .mirror_x = true, .mirror_y = false},
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {.buff_dma = true, .swap_bytes = true},
    };
    ESP_RETURN_ON_FALSE(lvgl_port_add_disp(&display_config), ESP_ERR_NO_MEM, TAG, "add display");
    ESP_RETURN_ON_FALSE(lvgl_port_lock(2000), ESP_ERR_TIMEOUT, TAG, "UI lock");
    create_pages();
    lvgl_port_unlock();

    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "backlight timer");
    ledc_channel_config_t channel = {
        .gpio_num = 38,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_1,
        .duty = 560,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "backlight PWM");
    ESP_LOGI(TAG, "UI_READY display=240x135 task_stack=8192 binding_code=server");
    return ESP_OK;
}

void ui_set_status(const demo_status_t *status)
{
    if (!status) return;
    portENTER_CRITICAL(&status_lock);
    latest_status = *status;
    portEXIT_CRITICAL(&status_lock);
}

unsigned ui_stack_high_water_mark(void)
{
    TaskHandle_t handle = xTaskGetHandle("taskLVGL");
    return handle ? (unsigned)uxTaskGetStackHighWaterMark(handle) : 0;
}
