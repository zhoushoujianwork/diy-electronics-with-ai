#include "board_ui.h"
#include "board_config.h"
#include "engine_canvas.h"
#include "stick_controls.h"
#include <math.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG="stick_ui";
static board_ui_action_cb_t action_cb;
static board_ui_state_cb_t state_cb;
static void *cb_context;
static lv_obj_t *title,*rpm_text,*status_text,*canvas,*hint,*setting,*help;
static ev_stick_controls_t keys;
static board_ui_state_t previous;
static bool have_previous,ready;
static float phase,speed;
static uint32_t last_frame,draw_count,render_count,render_max_us,update_max_us;
static int64_t render_started;
_Alignas(4) static uint16_t pixels[240*80];

static void action(board_ui_action_t code,int value) {
    ESP_LOGI(TAG,"BUTTON action=%d value=%d page=%d",code,value,keys.page);
    action_cb(code,value,cb_context);
}
static void render_event(lv_event_t *e) {
    if(lv_event_get_code(e)==LV_EVENT_RENDER_START) render_started=esp_timer_get_time();
    if(lv_event_get_code(e)==LV_EVENT_RENDER_READY) {
        uint32_t us=(uint32_t)(esp_timer_get_time()-render_started);
        if(us>render_max_us) render_max_us=us;
        render_count++;
    }
}
static void show_page(void) {
    bool main=keys.page==EV_STICK_MAIN;
    lv_obj_set_flag(canvas,LV_OBJ_FLAG_HIDDEN,!main);
    lv_obj_set_flag(setting,LV_OBJ_FLAG_HIDDEN,main);
    lv_obj_set_flag(help,LV_OBJ_FLAG_HIDDEN,main);
    lv_label_set_text(hint,main?"A:REV  B:NEXT  HOLD B:SET":"A:-   B:+   HOLD B:NEXT");
    if(!main) lv_label_set_text(help,keys.page==EV_STICK_VOLUME?
        "Battery: keep below 75%\nA+B: stop engine":"Virtual engine RPM limit\nA+B: stop engine");
    have_previous=false;
    ESP_LOGI(TAG,"STATE_TRANSITION: UI_PAGE -> %d",keys.page);
}
static void poll_keys(lv_timer_t *timer) {
    (void)timer;
    unsigned events=ev_stick_poll(&keys,lv_tick_get(),
        gpio_get_level(EV_BUTTON_A_GPIO)==0,gpio_get_level(EV_BUTTON_B_GPIO)==0);
    if(!events) return;
    board_ui_state_t s;
    /* Release never depends on a status-query success. */
    if(events&EV_KEY_REV_OFF) action(BOARD_UI_REV_RELEASE,0);
    if(events&EV_KEY_STOP) action(BOARD_UI_ENGINE_STOP,0);
    if(events&EV_KEY_PAGE) show_page();
    if(!state_cb(&s,cb_context)) {
        ESP_LOGW(TAG,"BUTTON status unavailable; events=0x%x",events);
        return;
    }
    if(events&EV_KEY_REV_ON) action(BOARD_UI_REV_PRESS,100);
    if(events&EV_KEY_NEXT) action(BOARD_UI_PROFILE,(s.profile+1)%EV_PROFILES);
    int delta=(events&EV_KEY_PLUS)?1:(events&EV_KEY_MINUS)?-1:0;
    if(delta) action(keys.page==EV_STICK_VOLUME?BOARD_UI_VOLUME_DELTA:BOARD_UI_REDLINE_DELTA,
                     delta*(keys.page==EV_STICK_VOLUME?5:500));
}
static void refresh(lv_timer_t *timer) {
    (void)timer;
    int64_t begin=esp_timer_get_time();
    board_ui_state_t s;
    if(!state_cb(&s,cb_context)) return;
    if(s.profile>=EV_PROFILES) return;
    uint32_t now=lv_tick_get();
    float dt=fminf((uint32_t)(now-last_frame)*.001f,.1f);
    last_frame=now;
    bool changed=!have_previous || previous.profile!=s.profile;
    if(changed) {
        lv_label_set_text(title,keys.page==EV_STICK_MAIN?ev_profiles[s.profile].ui_name:
            keys.page==EV_STICK_VOLUME?"SPEAKER VOLUME":"REDLINE RPM");
        if(have_previous && previous.profile!=s.profile) phase=0;
    }
    if(!have_previous || (int)s.rpm!=(int)previous.rpm)
        lv_label_set_text_fmt(rpm_text,"%5d RPM",(int)s.rpm);
    if(!have_previous || s.phase!=previous.phase || s.volume!=previous.volume || s.fault!=previous.fault)
        lv_label_set_text_fmt(status_text,"%s  VOL %d",s.fault?"FAULT":ev_phase_name(s.phase),(int)lroundf(s.volume*100));
    if(keys.page!=EV_STICK_MAIN) {
        if(!have_previous || s.volume!=previous.volume || s.redline_rpm!=previous.redline_rpm) {
            if(keys.page==EV_STICK_VOLUME) lv_label_set_text_fmt(setting,"%d%%",(int)lroundf(s.volume*100));
            else lv_label_set_text_fmt(setting,"%u",s.redline_rpm);
        }
    } else if(s.running || previous.running || changed) {
        float target=s.running?fminf(1.7f,.20f+s.rpm/11000.0f):0;
        speed+=(target-speed)*fminf(1,dt*8);
        phase=fmodf(phase+speed*dt,1);
        ev_canvas_render(pixels,240,80,s.profile,phase,s.running);
        lv_obj_invalidate(canvas);
        draw_count++;
    }
    previous=s; have_previous=true;
    uint32_t us=(uint32_t)(esp_timer_get_time()-begin);
    if(us>update_max_us) update_max_us=us;
}
static lv_obj_t *label(lv_obj_t *root,int x,int y,int width,const lv_font_t *font,uint32_t color) {
    lv_obj_t *obj=lv_label_create(root);
    lv_obj_set_pos(obj,x,y); lv_obj_set_width(obj,width);
    lv_label_set_long_mode(obj,LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(obj,font,0);
    lv_obj_set_style_text_color(obj,lv_color_hex(color),0);
    lv_label_set_text(obj,"");
    return obj;
}
static void create_ui(void) {
    lv_obj_t *root=lv_screen_active();
    lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root,lv_color_hex(0x090E15),0);
    title=label(root,6,2,228,&lv_font_montserrat_14,0x2DE2A6);
    rpm_text=label(root,6,20,88,&lv_font_montserrat_14,0xE5EDF5);
    status_text=label(root,100,22,136,&lv_font_montserrat_12,0x92A7BC);
    canvas=lv_canvas_create(root);
    lv_canvas_set_buffer(canvas,pixels,240,80,LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(canvas,0,38);
    setting=label(root,8,46,224,&lv_font_montserrat_28,0xE5EDF5);
    help=label(root,8,82,224,&lv_font_montserrat_12,0x92A7BC);
    hint=label(root,4,120,232,&lv_font_montserrat_12,0x92A7BC);
    show_page();
    last_frame=lv_tick_get();
    lv_timer_create(poll_keys,10,NULL);
    lv_timer_create(refresh,33,NULL);
}

esp_err_t board_ui_init(i2c_master_bus_handle_t shared_i2c,board_ui_action_cb_t cb,
                        board_ui_state_cb_t snapshot,void *context) {
    ESP_RETURN_ON_FALSE(shared_i2c && cb && snapshot,ESP_ERR_INVALID_ARG,TAG,"callbacks/bus");
    action_cb=cb; state_cb=snapshot; cb_context=context;
    gpio_config_t buttons={.pin_bit_mask=(1ULL<<EV_BUTTON_A_GPIO)|(1ULL<<EV_BUTTON_B_GPIO),
        .mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};
    ESP_RETURN_ON_ERROR(gpio_config(&buttons),TAG,"button inputs");
    /* Do not interpret a button held through reset/download as throttle. */
    keys.blocked=true;
    gpio_config_t bl={.pin_bit_mask=1ULL<<EV_DISPLAY_BL_GPIO,.mode=GPIO_MODE_OUTPUT};
    ESP_RETURN_ON_ERROR(gpio_config(&bl),TAG,"backlight GPIO");
    ESP_RETURN_ON_ERROR(gpio_set_level(EV_DISPLAY_BL_GPIO,0),TAG,"backlight off");
    spi_bus_config_t spi={.mosi_io_num=EV_DISPLAY_MOSI_GPIO,.miso_io_num=-1,
        .sclk_io_num=EV_DISPLAY_SCLK_GPIO,.quadwp_io_num=-1,.quadhd_io_num=-1,
        .max_transfer_sz=EV_DISPLAY_WIDTH*EV_DISPLAY_BUFFER_LINES*2};
    ESP_RETURN_ON_ERROR(spi_bus_initialize(EV_DISPLAY_SPI_HOST,&spi,SPI_DMA_CH_AUTO),TAG,"SPI init");
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg={.cs_gpio_num=EV_DISPLAY_CS_GPIO,.dc_gpio_num=EV_DISPLAY_DC_GPIO,
        .spi_mode=EV_DISPLAY_SPI_MODE,.pclk_hz=EV_DISPLAY_PCLK_HZ,
        .trans_queue_depth=10,.lcd_cmd_bits=8,.lcd_param_bits=8};
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EV_DISPLAY_SPI_HOST,&io_cfg,&io),TAG,"LCD IO");
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_dev_config_t panel_cfg={.reset_gpio_num=EV_DISPLAY_RST_GPIO,
        .rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB,.bits_per_pixel=16};
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io,&panel_cfg,&panel),TAG,"ST7789 create");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel),TAG,"LCD reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel),TAG,"LCD init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(panel,EV_DISPLAY_SWAP_XY),TAG,"LCD swap");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(panel,EV_DISPLAY_MIRROR_X,EV_DISPLAY_MIRROR_Y),TAG,"LCD mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(panel,EV_DISPLAY_GAP_X,EV_DISPLAY_GAP_Y),TAG,"LCD offsets");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(panel,true),TAG,"LCD inversion");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel,true),TAG,"LCD on");
    lvgl_port_cfg_t lv_cfg=ESP_LVGL_PORT_INIT_CONFIG();
    /* 10KiB: rasterizer + LVGL flush/layout + button callback/status snapshot.
     * Canvas (38.4KiB) is static, not on task stack. Measure HWM on hardware. */
    lv_cfg.task_stack=10240; lv_cfg.task_priority=4; lv_cfg.task_affinity=0;
    lv_cfg.task_max_sleep_ms=10;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lv_cfg),TAG,"LVGL init");
    lvgl_port_display_cfg_t disp_cfg={.io_handle=io,.panel_handle=panel,
        .buffer_size=EV_DISPLAY_WIDTH*EV_DISPLAY_BUFFER_LINES,.double_buffer=true,
        .hres=EV_DISPLAY_WIDTH,.vres=EV_DISPLAY_HEIGHT,
        .rotation={.swap_xy=EV_DISPLAY_SWAP_XY,.mirror_x=EV_DISPLAY_MIRROR_X,.mirror_y=EV_DISPLAY_MIRROR_Y},
        .color_format=LV_COLOR_FORMAT_RGB565,.flags={.buff_dma=true,.swap_bytes=true}};
    lv_display_t *display=lvgl_port_add_disp(&disp_cfg);
    ESP_RETURN_ON_FALSE(display,ESP_ERR_NO_MEM,TAG,"display allocation");
    ESP_RETURN_ON_FALSE(lvgl_port_lock(2000),ESP_ERR_TIMEOUT,TAG,"UI lock");
    lv_timer_set_period(lv_display_get_refr_timer(display),16);
    lv_display_add_event_cb(display,render_event,LV_EVENT_ALL,NULL);
    create_ui(); lvgl_port_unlock();
    ledc_timer_config_t timer={.speed_mode=LEDC_LOW_SPEED_MODE,.duty_resolution=LEDC_TIMER_10_BIT,
        .timer_num=LEDC_TIMER_1,.freq_hz=5000,.clk_cfg=LEDC_AUTO_CLK};
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer),TAG,"backlight timer");
    ledc_channel_config_t channel={.gpio_num=EV_DISPLAY_BL_GPIO,.speed_mode=LEDC_LOW_SPEED_MODE,
        .channel=LEDC_CHANNEL_0,.timer_sel=LEDC_TIMER_1,.duty=512};
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel),TAG,"backlight PWM");
    ready=true;
    ESP_LOGI(TAG,"UI_READY panel=ST7789 240x135 touch=NONE lvgl_stack=10240 animation=slider_crank selector=buttons");
    return ESP_OK;
}
unsigned board_ui_stack_high_water_mark(void) {
    TaskHandle_t handle=xTaskGetHandle("taskLVGL");
    return handle?(unsigned)uxTaskGetStackHighWaterMark(handle):0;
}
esp_err_t board_ui_report(void) {
    ESP_RETURN_ON_FALSE(ready,ESP_ERR_INVALID_STATE,TAG,"UI not ready");
    ESP_RETURN_ON_FALSE(lvgl_port_lock(1000),ESP_ERR_TIMEOUT,TAG,"readback lock");
    unsigned profile=previous.profile<EV_PROFILES?previous.profile:0;
    unsigned draws=draw_count,renders=render_count,render_us=render_max_us,update_us=update_max_us;
    unsigned page=keys.page;
    lvgl_port_unlock();
    ESP_LOGI(TAG,"UI_READBACK ready=1 stack_lvgl=%u animation=slider_crank profile=%s render_count=%u render_max_us=%u update_max_us=%u draw_passes=%u scrolling=0 page=%u",
        board_ui_stack_high_water_mark(),ev_profiles[profile].name,renders,render_us,update_us,draws,page);
    return ESP_OK;
}
