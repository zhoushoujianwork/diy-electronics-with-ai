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

static const char *TAG="board_ui";
static const float TWO_PI=6.283185307f;
enum { DRAWER_HEIGHT=112, DRAWER_CLOSED_Y=-112 };

static esp_lcd_panel_io_handle_t panel_io;
static esp_lcd_panel_handle_t panel;
static esp_lcd_touch_handle_t touch;
static lv_display_t *display;
static lv_indev_t *touch_indev;
static lv_obj_t *main_page;
static lv_obj_t *engine_visual;
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

static void style_button(lv_obj_t *button,uint32_t color,int radius) {
    lv_obj_set_style_bg_color(button,lv_color_hex(color),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_width(button,1,LV_PART_MAIN);
    lv_obj_set_style_border_color(button,lv_color_hex(0x777B7E),LV_PART_MAIN);
    lv_obj_set_style_radius(button,radius,LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(button,0,LV_PART_MAIN);
}

enum { RIG_W=EV_POWERTRAIN_WIDTH, RIG_H=EV_POWERTRAIN_HEIGHT };
_Alignas(4) static uint16_t engine_pixels[RIG_W*RIG_H];

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
    ev_powertrain_render(engine_pixels,&visual);
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

static lv_obj_t *create_page(lv_obj_t *screen) {
    lv_obj_t *page=lv_obj_create(screen);
    lv_obj_set_pos(page,0,0);
    lv_obj_set_size(page,320,240);
    lv_obj_set_style_bg_color(page,lv_color_hex(0x0E1012),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(page,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_width(page,0,LV_PART_MAIN);
    lv_obj_set_style_radius(page,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(page,0,LV_PART_MAIN);
    lv_obj_clear_flag(page,LV_OBJ_FLAG_SCROLLABLE);
    return page;
}

static lv_obj_t *create_cycle_selector(lv_obj_t *parent,int x,const char *title,
                                       uint32_t accent,lv_obj_t **value_label,
                                       lv_event_cb_t event_cb) {
    lv_obj_t *button=lv_button_create(parent);
    lv_obj_set_pos(button,x,140);
    lv_obj_set_size(button,153,38);
    style_button(button,0x0E1012,0);
    lv_obj_set_style_border_color(button,lv_color_hex(accent),LV_PART_MAIN);
    lv_obj_t *caption=lv_label_create(button);
    lv_label_set_text(caption,title);
    lv_obj_set_pos(caption,7,2);
    lv_obj_set_style_text_font(caption,&lv_font_montserrat_12,LV_PART_MAIN);
    lv_obj_set_style_text_color(caption,lv_color_hex(0x777B7E),LV_PART_MAIN);
    *value_label=lv_label_create(button);
    lv_label_set_text(*value_label,"--  >");
    lv_obj_set_pos(*value_label,7,18);
    lv_obj_set_width(*value_label,138);
    lv_label_set_long_mode(*value_label,LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(*value_label,&lv_font_montserrat_12,LV_PART_MAIN);
    lv_obj_set_style_text_color(*value_label,lv_color_hex(accent),LV_PART_MAIN);
    lv_obj_add_event_cb(button,event_cb,LV_EVENT_CLICKED,NULL);
    return button;
}

static void create_ui(void) {
    lv_obj_t *screen=lv_screen_active();
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x0E1012),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen,0,LV_PART_MAIN);
    lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);

    main_page=create_page(screen);

    lv_obj_t *logo=lv_label_create(main_page);
    lv_label_set_text(logo,"A");
    lv_obj_set_pos(logo,2,-4);
    lv_obj_set_style_text_font(logo,&lv_font_montserrat_28,LV_PART_MAIN);
    lv_obj_set_style_text_color(logo,lv_color_hex(0xFFFFFF),LV_PART_MAIN);

    lv_obj_t *title=lv_label_create(main_page);
    lv_label_set_text(title,"EV ENGINE");
    lv_obj_set_pos(title,21,1);
    lv_obj_set_style_text_font(title,&lv_font_montserrat_12,LV_PART_MAIN);
    lv_obj_set_style_text_color(title,lv_color_hex(0xFFFFFF),LV_PART_MAIN);
    lv_obj_t *subtitle=lv_label_create(main_page);
    lv_label_set_text(subtitle,"SIMULATOR");
    lv_obj_set_pos(subtitle,21,13);
    lv_obj_set_style_text_font(subtitle,&lv_font_montserrat_12,LV_PART_MAIN);
    lv_obj_set_style_text_color(subtitle,lv_color_hex(0x777B7E),LV_PART_MAIN);

    pulse_led=lv_obj_create(main_page);
    lv_obj_set_pos(pulse_led,91,10);
    lv_obj_set_size(pulse_led,7,7);
    lv_obj_set_style_radius(pulse_led,LV_RADIUS_CIRCLE,LV_PART_MAIN);
    lv_obj_set_style_border_width(pulse_led,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(pulse_led,0,LV_PART_MAIN);
    lv_obj_set_style_bg_color(pulse_led,lv_color_hex(0xEE4445),LV_PART_MAIN);

    rpm_label=lv_label_create(main_page);
    lv_label_set_text(rpm_label,"0 RPM");
    lv_obj_set_pos(rpm_label,102,5);
    lv_obj_set_width(rpm_label,68);
    lv_label_set_long_mode(rpm_label,LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(rpm_label,&lv_font_montserrat_12,LV_PART_MAIN);
    lv_obj_set_style_text_color(rpm_label,lv_color_hex(0xFFFFFF),LV_PART_MAIN);

    state_label=lv_label_create(main_page);
    lv_label_set_text(state_label,"OFF");
    lv_obj_set_pos(state_label,174,5);
    lv_obj_set_width(state_label,62);
    lv_label_set_long_mode(state_label,LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(state_label,&lv_font_montserrat_12,LV_PART_MAIN);
    lv_obj_set_style_text_color(state_label,lv_color_hex(0x777B7E),LV_PART_MAIN);

    lv_obj_t *settings_hint=lv_label_create(main_page);
    lv_label_set_text(settings_hint,"SETTINGS\nPULL DOWN V");
    lv_obj_set_pos(settings_hint,240,1);
    lv_obj_set_width(settings_hint,73);
    lv_obj_set_style_text_align(settings_hint,LV_TEXT_ALIGN_RIGHT,LV_PART_MAIN);
    lv_obj_set_style_text_font(settings_hint,&lv_font_montserrat_12,LV_PART_MAIN);
    lv_obj_set_style_text_color(settings_hint,lv_color_hex(0x777B7E),LV_PART_MAIN);

    engine_visual=lv_canvas_create(main_page);
    lv_canvas_set_buffer(engine_visual,engine_pixels,RIG_W,RIG_H,LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(engine_visual,0,28);
    lv_obj_set_size(engine_visual,RIG_W,RIG_H);
    lv_obj_set_style_bg_color(engine_visual,lv_color_hex(0x0E1012),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(engine_visual,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_width(engine_visual,0,LV_PART_MAIN);
    lv_obj_set_style_radius(engine_visual,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(engine_visual,0,LV_PART_MAIN);
    lv_obj_clear_flag(engine_visual,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);

    create_cycle_selector(main_page,5,"ENGINE · TAP NEXT",0x77CEE0,
                          &engine_value_label,profile_next_event);
    create_cycle_selector(main_page,162,"EXHAUST · TAP NEXT",0xF4802A,
                          &exhaust_value_label,exhaust_next_event);

    rev_button=lv_button_create(main_page);
    lv_obj_set_pos(rev_button,5,184);
    lv_obj_set_size(rev_button,310,47);
    style_button(rev_button,0x0E1012,0);
    lv_obj_set_style_border_color(rev_button,lv_color_hex(0x777B7E),LV_PART_MAIN);
    lv_obj_t *rev_caption=lv_label_create(rev_button);
    lv_label_set_text(rev_caption,"THROTTLE INPUT");
    lv_obj_set_pos(rev_caption,8,1);
    lv_obj_set_style_text_font(rev_caption,&lv_font_montserrat_12,LV_PART_MAIN);
    lv_obj_set_style_text_color(rev_caption,lv_color_hex(0x777B7E),LV_PART_MAIN);
    rev_label=lv_label_create(rev_button);
    lv_label_set_text(rev_label,"PRESS + HOLD");
    lv_obj_set_pos(rev_label,8,14);
    lv_obj_set_style_text_font(rev_label,&lv_font_montserrat_12,LV_PART_MAIN);
    lv_obj_set_style_text_color(rev_label,lv_color_hex(0xFFFFFF),LV_PART_MAIN);
    rev_detail_label=lv_label_create(rev_button);
    lv_label_set_text(rev_detail_label,"STARTS ENGINE AUTOMATICALLY");
    lv_obj_set_pos(rev_detail_label,8,28);
    lv_obj_set_style_text_font(rev_detail_label,&lv_font_montserrat_12,LV_PART_MAIN);
    lv_obj_set_style_text_color(rev_detail_label,lv_color_hex(0x777B7E),LV_PART_MAIN);
    countdown_bar=lv_bar_create(rev_button);
    lv_obj_set_pos(countdown_bar,8,41);
    lv_obj_set_size(countdown_bar,143,3);
    lv_bar_set_range(countdown_bar,0,100);
    lv_bar_set_value(countdown_bar,0,LV_ANIM_OFF);
    lv_obj_set_style_bg_color(countdown_bar,lv_color_hex(0x303336),LV_PART_MAIN);
    lv_obj_set_style_bg_color(countdown_bar,lv_color_hex(0xF4802A),LV_PART_INDICATOR);
    lv_obj_t *stem=lv_obj_create(rev_button);
    lv_obj_set_pos(stem,170,22); lv_obj_set_size(stem,25,4);
    lv_obj_set_style_bg_color(stem,lv_color_hex(0xD8DADC),LV_PART_MAIN);
    lv_obj_set_style_border_width(stem,0,LV_PART_MAIN); lv_obj_set_style_radius(stem,0,LV_PART_MAIN);
    lv_obj_clear_flag(stem,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *grip=lv_obj_create(rev_button);
    lv_obj_set_pos(grip,190,11); lv_obj_set_size(grip,100,22);
    lv_obj_set_style_bg_color(grip,lv_color_hex(0x25292C),LV_PART_MAIN);
    lv_obj_set_style_border_color(grip,lv_color_hex(0x4D5154),LV_PART_MAIN);
    lv_obj_set_style_border_width(grip,1,LV_PART_MAIN); lv_obj_set_style_radius(grip,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(grip,0,LV_PART_MAIN);
    lv_obj_clear_flag(grip,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    for(int x=6;x<94;x+=8) {
        lv_obj_t *groove=lv_obj_create(grip);
        lv_obj_set_pos(groove,x,1); lv_obj_set_size(groove,3,18);
        lv_obj_set_style_bg_color(groove,lv_color_hex(0x0E1012),LV_PART_MAIN);
        lv_obj_set_style_border_width(groove,0,LV_PART_MAIN);
        lv_obj_set_style_radius(groove,0,LV_PART_MAIN);
        lv_obj_clear_flag(groove,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t *grip_end=lv_obj_create(rev_button);
    lv_obj_set_pos(grip_end,290,10); lv_obj_set_size(grip_end,12,24);
    lv_obj_set_style_bg_color(grip_end,lv_color_hex(0xF4802A),LV_PART_MAIN);
    lv_obj_set_style_border_width(grip_end,0,LV_PART_MAIN); lv_obj_set_style_radius(grip_end,0,LV_PART_MAIN);
    lv_obj_clear_flag(grip_end,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(rev_button,rev_event,LV_EVENT_ALL,NULL);

    drawer_viewport=lv_obj_create(main_page);
    lv_obj_set_pos(drawer_viewport,0,28);
    lv_obj_set_size(drawer_viewport,320,DRAWER_HEIGHT);
    lv_obj_set_style_bg_opa(drawer_viewport,LV_OPA_TRANSP,LV_PART_MAIN);
    lv_obj_set_style_border_width(drawer_viewport,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(drawer_viewport,0,LV_PART_MAIN);
    lv_obj_set_style_radius(drawer_viewport,0,LV_PART_MAIN);
    lv_obj_clear_flag(drawer_viewport,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);

    drawer_panel=lv_obj_create(drawer_viewport);
    lv_obj_set_pos(drawer_panel,0,DRAWER_CLOSED_Y);
    lv_obj_set_size(drawer_panel,320,DRAWER_HEIGHT);
    lv_obj_set_style_bg_color(drawer_panel,lv_color_hex(0x121517),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(drawer_panel,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_color(drawer_panel,lv_color_hex(0x777B7E),LV_PART_MAIN);
    lv_obj_set_style_border_width(drawer_panel,1,LV_PART_MAIN);
    lv_obj_set_style_radius(drawer_panel,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(drawer_panel,0,LV_PART_MAIN);
    lv_obj_clear_flag(drawer_panel,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *drawer_title=lv_label_create(drawer_panel);
    lv_label_set_text(drawer_title,"QUICK SETTINGS");
    lv_obj_set_pos(drawer_title,9,3);
    lv_obj_set_style_text_font(drawer_title,&lv_font_montserrat_12,LV_PART_MAIN);
    lv_obj_set_style_text_color(drawer_title,lv_color_hex(0xFFFFFF),LV_PART_MAIN);
    lv_obj_t *drawer_close=lv_label_create(drawer_panel);
    lv_label_set_text(drawer_close,"DRAG UP TO CLOSE");
    lv_obj_set_pos(drawer_close,205,3);
    lv_obj_set_style_text_font(drawer_close,&lv_font_montserrat_12,LV_PART_MAIN);
    lv_obj_set_style_text_color(drawer_close,lv_color_hex(0x777B7E),LV_PART_MAIN);

    const char *row_titles[]={"VOLUME","REDLINE","AUTO OFF"};
    const int row_y[]={27,54,81};
    for(unsigned i=0;i<3;i++) {
        lv_obj_t *row=lv_label_create(drawer_panel);
        lv_label_set_text(row,row_titles[i]);
        lv_obj_set_pos(row,9,row_y[i]);
        lv_obj_set_style_text_font(row,&lv_font_montserrat_12,LV_PART_MAIN);
        lv_obj_set_style_text_color(row,lv_color_hex(0xFFFFFF),LV_PART_MAIN);
    }
    volume_slider=lv_slider_create(drawer_panel);
    redline_slider=lv_slider_create(drawer_panel);
    auto_off_slider=lv_slider_create(drawer_panel);
    lv_obj_t *sliders[]={volume_slider,redline_slider,auto_off_slider};
    const uint32_t slider_colors[]={0x77CEE0,0xF4802A,0xF394BE};
    for(unsigned i=0;i<3;i++) {
        lv_obj_set_pos(sliders[i],91,row_y[i]+1);
        lv_obj_set_size(sliders[i],172,12);
        lv_obj_set_style_bg_color(sliders[i],lv_color_hex(0x303336),LV_PART_MAIN);
        lv_obj_set_style_bg_color(sliders[i],lv_color_hex(slider_colors[i]),LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(sliders[i],lv_color_hex(0xFDBD2E),LV_PART_KNOB);
        lv_obj_set_style_pad_all(sliders[i],3,LV_PART_KNOB);
    }
    lv_slider_set_range(volume_slider,0,100);
    lv_slider_set_value(volume_slider,60,LV_ANIM_OFF);
    lv_slider_set_range(redline_slider,1800,EV_MAX_RPM);
    lv_slider_set_value(redline_slider,8000,LV_ANIM_OFF);
    lv_slider_set_range(auto_off_slider,10,80);
    lv_slider_set_value(auto_off_slider,30,LV_ANIM_OFF);
    lv_obj_add_event_cb(volume_slider,volume_event,LV_EVENT_ALL,NULL);
    lv_obj_add_event_cb(redline_slider,redline_event,LV_EVENT_ALL,NULL);
    lv_obj_add_event_cb(auto_off_slider,auto_off_event,LV_EVENT_ALL,NULL);
    volume_value_label=lv_label_create(drawer_panel);
    redline_value_label=lv_label_create(drawer_panel);
    auto_off_value_label=lv_label_create(drawer_panel);
    lv_obj_t *values[]={volume_value_label,redline_value_label,auto_off_value_label};
    const char *defaults[]={"60%","8000","3.0 s"};
    for(unsigned i=0;i<3;i++) {
        lv_label_set_text(values[i],defaults[i]);
        lv_obj_set_pos(values[i],268,row_y[i]);
        lv_obj_set_width(values[i],43);
        lv_obj_set_style_text_align(values[i],LV_TEXT_ALIGN_RIGHT,LV_PART_MAIN);
        lv_obj_set_style_text_font(values[i],&lv_font_montserrat_12,LV_PART_MAIN);
        lv_obj_set_style_text_color(values[i],lv_color_hex(slider_colors[i]),LV_PART_MAIN);
    }
    drawer_grab=lv_obj_create(drawer_panel);
    lv_obj_set_pos(drawer_grab,120,98); lv_obj_set_size(drawer_grab,80,14);
    lv_obj_set_style_bg_opa(drawer_grab,LV_OPA_TRANSP,LV_PART_MAIN);
    lv_obj_set_style_border_width(drawer_grab,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(drawer_grab,0,LV_PART_MAIN);
    lv_obj_clear_flag(drawer_grab,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(drawer_grab,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *grab_line=lv_obj_create(drawer_grab);
    lv_obj_set_pos(grab_line,20,5); lv_obj_set_size(grab_line,40,2);
    lv_obj_set_style_bg_color(grab_line,lv_color_hex(0x777B7E),LV_PART_MAIN);
    lv_obj_set_style_border_width(grab_line,0,LV_PART_MAIN);
    lv_obj_set_style_radius(grab_line,0,LV_PART_MAIN);
    lv_obj_clear_flag(grab_line,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(drawer_grab,drawer_drag_event,LV_EVENT_ALL,NULL);

    header_drag=lv_obj_create(main_page);
    lv_obj_set_pos(header_drag,0,0); lv_obj_set_size(header_drag,320,28);
    lv_obj_set_style_bg_opa(header_drag,LV_OPA_TRANSP,LV_PART_MAIN);
    lv_obj_set_style_border_width(header_drag,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(header_drag,0,LV_PART_MAIN);
    lv_obj_clear_flag(header_drag,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(header_drag,LV_OBJ_FLAG_CLICKABLE);
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
    ESP_LOGI(TAG,"UI_READY panel=ST7789 320x240 touch=FT6336 lvgl_stack=10240 animation=powertrain_rig layout=dual_panel_v2 refresh_ms=16 motion_ms=33 selector=cycle_buttons menu=pull_down");
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
    ESP_LOGI(TAG,"UI_READBACK ready=1 stack_lvgl=%u animation=powertrain_rig layout=dual_panel_v2 profile=%s exhaust=%s render_count=%u render_max_us=%u update_max_us=%u draw_passes=%u selector=cycle_buttons menu=pull_down drawer_open=%d auto_off_ms=%u",
             board_ui_stack_high_water_mark(),ev_profiles[profile].name,ev_exhausts[exhaust].name,
             renders,render_us,refresh_us,draws,drawer_open,auto_off_ms);
    return ESP_OK;
}
