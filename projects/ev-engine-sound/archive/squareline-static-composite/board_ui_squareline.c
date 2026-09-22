#include "board_ui.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_audio.h"
#include "board_config.h"
#include "engine_voice.h"
#include "powertrain_canvas.h"
#include "esp_timer.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_ft5x06.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ui.h"

static const char *TAG="board_ui";
static const float TWO_PI=6.283185307f;
enum {
    DRAWER_HEIGHT=112,
    DRAWER_CLOSED_Y=-112,
    DRAWER_AUTO_CLOSE_MS=3500,
};

static esp_lcd_panel_io_handle_t panel_io;
static esp_lcd_panel_handle_t panel;
static esp_lcd_touch_handle_t touch;
static lv_display_t *display;
static lv_indev_t *touch_indev;
static lv_obj_t *engine_visual;
static lv_obj_t *engine_fire_left,*engine_fire_right,*engine_crank_pin;
static lv_obj_t *engine_ignition_dots[6];
static lv_obj_t *engine_cylinder_label,*engine_live_label;
static lv_obj_t *engine_value_label,*exhaust_value_label;
static lv_obj_t *rpm_label,*state_label,*pulse_led;
static lv_obj_t *redline_slider,*redline_value_label,*rev_button,*rev_label,*rev_detail_label;
static lv_obj_t *volume_slider,*volume_value_label,*auto_off_slider,*auto_off_value_label;
static lv_obj_t *drawer_viewport,*drawer_panel,*drawer_grab,*header_drag,*countdown_bar;
static board_ui_action_cb_t action_callback;
static board_ui_state_cb_t state_callback;
static void *callback_context;
static uint64_t last_firings;
static bool last_pulse;
static unsigned displayed_profile=EV_PROFILES;
static unsigned displayed_exhaust=EV_EXHAUSTS;
static float crank_phase;
static bool redline_dragging,volume_dragging,auto_off_dragging;
static bool drawer_dragging;
static int32_t drawer_drag_start_y,drawer_panel_start_y;
static uint32_t drawer_auto_close_tick;
static bool ready;
static bool settings_visible;
static uint32_t draw_count,refresh_max_us,last_refresh_tick;
static uint32_t render_count,render_max_us;
static int64_t render_started;
static board_ui_state_t previous_state;
static bool have_previous_state;
static float visual_speed;

static void display_render_event(lv_event_t *event) {
    if(lv_event_get_code(event)==LV_EVENT_RENDER_START) render_started=esp_timer_get_time();
    if(lv_event_get_code(event)==LV_EVENT_RENDER_READY) {
        uint32_t us=(uint32_t)(esp_timer_get_time()-render_started);
        if(us>render_max_us) render_max_us=us;
        render_count++;
    }
}

enum { RIG_H=EV_POWERTRAIN_HEIGHT, EXHAUST_W=EV_EXHAUST_WIDTH };
_Alignas(4) static uint16_t exhaust_pixels[EXHAUST_W*RIG_H];

static lv_obj_t *engine_marker(lv_obj_t *parent,int size,uint32_t color) {
    lv_obj_t *marker=lv_obj_create(parent);
    lv_obj_set_size(marker,size,size);
    lv_obj_set_style_radius(marker,LV_RADIUS_CIRCLE,LV_PART_MAIN);
    lv_obj_set_style_bg_color(marker,lv_color_hex(color),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(marker,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_width(marker,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(marker,0,LV_PART_MAIN);
    lv_obj_clear_flag(marker,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    return marker;
}

static void update_engine_overlays(const board_ui_state_t *state) {
    unsigned cylinders=ev_profiles[state->profile].cylinders;
    if(cylinders>6) cylinders=6;
    if(cylinders<1) cylinders=1;
    unsigned active=state->last_cylinder%cylinders;
    for(unsigned i=0;i<6;i++) {
        bool visible=i<cylinders;
        bool firing=visible && state->running && i==active;
        lv_obj_set_style_bg_color(engine_ignition_dots[i],
            lv_color_hex(firing?0xFF9D24:0x3E4549),LV_PART_MAIN);
        if(visible) lv_obj_clear_flag(engine_ignition_dots[i],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(engine_ignition_dots[i],LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_bg_opa(engine_fire_left,
        state->running && !(state->last_cylinder&1u)?LV_OPA_60:LV_OPA_TRANSP,LV_PART_MAIN);
    lv_obj_set_style_bg_opa(engine_fire_right,
        state->running && (state->last_cylinder&1u)?LV_OPA_60:LV_OPA_TRANSP,LV_PART_MAIN);
    lv_obj_set_pos(engine_crank_pin,
        84+(int)lroundf(8*cosf(crank_phase)),
        91+(int)lroundf(8*sinf(crank_phase)));
    lv_label_set_text_fmt(engine_cylinder_label,"%uC",ev_profiles[state->profile].cylinders);
    lv_obj_set_style_text_color(engine_live_label,
        lv_color_hex(state->running?0xFF9D24:0x777B7E),LV_PART_MAIN);
}

static void draw_powertrain(const board_ui_state_t *state) {
    unsigned profile=state->profile<EV_PROFILES?state->profile:0;
    ev_powertrain_state_t visual={
        .profile=profile,
        .cylinders=ev_profiles[profile].cylinders,
        .exhaust=state->exhaust<EV_EXHAUSTS?state->exhaust:0,
        .last_cylinder=state->last_cylinder,
        .running=state->running,
        .throttle=state->throttle,
        .phase=fmodf(crank_phase/TWO_PI,1.f)
    };
    ev_exhaust_render(exhaust_pixels,&visual);
    update_engine_overlays(state);
    draw_count++;
    lv_obj_invalidate(engine_visual);
}

static void sync_selector_labels(const board_ui_state_t *state) {
    unsigned profile=state->profile<EV_PROFILES?state->profile:0;
    unsigned exhaust=state->exhaust<EV_EXHAUSTS?state->exhaust:0;
    displayed_profile=profile;
    displayed_exhaust=exhaust;
    lv_label_set_text_fmt(engine_value_label,"%uC  %s  >",
                          ev_profiles[profile].cylinders,ev_profiles[profile].ui_name);
    lv_label_set_text_fmt(exhaust_value_label,"%s  >",ev_exhausts[exhaust].ui_name);
    ESP_LOGI(TAG,"UI_SYNC mode=cycle_buttons profile=%s exhaust=%s",
             ev_profiles[profile].name,ev_exhausts[exhaust].name);
}

static void profile_next_event(lv_event_t *event) {
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED || !have_previous_state) return;
    unsigned current=previous_state.profile<EV_PROFILES?previous_state.profile:0;
    unsigned next=(current+1)%EV_PROFILES;
    ESP_LOGI(TAG,"TOUCH action=profile_next from=%s to=%s",
             ev_profiles[current].name,ev_profiles[next].name);
    if(action_callback) action_callback(BOARD_UI_PROFILE,(int)next,callback_context);
}

static void exhaust_next_event(lv_event_t *event) {
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED || !have_previous_state) return;
    unsigned current=previous_state.exhaust<EV_EXHAUSTS?previous_state.exhaust:0;
    unsigned next=(current+1)%EV_EXHAUSTS;
    ESP_LOGI(TAG,"TOUCH action=exhaust_next from=%s to=%s",
             ev_exhausts[current].name,ev_exhausts[next].name);
    if(action_callback) action_callback(BOARD_UI_EXHAUST,(int)next,callback_context);
}

static void volume_event(lv_event_t *event) {
    lv_event_code_t code=lv_event_get_code(event);
    if(code==LV_EVENT_PRESSED || code==LV_EVENT_VALUE_CHANGED ||
       code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST)
        drawer_auto_close_tick=lv_tick_get()+DRAWER_AUTO_CLOSE_MS;
    if(code==LV_EVENT_PRESSED) volume_dragging=true;
    if(code==LV_EVENT_VALUE_CHANGED) {
        int value=lv_slider_get_value(volume_slider);
        lv_label_set_text_fmt(volume_value_label,"%d%%",value);
    }
    if(code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST) {
        volume_dragging=false;
        int value=lv_slider_get_value(volume_slider);
        ESP_LOGI(TAG,"TOUCH action=volume_slide value=%d",value);
        if(action_callback) action_callback(BOARD_UI_VOLUME_SET,value,callback_context);
    }
}

static void redline_event(lv_event_t *event) {
    lv_event_code_t code=lv_event_get_code(event);
    if(code==LV_EVENT_PRESSED || code==LV_EVENT_VALUE_CHANGED ||
       code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST)
        drawer_auto_close_tick=lv_tick_get()+DRAWER_AUTO_CLOSE_MS;
    if(code==LV_EVENT_PRESSED) redline_dragging=true;
    if(code==LV_EVENT_VALUE_CHANGED) {
        int value=lv_slider_get_value(redline_slider);
        lv_label_set_text_fmt(redline_value_label,"%d",value);
    }
    if(code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST) {
        redline_dragging=false;
        int value=lv_slider_get_value(redline_slider);
        ESP_LOGI(TAG,"TOUCH action=redline_slide value=%d",value);
        if(action_callback) action_callback(BOARD_UI_REDLINE_SET,value,callback_context);
    }
}

static void auto_off_event(lv_event_t *event) {
    lv_event_code_t code=lv_event_get_code(event);
    if(code==LV_EVENT_PRESSED || code==LV_EVENT_VALUE_CHANGED ||
       code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST)
        drawer_auto_close_tick=lv_tick_get()+DRAWER_AUTO_CLOSE_MS;
    if(code==LV_EVENT_PRESSED) auto_off_dragging=true;
    if(code==LV_EVENT_VALUE_CHANGED) {
        int deciseconds=lv_slider_get_value(auto_off_slider);
        lv_label_set_text_fmt(auto_off_value_label,"%d.%d s",deciseconds/10,deciseconds%10);
    }
    if(code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST) {
        auto_off_dragging=false;
        int milliseconds=lv_slider_get_value(auto_off_slider)*100;
        ESP_LOGI(TAG,"TOUCH action=auto_off_slide value_ms=%d",milliseconds);
        if(action_callback) action_callback(BOARD_UI_AUTO_OFF_SET,milliseconds,callback_context);
    }
}

static void drawer_drag_event(lv_event_t *event) {
    lv_event_code_t code=lv_event_get_code(event);
    lv_indev_t *indev=lv_indev_active();
    if(!indev) return;
    lv_point_t point={0};
    lv_indev_get_point(indev,&point);
    if(code==LV_EVENT_PRESSED) {
        drawer_dragging=true;
        drawer_auto_close_tick=0;
        drawer_drag_start_y=point.y;
        drawer_panel_start_y=lv_obj_get_y(drawer_panel);
        settings_visible=true;
    } else if(code==LV_EVENT_PRESSING && drawer_dragging) {
        int32_t y=drawer_panel_start_y+point.y-drawer_drag_start_y;
        if(y<DRAWER_CLOSED_Y) y=DRAWER_CLOSED_Y;
        if(y>0) y=0;
        lv_obj_set_y(drawer_panel,y);
    } else if((code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST) && drawer_dragging) {
        drawer_dragging=false;
        bool open=lv_obj_get_y(drawer_panel)>-70;
        lv_obj_set_y(drawer_panel,open?0:DRAWER_CLOSED_Y);
        settings_visible=open;
        drawer_auto_close_tick=open?lv_tick_get()+DRAWER_AUTO_CLOSE_MS:0;
        ESP_LOGI(TAG,"TOUCH action=settings_drawer value=%s",open?"open":"closed");
    }
}

static void rev_event(lv_event_t *event) {
    lv_event_code_t code=lv_event_get_code(event);
    if(code==LV_EVENT_PRESSED) {
        ESP_LOGI(TAG,"TOUCH action=rev state=PRESSED");
        if(action_callback) action_callback(BOARD_UI_REV_PRESS,100,callback_context);
    } else if(code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST) {
        ESP_LOGI(TAG,"TOUCH action=rev state=RELEASED");
        if(action_callback) action_callback(BOARD_UI_REV_RELEASE,0,callback_context);
    }
}

static void refresh_timer(lv_timer_t *timer) {
    (void)timer;
    if(!state_callback) return;
    int64_t begin=esp_timer_get_time();
    uint32_t now=lv_tick_get();
    if(settings_visible && drawer_auto_close_tick && !drawer_dragging &&
       !volume_dragging && !redline_dragging && !auto_off_dragging &&
       (int32_t)(now-drawer_auto_close_tick)>=0) {
        lv_obj_set_y(drawer_panel,DRAWER_CLOSED_Y);
        settings_visible=false;
        drawer_auto_close_tick=0;
        ESP_LOGI(TAG,"UI_AUTO_CLOSE menu=pull_down idle_ms=%u",(unsigned)DRAWER_AUTO_CLOSE_MS);
    }
    float dt=last_refresh_tick?(now-last_refresh_tick)*.001f:.033f;
    last_refresh_tick=now;
    if(dt>.10f) dt=.10f;
    board_ui_state_t state={0};
    if(!state_callback(&state,callback_context)) return;
    if(state.profile>=EV_PROFILES) state.profile=0;
    if(state.redline_rpm<1) state.redline_rpm=1;
    bool changed=!have_previous_state;
    bool moving=state.rpm>1 || visual_speed>.01f;
    float ratio=fminf(1,fmaxf(0,state.rpm/(float)state.redline_rpm));
    float speed=state.rpm>1?.5f+2.5f*sqrtf(ratio):0;
    visual_speed+=(speed-visual_speed)*fminf(1,dt*7);
    crank_phase=fmodf(crank_phase+visual_speed*TWO_PI*dt,2*TWO_PI);
    if(state.profile!=displayed_profile || state.exhaust!=displayed_exhaust)
        sync_selector_labels(&state);
    /* Static text/style updates only on change; UI input keeps its own 16 ms timer. */
    int rpm=(int)lroundf(state.rpm/10)*10;
    int previous_rpm=(int)lroundf(previous_state.rpm/10)*10;
    if(changed || rpm!=previous_rpm) lv_label_set_text_fmt(rpm_label,"%d RPM",rpm);
    if(changed || state.running!=previous_state.running || state.fault!=previous_state.fault ||
       state.phase!=previous_state.phase) {
        lv_label_set_text(state_label,state.fault?"ERR":ev_phase_name(state.phase));
        uint32_t state_color=state.fault?0xEE4445:
            state.phase==EV_PHASE_COAST?0xFDBD2E:state.running?0xF4802A:0x777B7E;
        lv_obj_set_style_text_color(state_label,lv_color_hex(state_color),LV_PART_MAIN);
    }
    bool pressed=state.throttle>.02f;
    unsigned remaining_ds=(state.auto_off_remaining_ms+99)/100;
    unsigned old_remaining_ds=(previous_state.auto_off_remaining_ms+99)/100;
    if(changed || pressed!=(previous_state.throttle>.02f) ||
       remaining_ds!=old_remaining_ds || state.running!=previous_state.running) {
        if(pressed) {
            lv_label_set_text(rev_label,"THROTTLE OPEN");
            lv_label_set_text(rev_detail_label,"RELEASE = AUTO OFF");
        } else if(state.running && remaining_ds) {
            lv_label_set_text(rev_label,"THROTTLE RELEASED");
            lv_label_set_text_fmt(rev_detail_label,"AUTO OFF IN %u.%u s",
                                  remaining_ds/10,remaining_ds%10);
        } else {
            lv_label_set_text(rev_label,"PRESS + HOLD");
            lv_label_set_text(rev_detail_label,"STARTS ENGINE AUTOMATICALLY");
        }
        int countdown=state.auto_off_ms?
            (int)(100U*state.auto_off_remaining_ms/state.auto_off_ms):0;
        lv_bar_set_value(countdown_bar,countdown,LV_ANIM_OFF);
        lv_obj_set_style_border_color(rev_button,
            lv_color_hex(state.running?0xEE4445:0x777B7E),LV_PART_MAIN);
    }
    if(!volume_dragging && (changed || state.volume!=previous_state.volume)) {
        int volume=(int)lroundf(state.volume*100);
        lv_slider_set_value(volume_slider,volume,LV_ANIM_OFF);
        lv_label_set_text_fmt(volume_value_label,"%d%%",(int)lroundf(state.volume*100));
    }
    unsigned minimum=(unsigned)ev_profiles[state.profile].idle_rpm+500;
    if(changed || state.profile!=previous_state.profile)
        lv_slider_set_range(redline_slider,(int32_t)minimum,EV_MAX_RPM);
    if(!redline_dragging && (changed || state.redline_rpm!=previous_state.redline_rpm)) {
        lv_slider_set_value(redline_slider,(int32_t)state.redline_rpm,LV_ANIM_OFF);
        lv_label_set_text_fmt(redline_value_label,"%u",state.redline_rpm);
    }
    if(!auto_off_dragging && (changed || state.auto_off_ms!=previous_state.auto_off_ms)) {
        unsigned deciseconds=state.auto_off_ms/100;
        lv_slider_set_value(auto_off_slider,(int32_t)deciseconds,LV_ANIM_OFF);
        lv_label_set_text_fmt(auto_off_value_label,"%u.%u s",deciseconds/10,deciseconds%10);
    }
    unsigned zone=state.fault?3:ratio>.82f?2:ratio>.58f?1:0;
    float old_ratio=previous_state.redline_rpm?previous_state.rpm/previous_state.redline_rpm:0;
    unsigned old_zone=previous_state.fault?3:old_ratio>.82f?2:old_ratio>.58f?1:0;
    if(changed || zone!=old_zone) {
        uint32_t color=zone>=2?0xFF637C:zone==1?0xFFBA66:0x60DCB0;
        lv_obj_set_style_text_color(rpm_label,lv_color_hex(color),LV_PART_MAIN);
    }
    bool fired=state.running && state.firings!=last_firings;
    if(changed || fired!=last_pulse)
        lv_obj_set_style_bg_opa(pulse_led,fired?LV_OPA_COVER:LV_OPA_30,LV_PART_MAIN);
    last_pulse=fired;
    last_firings=state.firings;
    if(!settings_visible && (moving || changed || state.profile!=previous_state.profile ||
       state.exhaust!=previous_state.exhaust || state.running!=previous_state.running))
        draw_powertrain(&state);
    previous_state=state; have_previous_state=true;
    uint32_t elapsed=(uint32_t)(esp_timer_get_time()-begin);
    if(elapsed>refresh_max_us) refresh_max_us=elapsed;
}

static void create_ui(void) {
    ui_init();

    engine_value_label=ui_EngineSelectorValue;
    exhaust_value_label=ui_ExhaustSelectorValue;
    rpm_label=ui_RpmValue;
    state_label=ui_RunState;
    pulse_led=ui_PulseLed;
    rev_button=ui_ThrottleButton;
    rev_label=ui_ThrottleState;
    rev_detail_label=ui_ThrottleDetail;
    countdown_bar=ui_CountdownBar;
    drawer_viewport=ui_DrawerViewport;
    drawer_panel=ui_SettingsDrawer;
    drawer_grab=ui_DrawerGrab;
    header_drag=ui_HeaderDrag;
    volume_slider=ui_VolumeSlider;
    volume_value_label=ui_VolumeSliderValue;
    redline_slider=ui_RedlineSlider;
    redline_value_label=ui_RedlineSliderValue;
    auto_off_slider=ui_AutoOffSlider;
    auto_off_value_label=ui_AutoOffSliderValue;

    lv_obj_add_flag(ui_PowertrainPlaceholder,LV_OBJ_FLAG_HIDDEN);
    engine_visual=lv_canvas_create(ui_PowertrainHost);
    lv_canvas_set_buffer(engine_visual,exhaust_pixels,EXHAUST_W,RIG_H,LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(engine_visual,172,0);
    lv_obj_set_size(engine_visual,EXHAUST_W,RIG_H);
    lv_obj_set_style_bg_color(engine_visual,lv_color_hex(0x0D1113),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(engine_visual,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_width(engine_visual,0,LV_PART_MAIN);
    lv_obj_set_style_radius(engine_visual,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(engine_visual,0,LV_PART_MAIN);
    lv_obj_clear_flag(engine_visual,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);

    engine_fire_left=engine_marker(ui_PowertrainHost,8,0xFF9D24);
    lv_obj_set_pos(engine_fire_left,49,37);
    lv_obj_set_style_bg_opa(engine_fire_left,LV_OPA_TRANSP,LV_PART_MAIN);
    engine_fire_right=engine_marker(ui_PowertrainHost,8,0xFF9D24);
    lv_obj_set_pos(engine_fire_right,117,37);
    lv_obj_set_style_bg_opa(engine_fire_right,LV_OPA_TRANSP,LV_PART_MAIN);
    engine_crank_pin=engine_marker(ui_PowertrainHost,5,0xD6D9DB);
    for(unsigned i=0;i<6;i++) {
        engine_ignition_dots[i]=engine_marker(ui_PowertrainHost,6,0x3E4549);
        lv_obj_set_pos(engine_ignition_dots[i],38+(int)i*17,98);
    }
    engine_cylinder_label=lv_label_create(ui_PowertrainHost);
    lv_obj_set_pos(engine_cylinder_label,150,3);
    lv_obj_set_size(engine_cylinder_label,17,8);
    lv_label_set_text(engine_cylinder_label,"4C");
    lv_obj_set_style_text_font(engine_cylinder_label,&lv_font_montserrat_8,LV_PART_MAIN);
    lv_obj_set_style_text_color(engine_cylinder_label,lv_color_hex(0x777B7E),LV_PART_MAIN);
    engine_live_label=lv_label_create(ui_PowertrainHost);
    lv_obj_set_pos(engine_live_label,143,96);
    lv_obj_set_size(engine_live_label,26,8);
    lv_label_set_text(engine_live_label,"LIVE");
    lv_obj_set_style_text_font(engine_live_label,&lv_font_montserrat_8,LV_PART_MAIN);
    lv_obj_set_style_text_align(engine_live_label,LV_TEXT_ALIGN_RIGHT,LV_PART_MAIN);
    lv_obj_set_style_text_color(engine_live_label,lv_color_hex(0x777B7E),LV_PART_MAIN);

    lv_slider_set_range(volume_slider,0,100);
    lv_slider_set_value(volume_slider,60,LV_ANIM_OFF);
    lv_slider_set_range(redline_slider,1800,EV_MAX_RPM);
    lv_slider_set_value(redline_slider,8000,LV_ANIM_OFF);
    lv_slider_set_range(auto_off_slider,10,80);
    lv_slider_set_value(auto_off_slider,30,LV_ANIM_OFF);
    lv_bar_set_range(countdown_bar,0,100);
    lv_bar_set_value(countdown_bar,0,LV_ANIM_OFF);

    lv_obj_add_event_cb(ui_EngineSelector,profile_next_event,LV_EVENT_CLICKED,NULL);
    lv_obj_add_event_cb(ui_ExhaustSelector,exhaust_next_event,LV_EVENT_CLICKED,NULL);
    lv_obj_add_event_cb(rev_button,rev_event,LV_EVENT_ALL,NULL);
    lv_obj_add_event_cb(volume_slider,volume_event,LV_EVENT_ALL,NULL);
    lv_obj_add_event_cb(redline_slider,redline_event,LV_EVENT_ALL,NULL);
    lv_obj_add_event_cb(auto_off_slider,auto_off_event,LV_EVENT_ALL,NULL);
    lv_obj_add_event_cb(drawer_grab,drawer_drag_event,LV_EVENT_ALL,NULL);
    lv_obj_add_event_cb(header_drag,drawer_drag_event,LV_EVENT_ALL,NULL);
    lv_obj_move_foreground(drawer_viewport);
    lv_obj_move_foreground(header_drag);

    lv_timer_create(refresh_timer,33,NULL);
}

static esp_err_t init_backlight(void) {
    const ledc_timer_config_t timer_config={
        .speed_mode=LEDC_LOW_SPEED_MODE,
        .duty_resolution=LEDC_TIMER_10_BIT,
        .timer_num=LEDC_TIMER_1,
        .freq_hz=EV_DISPLAY_BL_PWM_HZ,
        .clk_cfg=LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config),TAG,"backlight timer");
    const ledc_channel_config_t channel_config={
        .gpio_num=EV_DISPLAY_BL_GPIO,
        .speed_mode=LEDC_LOW_SPEED_MODE,
        .channel=LEDC_CHANNEL_3,
        .timer_sel=LEDC_TIMER_1,
        .duty=EV_DISPLAY_BL_PWM_DUTY,
        .hpoint=0,
    };
    return ledc_channel_config(&channel_config);
}

esp_err_t board_ui_init(i2c_master_bus_handle_t shared_i2c,
                        board_ui_action_cb_t action_cb,
                        board_ui_state_cb_t state_cb,
                        void *context) {
    ESP_RETURN_ON_FALSE(shared_i2c && action_cb && state_cb,ESP_ERR_INVALID_ARG,TAG,"invalid UI callbacks/bus");
    action_callback=action_cb;
    state_callback=state_cb;
    callback_context=context;

    const spi_bus_config_t bus_config={
        .sclk_io_num=EV_DISPLAY_SCLK_GPIO,
        .mosi_io_num=EV_DISPLAY_MOSI_GPIO,
        .miso_io_num=-1,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=EV_DISPLAY_WIDTH*EV_DISPLAY_BUFFER_LINES*(int)sizeof(uint16_t),
    };
    esp_err_t err=spi_bus_initialize((spi_host_device_t)EV_DISPLAY_SPI_HOST,
                                     &bus_config,SPI_DMA_CH_AUTO);
    if(err!=ESP_OK && err!=ESP_ERR_INVALID_STATE) return err;
    const esp_lcd_panel_io_spi_config_t io_config={
        .dc_gpio_num=EV_DISPLAY_DC_GPIO,
        .cs_gpio_num=EV_DISPLAY_CS_GPIO,
        .pclk_hz=EV_DISPLAY_PCLK_HZ,
        .spi_mode=EV_DISPLAY_SPI_MODE,
        .trans_queue_depth=10,
        .lcd_cmd_bits=8,
        .lcd_param_bits=8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)EV_DISPLAY_SPI_HOST,&io_config,&panel_io),TAG,"panel io");
    const esp_lcd_panel_dev_config_t panel_config={
        .reset_gpio_num=EV_DISPLAY_RST_GPIO,
        .rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel=16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(panel_io,&panel_config,&panel),TAG,"ST7789 panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel),TAG,"panel reset");
    ESP_RETURN_ON_ERROR(board_audio_set_lcd_selected(true),TAG,"LCD CS select");
    vTaskDelay(pdMS_TO_TICKS(150));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel),TAG,"panel init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(panel,EV_DISPLAY_SWAP_XY),TAG,"panel swap xy");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(panel,EV_DISPLAY_MIRROR_X,
                                             EV_DISPLAY_MIRROR_Y),TAG,"panel mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(panel,EV_DISPLAY_INVERT_COLOR),TAG,"panel invert");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel,true),TAG,"panel on");

    lvgl_port_cfg_t lvgl_config=ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_config.task_priority=4;
    lvgl_config.task_stack=10240;
    lvgl_config.task_affinity=0;
    lvgl_config.task_max_sleep_ms=16;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_config),TAG,"LVGL port");
    const lvgl_port_display_cfg_t display_config={
        .io_handle=panel_io,
        .panel_handle=panel,
        .buffer_size=EV_DISPLAY_WIDTH*EV_DISPLAY_BUFFER_LINES,
        .double_buffer=true,
        .hres=EV_DISPLAY_WIDTH,
        .vres=EV_DISPLAY_HEIGHT,
        .monochrome=false,
        .rotation={
            .swap_xy=EV_DISPLAY_SWAP_XY,
            .mirror_x=EV_DISPLAY_MIRROR_X,
            .mirror_y=EV_DISPLAY_MIRROR_Y,
        },
        .color_format=LV_COLOR_FORMAT_RGB565,
        .flags={.buff_dma=true,.swap_bytes=true},
    };
    display=lvgl_port_add_disp(&display_config);
    ESP_RETURN_ON_FALSE(display,ESP_FAIL,TAG,"LVGL display add");

    esp_lcd_panel_io_handle_t touch_io=NULL;
    esp_lcd_panel_io_i2c_config_t touch_io_config=ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    touch_io_config.scl_speed_hz=400000;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(shared_i2c,&touch_io_config,&touch_io),TAG,"touch io");
    const esp_lcd_touch_config_t touch_config={
        .x_max=EV_DISPLAY_HEIGHT,
        .y_max=EV_DISPLAY_WIDTH,
        .rst_gpio_num=EV_TOUCH_RST_GPIO,
        .int_gpio_num=EV_TOUCH_INT_GPIO,
        .levels={.reset=0,.interrupt=0},
        .flags={
            .swap_xy=EV_TOUCH_SWAP_XY,
            .mirror_x=EV_TOUCH_MIRROR_X,
            .mirror_y=EV_TOUCH_MIRROR_Y,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_ft5x06(touch_io,&touch_config,&touch),TAG,"FT6336 touch");
    const lvgl_port_touch_cfg_t port_touch_config={.disp=display,.handle=touch};
    touch_indev=lvgl_port_add_touch(&port_touch_config);
    ESP_RETURN_ON_FALSE(touch_indev,ESP_FAIL,TAG,"LVGL touch add");

    ESP_RETURN_ON_FALSE(lvgl_port_lock(2000),ESP_ERR_TIMEOUT,TAG,"LVGL create lock");
    lv_timer_set_period(lv_display_get_refr_timer(display),16);
    lv_display_add_event_cb(display,display_render_event,LV_EVENT_ALL,NULL);
    lv_timer_set_period(lv_indev_get_read_timer(touch_indev),16);
    lv_indev_set_scroll_limit(touch_indev,6);
    lv_indev_set_scroll_throw(touch_indev,12);
    create_ui();
    lvgl_port_unlock();
    ESP_RETURN_ON_ERROR(init_backlight(),TAG,"backlight");
    ready=true;
    ESP_LOGI(TAG,"UI_READY panel=ST7789 320x240 touch=FT6336 lvgl_stack=10240 animation=powertrain_rig layout=dual_panel_v2 refresh_ms=16 motion_ms=33 selector=cycle_buttons menu=pull_down drawer_auto_close_ms=%u font_scale=compact_8_10",(unsigned)DRAWER_AUTO_CLOSE_MS);
    return ESP_OK;
}

unsigned board_ui_stack_high_water_mark(void) {
    TaskHandle_t handle=xTaskGetHandle("taskLVGL");
    return handle?(unsigned)uxTaskGetStackHighWaterMark(handle):0;
}

esp_err_t board_ui_report(void) {
    ESP_RETURN_ON_FALSE(ready,ESP_ERR_INVALID_STATE,TAG,"UI not ready");
    ESP_RETURN_ON_FALSE(lvgl_port_lock(1000),ESP_ERR_TIMEOUT,TAG,"UI readback lock");
    unsigned profile=displayed_profile<EV_PROFILES?displayed_profile:0;
    unsigned draws=draw_count,renders=render_count,render_us=render_max_us,refresh_us=refresh_max_us;
    unsigned exhaust=displayed_exhaust<EV_EXHAUSTS?displayed_exhaust:0;
    unsigned auto_off_ms=previous_state.auto_off_ms;
    bool drawer_open=lv_obj_get_y(drawer_panel)==0;
    lvgl_port_unlock();
    ESP_LOGI(TAG,"UI_READBACK ready=1 stack_lvgl=%u animation=powertrain_rig layout=dual_panel_v2 profile=%s exhaust=%s render_count=%u render_max_us=%u update_max_us=%u draw_passes=%u selector=cycle_buttons menu=pull_down drawer_open=%d drawer_auto_close_ms=%u font_scale=compact_8_10 auto_off_ms=%u",
             board_ui_stack_high_water_mark(),ev_profiles[profile].name,ev_exhausts[exhaust].name,
             renders,render_us,refresh_us,draws,drawer_open,
             (unsigned)DRAWER_AUTO_CLOSE_MS,auto_off_ms);
    return ESP_OK;
}
