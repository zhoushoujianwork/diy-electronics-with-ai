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
#include "qrcode.h"

#define LCD_WIDTH 240
#define LCD_HEIGHT 135
#define QR_CANVAS_SIZE 116

static const char *TAG = "ui";
static portMUX_TYPE status_lock = portMUX_INITIALIZER_UNLOCKED;
static demo_status_t latest_status;
static char shown_device_id[24];
static lv_obj_t *status_page;
static lv_obj_t *bind_page;
static lv_obj_t *gps_label;
static lv_obj_t *speed_label;
static lv_obj_t *network_label;
static lv_obj_t *queue_label;
static lv_obj_t *time_label;
static lv_obj_t *qr_canvas;
static uint16_t qr_pixels[QR_CANVAS_SIZE * QR_CANVAS_SIZE];
static bool bind_page_visible;
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

static void render_qrcode(esp_qrcode_handle_t code)
{
    const int modules = esp_qrcode_get_size(code);
    const int quiet = 4;
    const int scale = QR_CANVAS_SIZE / (modules + quiet * 2);
    const int drawn = (modules + quiet * 2) * scale;
    const int offset = (QR_CANVAS_SIZE - drawn) / 2;
    for (int y = 0; y < QR_CANVAS_SIZE; ++y) {
        for (int x = 0; x < QR_CANVAS_SIZE; ++x) {
            int qx = (x - offset) / scale - quiet;
            int qy = (y - offset) / scale - quiet;
            bool inside = x >= offset && y >= offset && x < offset + drawn && y < offset + drawn;
            bool black = inside && qx >= 0 && qy >= 0 && qx < modules && qy < modules &&
                         esp_qrcode_get_module(code, qx, qy);
            qr_pixels[y * QR_CANVAS_SIZE + x] = black ? 0x0000 : 0xffff;
        }
    }
}

static void refresh_timer(lv_timer_t *timer)
{
    (void)timer;
    demo_status_t status;
    portENTER_CRITICAL(&status_lock);
    status = latest_status;
    portEXIT_CRITICAL(&status_lock);

    lv_label_set_text_fmt(gps_label, "GPS  %s  SAT %d  HDOP %.1f",
                          status.fix_valid ? "FIX" : "WAIT", status.satellites, status.hdop);
    lv_label_set_text_fmt(speed_label, "%.1f km/h   AGE %ums", status.speed_kmh,
                          (unsigned)status.fix_age_ms);
    lv_label_set_text_fmt(network_label, "Wi-Fi %s   MQTT %s",
                          status.wifi_connected ? "UP" : "DOWN",
                          status.mqtt_connected ? "UP" : "DOWN");
    lv_label_set_text_fmt(queue_label, "QUEUE %u/120   DROP %u",
                          status.queue_depth, status.queue_dropped);
    lv_label_set_text(time_label, status.time_trusted ? "UTC READY" : "WAITING FOR UTC");

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
    lv_label_set_text(title, "MotoBox GPS Demo");
    gps_label = make_label(status_page, 7, 28, 226, &lv_font_montserrat_14, 0xe5edf5);
    speed_label = make_label(status_page, 7, 50, 226, &lv_font_montserrat_14, 0xe5edf5);
    network_label = make_label(status_page, 7, 72, 226, &lv_font_montserrat_12, 0x92a7bc);
    queue_label = make_label(status_page, 7, 91, 226, &lv_font_montserrat_12, 0x92a7bc);
    time_label = make_label(status_page, 7, 110, 160, &lv_font_montserrat_12, 0x92a7bc);
    lv_obj_t *hint = make_label(status_page, 181, 110, 52, &lv_font_montserrat_12, 0x2de2a6);
    lv_label_set_text(hint, "A: BIND");

    bind_page = lv_obj_create(screen);
    lv_obj_remove_flag(bind_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(bind_page, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(bind_page, 0, 0);
    lv_obj_set_style_border_width(bind_page, 0, 0);
    lv_obj_set_style_radius(bind_page, 0, 0);
    lv_obj_set_style_pad_all(bind_page, 0, 0);
    lv_obj_set_style_bg_color(bind_page, lv_color_hex(0xffffff), 0);
    qr_canvas = lv_canvas_create(bind_page);
    lv_canvas_set_buffer(qr_canvas, qr_pixels, QR_CANVAS_SIZE, QR_CANVAS_SIZE, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(qr_canvas, 4, 9);
    lv_obj_t *scan = make_label(bind_page, 126, 12, 109, &lv_font_montserrat_14, 0x18181b);
    lv_label_set_text(scan, "SCAN IN\nMOTOBOX");
    lv_obj_t *prefix = make_label(bind_page, 126, 53, 109, &lv_font_montserrat_20, 0x18181b);
    lv_label_set_text(prefix, "BOX-");
    lv_obj_t *id_first = make_label(bind_page, 126, 80, 109, &lv_font_montserrat_14, 0x18181b);
    lv_obj_t *id_second = make_label(bind_page, 126, 99, 109, &lv_font_montserrat_14, 0x18181b);
    const char *suffix = strlen(shown_device_id) > 4 ? shown_device_id + 4 : shown_device_id;
    char first[7] = {0};
    strncpy(first, suffix, 6);
    lv_label_set_text(id_first, first);
    lv_label_set_text(id_second, strlen(suffix) > 6 ? suffix + 6 : "");
    lv_obj_t *back = make_label(bind_page, 184, 120, 52, &lv_font_montserrat_12, 0x18181b);
    lv_label_set_text(back, "A: BACK");

    esp_qrcode_config_t qr_config = ESP_QRCODE_CONFIG_DEFAULT();
    qr_config.display_func = render_qrcode;
    qr_config.max_qrcode_version = 4;
    qr_config.qrcode_ecc_level = ESP_QRCODE_ECC_MED;
    ESP_ERROR_CHECK(esp_qrcode_generate(&qr_config, shown_device_id));
    lv_obj_invalidate(qr_canvas);
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
    ESP_LOGI(TAG, "UI_READY display=240x135 task_stack=8192 qr_payload=%s", shown_device_id);
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
