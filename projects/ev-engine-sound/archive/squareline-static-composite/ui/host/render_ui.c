#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "lvgl.h"
#include "powertrain_canvas.h"
#include "ui.h"

enum { WIDTH=320, HEIGHT=240, RIG_H=EV_POWERTRAIN_HEIGHT, EXHAUST_W=EV_EXHAUST_WIDTH };

_Alignas(64) static uint16_t display_pixels[WIDTH*HEIGHT];
_Alignas(4) static uint16_t exhaust_pixels[EXHAUST_W*RIG_H];

static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels) {
    (void)area;
    (void)pixels;
    lv_display_flush_ready(display);
}

static int make_directory(const char *path) {
    if(mkdir(path,0755)==0 || errno==EEXIST) return 0;
    perror(path);
    return -1;
}

static int write_ppm(const char *path) {
    FILE *file=fopen(path,"wb");
    if(!file) {
        perror(path);
        return -1;
    }
    fprintf(file,"P6\n%d %d\n255\n",WIDTH,HEIGHT);
    for(unsigned index=0;index<WIDTH*HEIGHT;index++) {
        uint16_t pixel=display_pixels[index];
        uint8_t rgb[3]={
            (uint8_t)((((pixel>>11)&0x1f)*255+15)/31),
            (uint8_t)((((pixel>>5)&0x3f)*255+31)/63),
            (uint8_t)(((pixel&0x1f)*255+15)/31),
        };
        fwrite(rgb,1,sizeof(rgb),file);
    }
    if(fclose(file)!=0) {
        perror(path);
        return -1;
    }
    return 0;
}

static void render_now(void) {
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(NULL);
}

static void mount_powertrain(void) {
    ev_powertrain_state_t state={
        .profile=3,
        .cylinders=4,
        .exhaust=0,
        .last_cylinder=2,
        .running=true,
        .throttle=.42f,
        .phase=.35f,
    };
    ev_exhaust_render(exhaust_pixels,&state);
    lv_obj_add_flag(ui_PowertrainPlaceholder,LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *canvas=lv_canvas_create(ui_PowertrainHost);
    lv_canvas_set_buffer(canvas,exhaust_pixels,EXHAUST_W,RIG_H,LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(canvas,172,0);
    lv_obj_set_size(canvas,EXHAUST_W,RIG_H);
    lv_obj_remove_flag(canvas,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);

    for(unsigned i=0;i<4;i++) {
        lv_obj_t *dot=lv_obj_create(ui_PowertrainHost);
        lv_obj_set_size(dot,6,6);
        lv_obj_set_pos(dot,55+(int)i*17,98);
        lv_obj_set_style_radius(dot,LV_RADIUS_CIRCLE,LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot,lv_color_hex(i==2?0xFF9D24:0x3E4549),LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot,LV_OPA_COVER,LV_PART_MAIN);
        lv_obj_set_style_border_width(dot,0,LV_PART_MAIN);
        lv_obj_set_style_pad_all(dot,0,LV_PART_MAIN);
        lv_obj_remove_flag(dot,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_t *fire=lv_obj_create(ui_PowertrainHost);
    lv_obj_set_size(fire,8,8);
    lv_obj_set_pos(fire,117,37);
    lv_obj_set_style_radius(fire,LV_RADIUS_CIRCLE,LV_PART_MAIN);
    lv_obj_set_style_bg_color(fire,lv_color_hex(0xFF9D24),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(fire,LV_OPA_60,LV_PART_MAIN);
    lv_obj_set_style_border_width(fire,0,LV_PART_MAIN);
    lv_obj_remove_flag(fire,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *pin=lv_obj_create(ui_PowertrainHost);
    lv_obj_set_size(pin,5,5);
    lv_obj_set_pos(pin,78,97);
    lv_obj_set_style_radius(pin,LV_RADIUS_CIRCLE,LV_PART_MAIN);
    lv_obj_set_style_bg_color(pin,lv_color_hex(0xD6D9DB),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(pin,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_border_width(pin,0,LV_PART_MAIN);
    lv_obj_remove_flag(pin,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *cylinders=lv_label_create(ui_PowertrainHost);
    lv_obj_set_pos(cylinders,150,3);
    lv_obj_set_size(cylinders,17,8);
    lv_label_set_text(cylinders,"4C");
    lv_obj_set_style_text_font(cylinders,&lv_font_montserrat_8,LV_PART_MAIN);
    lv_obj_set_style_text_color(cylinders,lv_color_hex(0x777B7E),LV_PART_MAIN);
    lv_obj_t *live=lv_label_create(ui_PowertrainHost);
    lv_obj_set_pos(live,143,96);
    lv_obj_set_size(live,26,8);
    lv_label_set_text(live,"LIVE");
    lv_obj_set_style_text_font(live,&lv_font_montserrat_8,LV_PART_MAIN);
    lv_obj_set_style_text_align(live,LV_TEXT_ALIGN_RIGHT,LV_PART_MAIN);
    lv_obj_set_style_text_color(live,lv_color_hex(0xFF9D24),LV_PART_MAIN);
}

static void set_preview_state(void) {
    lv_label_set_text(ui_RpmValue,"4200 RPM");
    lv_label_set_text(ui_RunState,"RUNNING");
    lv_label_set_text(ui_EngineSelectorValue,"4C  INLINE 4  >");
    lv_label_set_text(ui_ExhaustSelectorValue,"STOCK  >");
    lv_label_set_text(ui_ThrottleState,"THROTTLE OPEN");
    lv_label_set_text(ui_ThrottleDetail,"RELEASE = AUTO OFF");
    lv_bar_set_value(ui_CountdownBar,68,LV_ANIM_OFF);
}

int main(int argc,char **argv) {
    const char *output=argc>1?argv[1]:"build/ui-squareline";
    if(make_directory(output)!=0) return 1;
    lv_init();
    lv_display_t *display=lv_display_create(WIDTH,HEIGHT);
    if(!display) return 2;
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,display_pixels,NULL,sizeof(display_pixels),LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display,flush);
    ui_init();
    mount_powertrain();
    set_preview_state();

    char path[1024];
    render_now();
    snprintf(path,sizeof(path),"%s/main.ppm",output);
    if(write_ppm(path)!=0) return 3;

    lv_obj_set_y(ui_SettingsDrawer,0);
    render_now();
    snprintf(path,sizeof(path),"%s/settings.ppm",output);
    if(write_ppm(path)!=0) return 4;
    printf("rendered %s/main.ppm and %s/settings.ppm\n",output,output);
    ui_destroy();
    lv_deinit();
    return 0;
}
