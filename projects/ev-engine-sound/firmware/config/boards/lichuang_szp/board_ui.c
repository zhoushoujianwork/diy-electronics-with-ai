#include "board_ui.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_audio.h"
#include "board_config.h"
#include "engine_voice.h"
#include "engine_canvas.h"
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

static esp_lcd_panel_io_handle_t panel_io;
static esp_lcd_panel_handle_t panel;
static esp_lcd_touch_handle_t touch;
static lv_display_t *display;
static lv_indev_t *touch_indev;
static lv_obj_t *main_page,*settings_page;
static lv_obj_t *profile_strip,*profile_cards[EV_PROFILES],*engine_visual;
static lv_obj_t *exhaust_strip,*exhaust_cards[EV_EXHAUSTS];
static lv_obj_t *rpm_label,*state_label,*pulse_led,*start_button,*start_label;
static lv_obj_t *redline_slider,*redline_value_label,*rev_button,*volume_value_label;
static board_ui_action_cb_t action_callback;
static board_ui_state_cb_t state_callback;
static void *callback_context;
static uint64_t last_firings;
static bool last_pulse;
static unsigned displayed_profile=EV_PROFILES;
static unsigned displayed_exhaust=EV_EXHAUSTS;
static float crank_phase;
static bool displayed_running;
static bool redline_dragging;
static bool ready;
static bool carousel_scrolling,carousel_syncing;
static bool exhaust_scrolling,exhaust_syncing,settings_visible;
static uint32_t draw_count,refresh_max_us,last_refresh_tick;
static uint32_t render_count,render_max_us;
static int64_t render_started;
static lv_obj_t *phase_label,*load_bar,*rev_label;
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
    lv_obj_set_style_border_color(button,lv_color_hex(0x354052),LV_PART_MAIN);
    lv_obj_set_style_radius(button,radius,LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(button,0,LV_PART_MAIN);
}

static lv_obj_t *button_with_label(lv_obj_t *parent,const char *text,
                                   int x,int y,int width,int height) {
    lv_obj_t *button=lv_button_create(parent);
    lv_obj_set_pos(button,x,y);
    lv_obj_set_size(button,width,height);
    style_button(button,0x151C27,8);
    lv_obj_t *label=lv_label_create(button);
    lv_label_set_text(label,text);
    lv_obj_set_style_text_color(label,lv_color_hex(0xC7D0DC),LV_PART_MAIN);
    lv_obj_set_style_text_font(label,&lv_font_montserrat_14,LV_PART_MAIN);
    lv_obj_center(label);
    return button;
}

_Alignas(4) static uint16_t engine_pixels[320*86];
static void draw_engine_frame(void) {
    ev_canvas_render(engine_pixels,320,86,displayed_profile,crank_phase/(2*TWO_PI),displayed_running);
    draw_count++;
    lv_obj_invalidate(engine_visual);
}

static void style_profile_cards(unsigned selected) {
    for(unsigned i=0;i<EV_PROFILES;i++) {
        bool active=i==selected;
        lv_obj_set_style_bg_opa(profile_cards[i],active?LV_OPA_COVER:LV_OPA_TRANSP,LV_PART_MAIN);
        lv_obj_set_style_bg_color(profile_cards[i],lv_color_hex(active?0x10231F:0x0B1017),LV_PART_MAIN);
        lv_obj_set_style_border_width(profile_cards[i],active?1:0,LV_PART_MAIN);
        lv_obj_set_style_border_color(profile_cards[i],lv_color_hex(0x2B6455),LV_PART_MAIN);
        lv_obj_set_style_text_color(profile_cards[i],lv_color_hex(active?0x2DE2A6:0x718092),LV_PART_MAIN);
    }
}

static void sync_profile_carousel(unsigned profile) {
    if(profile>=EV_PROFILES) profile=0;
    displayed_profile=profile;
    style_profile_cards(profile);
    carousel_syncing=true;
    lv_obj_scroll_to_view(profile_cards[profile],LV_ANIM_OFF);
    carousel_syncing=false;
    draw_engine_frame();
    ESP_LOGI(TAG,"UI_SYNC mode=swipe cylinders=%u layout=%s profile=%s",
             ev_profiles[profile].cylinders,ev_profiles[profile].ui_name,
             ev_profiles[profile].name);
}

static void select_profile(unsigned profile,const char *source) {
    if(profile>=EV_PROFILES || profile==displayed_profile) return;
    displayed_profile=profile;
    style_profile_cards(profile);
    ESP_LOGI(TAG,"TOUCH action=profile_%s value=%s cylinders=%u layout=%s",
             source,ev_profiles[profile].name,ev_profiles[profile].cylinders,
             ev_profiles[profile].ui_name);
    if(action_callback) action_callback(BOARD_UI_PROFILE,(int)profile,callback_context);
    lv_obj_invalidate(engine_visual);
}

static void profile_card_event(lv_event_t *event) {
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED) return;
    unsigned profile=(unsigned)(uintptr_t)lv_event_get_user_data(event);
    if(profile>=EV_PROFILES) return;
    lv_obj_scroll_to_view(profile_cards[profile],LV_ANIM_ON);
    select_profile(profile,"tap");
}

static void profile_strip_event(lv_event_t *event) {
    if(lv_event_get_code(event)==LV_EVENT_SCROLL_BEGIN) {
        carousel_scrolling=true;
        lv_anim_t *animation=lv_event_get_param(event);
        if(animation) {
            lv_anim_set_duration(animation,240);
            lv_anim_set_path_cb(animation,lv_anim_path_ease_out);
        }
        return;
    }
    if(lv_event_get_code(event)!=LV_EVENT_SCROLL_END) return;
    carousel_scrolling=false;
    if(carousel_syncing) return;
    lv_area_t strip_area;
    lv_obj_get_coords(profile_strip,&strip_area);
    int center=(strip_area.x1+strip_area.x2)/2;
    unsigned best=displayed_profile<EV_PROFILES?displayed_profile:0;
    int best_distance=INT32_MAX;
    for(unsigned i=0;i<EV_PROFILES;i++) {
        lv_area_t card_area;
        lv_obj_get_coords(profile_cards[i],&card_area);
        int card_center=(card_area.x1+card_area.x2)/2;
        int distance=abs(card_center-center);
        if(distance<best_distance) { best_distance=distance; best=i; }
    }
    select_profile(best,"swipe");
    /* Also rebuild a stopped engine after an animated card tap: the state
     * snapshot may already have consumed the profile change during scrolling. */
    draw_engine_frame();
}

static void style_exhaust_cards(unsigned selected) {
    for(unsigned i=0;i<EV_EXHAUSTS;i++) {
        bool active=i==selected;
        lv_obj_set_style_bg_opa(exhaust_cards[i],active?LV_OPA_COVER:LV_OPA_TRANSP,LV_PART_MAIN);
        lv_obj_set_style_bg_color(exhaust_cards[i],lv_color_hex(active?0x231B12:0x0B1017),LV_PART_MAIN);
        lv_obj_set_style_border_width(exhaust_cards[i],active?1:0,LV_PART_MAIN);
        lv_obj_set_style_border_color(exhaust_cards[i],lv_color_hex(0x9A6838),LV_PART_MAIN);
        lv_obj_set_style_text_color(exhaust_cards[i],lv_color_hex(active?0xFFB86A:0x718092),LV_PART_MAIN);
    }
}

static void sync_exhaust_carousel(unsigned exhaust) {
    if(exhaust>=EV_EXHAUSTS) exhaust=0;
    displayed_exhaust=exhaust;
    style_exhaust_cards(exhaust);
    exhaust_syncing=true;
    lv_obj_scroll_to_view(exhaust_cards[exhaust],LV_ANIM_OFF);
    exhaust_syncing=false;
    ESP_LOGI(TAG,"UI_SYNC mode=exhaust_swipe exhaust=%s",ev_exhausts[exhaust].name);
}

static void select_exhaust(unsigned exhaust,const char *source) {
    if(exhaust>=EV_EXHAUSTS || exhaust==displayed_exhaust) return;
    displayed_exhaust=exhaust;
    style_exhaust_cards(exhaust);
    ESP_LOGI(TAG,"TOUCH action=exhaust_%s value=%s",source,ev_exhausts[exhaust].name);
    if(action_callback) action_callback(BOARD_UI_EXHAUST,(int)exhaust,callback_context);
}

static void exhaust_card_event(lv_event_t *event) {
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED) return;
    unsigned exhaust=(unsigned)(uintptr_t)lv_event_get_user_data(event);
    if(exhaust>=EV_EXHAUSTS) return;
    lv_obj_scroll_to_view(exhaust_cards[exhaust],LV_ANIM_ON);
    select_exhaust(exhaust,"tap");
}

static void exhaust_strip_event(lv_event_t *event) {
    if(lv_event_get_code(event)==LV_EVENT_SCROLL_BEGIN) {
        exhaust_scrolling=true;
        lv_anim_t *animation=lv_event_get_param(event);
        if(animation) {
            lv_anim_set_duration(animation,220);
            lv_anim_set_path_cb(animation,lv_anim_path_ease_out);
        }
        return;
    }
    if(lv_event_get_code(event)!=LV_EVENT_SCROLL_END) return;
    exhaust_scrolling=false;
    if(exhaust_syncing) return;
    lv_area_t strip_area;
    lv_obj_get_coords(exhaust_strip,&strip_area);
    int center=(strip_area.x1+strip_area.x2)/2;
    unsigned best=displayed_exhaust<EV_EXHAUSTS?displayed_exhaust:0;
    int best_distance=INT32_MAX;
    for(unsigned i=0;i<EV_EXHAUSTS;i++) {
        lv_area_t card_area;
        lv_obj_get_coords(exhaust_cards[i],&card_area);
        int card_center=(card_area.x1+card_area.x2)/2;
        int distance=abs(card_center-center);
        if(distance<best_distance) { best_distance=distance; best=i; }
    }
    select_exhaust(best,"swipe");
}

static void engine_event(lv_event_t *event) {
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED) return;
    ESP_LOGI(TAG,"TOUCH action=engine_toggle");
    if(action_callback) action_callback(BOARD_UI_ENGINE_TOGGLE,0,callback_context);
}

static void volume_event(lv_event_t *event) {
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED) return;
    int delta=(int)(intptr_t)lv_event_get_user_data(event);
    ESP_LOGI(TAG,"TOUCH action=volume_delta value=%d",delta);
    if(action_callback) action_callback(BOARD_UI_VOLUME_DELTA,delta,callback_context);
}

static void redline_event(lv_event_t *event) {
    lv_event_code_t code=lv_event_get_code(event);
    if(code==LV_EVENT_PRESSED) redline_dragging=true;
    if(code==LV_EVENT_VALUE_CHANGED) {
        int value=lv_slider_get_value(redline_slider);
        lv_label_set_text_fmt(redline_value_label,"REDLINE  %d",value);
    }
    if(code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST) {
        redline_dragging=false;
        int value=lv_slider_get_value(redline_slider);
        ESP_LOGI(TAG,"TOUCH action=redline_slide value=%d",value);
        if(action_callback) action_callback(BOARD_UI_REDLINE_SET,value,callback_context);
    }
}

static void page_event(lv_event_t *event) {
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED) return;
    bool show_settings=(bool)(uintptr_t)lv_event_get_user_data(event);
    settings_visible=show_settings;
    ESP_LOGI(TAG,"TOUCH action=page value=%s",show_settings?"settings":"main");
    if(show_settings) {
        lv_obj_add_flag(main_page,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(settings_page,LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(settings_page,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(main_page,LV_OBJ_FLAG_HIDDEN);
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
    displayed_running=state.running;

    if(state.profile!=displayed_profile && !carousel_scrolling) sync_profile_carousel(state.profile);
    if(state.exhaust!=displayed_exhaust && !exhaust_scrolling) sync_exhaust_carousel(state.exhaust);
    /* Static text/style updates only on change; UI input keeps its own 16 ms timer. */
    int rpm=(int)lroundf(state.rpm/10)*10;
    int previous_rpm=(int)lroundf(previous_state.rpm/10)*10;
    if(changed || rpm!=previous_rpm) lv_label_set_text_fmt(rpm_label,"%d RPM",rpm);
    if(changed || state.running!=previous_state.running || state.fault!=previous_state.fault) {
        lv_label_set_text(start_label,state.running?"STOP":"START");
        lv_label_set_text(state_label,state.fault?"ERR":state.running?"RUN":"OFF");
        lv_obj_set_style_bg_color(start_button,lv_color_hex(state.running?0x5A2031:0x17483E),LV_PART_MAIN);
    }
    if(changed || state.phase!=previous_state.phase) {
        lv_label_set_text_fmt(phase_label,"%s / SLOW",ev_phase_name(state.phase));
        lv_obj_set_style_text_color(phase_label,lv_color_hex(state.phase==EV_PHASE_COAST?0x70BFDA:0xD5AE80),LV_PART_MAIN);
    }
    bool pressed=state.throttle>.02f;
    if(changed || pressed!=(previous_state.throttle>.02f)) {
        lv_label_set_text(rev_label,pressed?"RELEASE\nTO COAST":"HOLD\nTHROTTLE");
        lv_obj_set_style_bg_color(rev_button,lv_color_hex(pressed?0xA15D27:0x3E2B20),LV_PART_MAIN);
    }
    int load=(int)(state.load*100);
    if(changed || load!=(int)(previous_state.load*100)) lv_bar_set_value(load_bar,load,LV_ANIM_OFF);
    if(changed || state.volume!=previous_state.volume)
        lv_label_set_text_fmt(volume_value_label,"%d%%",(int)lroundf(state.volume*100));
    unsigned minimum=(unsigned)ev_profiles[state.profile].idle_rpm+500;
    if(changed || state.profile!=previous_state.profile)
        lv_slider_set_range(redline_slider,(int32_t)minimum,EV_MAX_RPM);
    if(!redline_dragging && (changed || state.redline_rpm!=previous_state.redline_rpm)) {
        lv_slider_set_value(redline_slider,(int32_t)state.redline_rpm,LV_ANIM_OFF);
        lv_label_set_text_fmt(redline_value_label,"REDLINE  %u",state.redline_rpm);
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
    /* During a swipe, give the small carousel region the entire redraw budget.
     * The mechanism resumes at the current phase when it settles. */
    if(!settings_visible && !carousel_scrolling && !exhaust_scrolling &&
       (moving || changed || state.profile!=previous_state.profile || state.running!=previous_state.running))
        draw_engine_frame();
    previous_state=state; have_previous_state=true;
    uint32_t elapsed=(uint32_t)(esp_timer_get_time()-begin);
    if(elapsed>refresh_max_us) refresh_max_us=elapsed;
}

static lv_obj_t *create_page(lv_obj_t *screen) {
    lv_obj_t *page=lv_obj_create(screen);
    lv_obj_set_pos(page,0,0);
    lv_obj_set_size(page,320,240);
    lv_obj_set_style_bg_color(page,lv_color_hex(0x080B10),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(page,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_width(page,0,LV_PART_MAIN);
    lv_obj_set_style_radius(page,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(page,0,LV_PART_MAIN);
    lv_obj_clear_flag(page,LV_OBJ_FLAG_SCROLLABLE);
    return page;
}

static void create_exhaust_mark(lv_obj_t *card,unsigned exhaust) {
    static const uint32_t body_colors[EV_EXHAUSTS]={0x78828D,0x3A3E43,0x9C7135,0xA9AFB5,0x6D747B};
    static const uint32_t accent_colors[EV_EXHAUSTS]={0xAAB2BA,0xD64535,0xD9AA4E,0x6C737A,0xE27B35};
    static const int widths[EV_EXHAUSTS]={22,24,21,18,27};
    static const int heights[EV_EXHAUSTS]={8,7,9,13,4};
    lv_obj_t *body=lv_obj_create(card);
    lv_obj_set_pos(body,5,14-heights[exhaust]/2);
    lv_obj_set_size(body,widths[exhaust],heights[exhaust]);
    lv_obj_set_style_bg_color(body,lv_color_hex(body_colors[exhaust]),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(body,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_width(body,exhaust==3?2:0,LV_PART_MAIN);
    lv_obj_set_style_border_color(body,lv_color_hex(0xD5D9DC),LV_PART_MAIN);
    lv_obj_set_style_radius(body,exhaust==3?2:LV_RADIUS_CIRCLE,LV_PART_MAIN);
    lv_obj_set_style_pad_all(body,0,LV_PART_MAIN);
    lv_obj_clear_flag(body,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *tip=lv_obj_create(card);
    lv_obj_set_pos(tip,5+widths[exhaust]-3,14-heights[exhaust]/2);
    lv_obj_set_size(tip,6,heights[exhaust]);
    lv_obj_set_style_bg_color(tip,lv_color_hex(accent_colors[exhaust]),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tip,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_width(tip,0,LV_PART_MAIN);
    lv_obj_set_style_radius(tip,LV_RADIUS_CIRCLE,LV_PART_MAIN);
    lv_obj_set_style_pad_all(tip,0,LV_PART_MAIN);
    lv_obj_clear_flag(tip,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
}

static void create_ui(void) {
    lv_obj_t *screen=lv_screen_active();
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x080B10),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen,0,LV_PART_MAIN);
    lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);

    main_page=create_page(screen);
    settings_page=create_page(screen);
    lv_obj_add_flag(settings_page,LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title=lv_label_create(main_page);
    lv_label_set_text(title,"EV SOUND");
    lv_obj_set_pos(title,8,6);
    lv_obj_set_style_text_color(title,lv_color_hex(0xEAF2F8),LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(title,1,LV_PART_MAIN);

    pulse_led=lv_obj_create(main_page);
    lv_obj_set_pos(pulse_led,77,9);
    lv_obj_set_size(pulse_led,7,7);
    lv_obj_set_style_radius(pulse_led,LV_RADIUS_CIRCLE,LV_PART_MAIN);
    lv_obj_set_style_border_width(pulse_led,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(pulse_led,0,LV_PART_MAIN);
    lv_obj_set_style_bg_color(pulse_led,lv_color_hex(0xFFBD6A),LV_PART_MAIN);

    state_label=lv_label_create(main_page);
    lv_label_set_text(state_label,"READY");
    lv_obj_set_pos(state_label,89,6);
    lv_obj_set_style_text_color(state_label,lv_color_hex(0x79879A),LV_PART_MAIN);

    rpm_label=lv_label_create(main_page);
    lv_label_set_text(rpm_label,"0000 RPM");
    lv_obj_set_pos(rpm_label,127,6);
    lv_obj_set_width(rpm_label,87);
    lv_label_set_long_mode(rpm_label,LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(rpm_label,LV_TEXT_ALIGN_RIGHT,LV_PART_MAIN);
    lv_obj_set_style_text_color(rpm_label,lv_color_hex(0x2DE2A6),LV_PART_MAIN);

    start_button=button_with_label(main_page,"START",219,3,45,23);
    start_label=lv_obj_get_child(start_button,0);
    lv_obj_add_event_cb(start_button,engine_event,LV_EVENT_CLICKED,NULL);
    lv_obj_t *settings_button=button_with_label(main_page,"SET",269,3,43,23);
    lv_obj_add_event_cb(settings_button,page_event,LV_EVENT_CLICKED,(void *)(uintptr_t)true);

    profile_strip=lv_obj_create(main_page);
    lv_obj_set_pos(profile_strip,0,28);
    lv_obj_set_size(profile_strip,320,32);
    lv_obj_set_style_bg_color(profile_strip,lv_color_hex(0x0B1017),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(profile_strip,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_width(profile_strip,1,LV_PART_MAIN);
    lv_obj_set_style_border_side(profile_strip,LV_BORDER_SIDE_TOP|LV_BORDER_SIDE_BOTTOM,LV_PART_MAIN);
    lv_obj_set_style_border_color(profile_strip,lv_color_hex(0x263141),LV_PART_MAIN);
    lv_obj_set_style_radius(profile_strip,0,LV_PART_MAIN);
    lv_obj_set_style_pad_left(profile_strip,104,LV_PART_MAIN);
    lv_obj_set_style_pad_right(profile_strip,104,LV_PART_MAIN);
    lv_obj_set_style_pad_top(profile_strip,2,LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(profile_strip,2,LV_PART_MAIN);
    lv_obj_set_style_pad_column(profile_strip,6,LV_PART_MAIN);
    lv_obj_set_flex_flow(profile_strip,LV_FLEX_FLOW_ROW);
    lv_obj_set_scroll_dir(profile_strip,LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(profile_strip,LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(profile_strip,LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(profile_strip,LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_flag(profile_strip,LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_event_cb(profile_strip,profile_strip_event,LV_EVENT_ALL,NULL);
    for(unsigned i=0;i<EV_PROFILES;i++) {
        profile_cards[i]=lv_button_create(profile_strip);
        lv_obj_set_size(profile_cards[i],112,26);
        lv_obj_clear_flag(profile_cards[i],LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(profile_cards[i],LV_OBJ_FLAG_SNAPPABLE);
        lv_obj_set_style_radius(profile_cards[i],7,LV_PART_MAIN);
        lv_obj_set_style_pad_all(profile_cards[i],0,LV_PART_MAIN);
        lv_obj_set_style_shadow_width(profile_cards[i],0,LV_PART_MAIN);
        lv_obj_t *label=lv_label_create(profile_cards[i]);
        lv_label_set_text_fmt(label,"%uC  %s",ev_profiles[i].cylinders,ev_profiles[i].ui_name);
        lv_obj_set_width(label,108);
        lv_obj_set_style_text_font(label,&lv_font_montserrat_12,LV_PART_MAIN);
        lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,LV_PART_MAIN);
        lv_obj_center(label);
        lv_obj_add_event_cb(profile_cards[i],profile_card_event,LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);
    }

    exhaust_strip=lv_obj_create(main_page);
    lv_obj_set_pos(exhaust_strip,0,60);
    lv_obj_set_size(exhaust_strip,320,34);
    lv_obj_set_style_bg_color(exhaust_strip,lv_color_hex(0x0D1117),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(exhaust_strip,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_width(exhaust_strip,1,LV_PART_MAIN);
    lv_obj_set_style_border_side(exhaust_strip,LV_BORDER_SIDE_BOTTOM,LV_PART_MAIN);
    lv_obj_set_style_border_color(exhaust_strip,lv_color_hex(0x3D3023),LV_PART_MAIN);
    lv_obj_set_style_radius(exhaust_strip,0,LV_PART_MAIN);
    lv_obj_set_style_pad_left(exhaust_strip,101,LV_PART_MAIN);
    lv_obj_set_style_pad_right(exhaust_strip,101,LV_PART_MAIN);
    lv_obj_set_style_pad_top(exhaust_strip,2,LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(exhaust_strip,2,LV_PART_MAIN);
    lv_obj_set_style_pad_column(exhaust_strip,6,LV_PART_MAIN);
    lv_obj_set_flex_flow(exhaust_strip,LV_FLEX_FLOW_ROW);
    lv_obj_set_scroll_dir(exhaust_strip,LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(exhaust_strip,LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(exhaust_strip,LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(exhaust_strip,LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_flag(exhaust_strip,LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_event_cb(exhaust_strip,exhaust_strip_event,LV_EVENT_ALL,NULL);
    for(unsigned i=0;i<EV_EXHAUSTS;i++) {
        exhaust_cards[i]=lv_button_create(exhaust_strip);
        lv_obj_set_size(exhaust_cards[i],118,28);
        lv_obj_clear_flag(exhaust_cards[i],LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(exhaust_cards[i],LV_OBJ_FLAG_SNAPPABLE);
        lv_obj_set_style_radius(exhaust_cards[i],7,LV_PART_MAIN);
        lv_obj_set_style_pad_all(exhaust_cards[i],0,LV_PART_MAIN);
        lv_obj_set_style_shadow_width(exhaust_cards[i],0,LV_PART_MAIN);
        create_exhaust_mark(exhaust_cards[i],i);
        lv_obj_t *label=lv_label_create(exhaust_cards[i]);
        lv_label_set_text(label,ev_exhausts[i].ui_name);
        lv_obj_set_pos(label,36,7);
        lv_obj_set_width(label,78);
        lv_obj_set_style_text_font(label,&lv_font_montserrat_12,LV_PART_MAIN);
        lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
        lv_obj_add_event_cb(exhaust_cards[i],exhaust_card_event,LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);
    }

    engine_visual=lv_canvas_create(main_page);
    lv_canvas_set_buffer(engine_visual,engine_pixels,320,86,LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(engine_visual,0,94);
    lv_obj_set_size(engine_visual,320,86);
    lv_obj_set_style_bg_color(engine_visual,lv_color_hex(0x090D12),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(engine_visual,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_width(engine_visual,0,LV_PART_MAIN);
    lv_obj_set_style_radius(engine_visual,0,LV_PART_MAIN);
    lv_obj_set_style_pad_all(engine_visual,0,LV_PART_MAIN);
    lv_obj_clear_flag(engine_visual,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);

    phase_label=lv_label_create(main_page);
    lv_label_set_text(phase_label,"OFF / SLOW");
    lv_obj_set_style_text_font(phase_label,&lv_font_montserrat_12,LV_PART_MAIN);
    lv_obj_set_pos(phase_label,9,166);
    load_bar=lv_bar_create(main_page);
    lv_obj_set_pos(load_bar,148,170);
    lv_obj_set_size(load_bar,161,5);
    lv_bar_set_range(load_bar,0,100);
    lv_obj_set_style_bg_color(load_bar,lv_color_hex(0x243241),LV_PART_MAIN);
    lv_obj_set_style_bg_color(load_bar,lv_color_hex(0xE7AC68),LV_PART_INDICATOR);

    redline_value_label=lv_label_create(main_page);
    lv_label_set_text(redline_value_label,"REDLINE  8000");
    lv_obj_set_pos(redline_value_label,9,184);
    lv_obj_set_style_text_color(redline_value_label,lv_color_hex(0xFFB020),LV_PART_MAIN);
    redline_slider=lv_slider_create(main_page);
    lv_obj_set_pos(redline_slider,9,205);
    lv_obj_set_size(redline_slider,198,22);
    lv_slider_set_range(redline_slider,1800,EV_MAX_RPM);
    lv_slider_set_value(redline_slider,8000,LV_ANIM_OFF);
    lv_obj_set_style_bg_color(redline_slider,lv_color_hex(0x252E3C),LV_PART_MAIN);
    lv_obj_set_style_bg_color(redline_slider,lv_color_hex(0xFFB020),LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(redline_slider,lv_color_hex(0xFFD28A),LV_PART_KNOB);
    lv_obj_set_style_pad_all(redline_slider,5,LV_PART_KNOB);
    lv_obj_add_event_cb(redline_slider,redline_event,LV_EVENT_ALL,NULL);

    rev_button=button_with_label(main_page,"HOLD\nTHROTTLE",216,190,95,40);
    rev_label=lv_obj_get_child(rev_button,0);
    lv_obj_set_style_text_align(rev_label,LV_TEXT_ALIGN_CENTER,LV_PART_MAIN);
    style_button(rev_button,0x693719,11);
    lv_obj_set_style_border_color(rev_button,lv_color_hex(0xFFB020),LV_PART_MAIN);
    lv_obj_add_event_cb(rev_button,rev_event,LV_EVENT_ALL,NULL);

    lv_obj_t *settings_title=lv_label_create(settings_page);
    lv_label_set_text(settings_title,"OUTPUT SETTINGS");
    lv_obj_set_pos(settings_title,12,10);
    lv_obj_set_style_text_color(settings_title,lv_color_hex(0xEAF2F8),LV_PART_MAIN);
    lv_obj_t *back_button=button_with_label(settings_page,"BACK",250,5,61,29);
    lv_obj_add_event_cb(back_button,page_event,LV_EVENT_CLICKED,(void *)(uintptr_t)false);
    lv_obj_t *volume_title=lv_label_create(settings_page);
    lv_label_set_text(volume_title,"OUTPUT VOLUME");
    lv_obj_set_pos(volume_title,15,65);
    lv_obj_set_style_text_color(volume_title,lv_color_hex(0x79879A),LV_PART_MAIN);
    lv_obj_t *volume_minus=button_with_label(settings_page,"-",15,91,58,48);
    lv_obj_t *volume_plus=button_with_label(settings_page,"+",247,91,58,48);
    lv_obj_set_style_text_font(lv_obj_get_child(volume_minus,0),&lv_font_montserrat_28,LV_PART_MAIN);
    lv_obj_set_style_text_font(lv_obj_get_child(volume_plus,0),&lv_font_montserrat_28,LV_PART_MAIN);
    lv_obj_add_event_cb(volume_minus,volume_event,LV_EVENT_CLICKED,(void *)(intptr_t)-5);
    lv_obj_add_event_cb(volume_plus,volume_event,LV_EVENT_CLICKED,(void *)(intptr_t)5);
    volume_value_label=lv_label_create(settings_page);
    lv_label_set_text(volume_value_label,"60%");
    lv_obj_set_pos(volume_value_label,80,94);
    lv_obj_set_width(volume_value_label,160);
    lv_obj_set_style_text_align(volume_value_label,LV_TEXT_ALIGN_CENTER,LV_PART_MAIN);
    lv_obj_set_style_text_font(volume_value_label,&lv_font_montserrat_28,LV_PART_MAIN);
    lv_obj_set_style_text_color(volume_value_label,lv_color_hex(0x2DE2A6),LV_PART_MAIN);

    lv_obj_update_layout(profile_strip);
    sync_profile_carousel(0);
    lv_obj_update_layout(exhaust_strip);
    sync_exhaust_carousel(0);
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
    ESP_LOGI(TAG,"UI_READY panel=ST7789 320x240 touch=FT6336 lvgl_stack=10240 animation=slider_crank refresh_ms=16 motion_ms=33 selector=swipe");
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
    bool scrolling=carousel_scrolling || exhaust_scrolling;
    unsigned exhaust=displayed_exhaust<EV_EXHAUSTS?displayed_exhaust:0;
    lvgl_port_unlock();
    ESP_LOGI(TAG,"UI_READBACK ready=1 stack_lvgl=%u animation=slider_crank profile=%s exhaust=%s render_count=%u render_max_us=%u update_max_us=%u draw_passes=%u scrolling=%d",
             board_ui_stack_high_water_mark(),ev_profiles[profile].name,ev_exhausts[exhaust].name,
             renders,render_us,refresh_us,draws,scrolling);
    return ESP_OK;
}
