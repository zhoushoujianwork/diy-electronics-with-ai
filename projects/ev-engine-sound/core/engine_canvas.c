#include "engine_canvas.h"
#include "engine_motion.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef enum { ENGINE_LAYOUT_INLINE,ENGINE_LAYOUT_V,ENGINE_LAYOUT_FLAT } engine_layout_t;
typedef struct { uint16_t *pixels; unsigned width,height; float cycle; bool running; } canvas_t;
typedef struct { int32_t x,y; } point_t;
typedef struct { int32_t x1,y1,x2,y2; } area_t;

static engine_layout_t profile_layout(unsigned profile) {
    if(profile>=EV_PROFILES) profile=0;
    const char *name=ev_profiles[profile].name;
    if(!strcmp(name,"flat6")) return ENGINE_LAYOUT_FLAT;
    if(!strcmp(name,"vtwin") || !strcmp(name,"vtwin90") ||
       !strcmp(name,"v6_60") || !strcmp(name,"flatplane8") ||
       !strcmp(name,"crossplane8") || !strcmp(name,"v10") ||
       !strcmp(name,"v12")) return ENGINE_LAYOUT_V;
    return ENGINE_LAYOUT_INLINE;
}

static uint16_t pixel_color(uint32_t rgb) {
    return (uint16_t)(((rgb>>8)&0xF800)|((rgb>>5)&0x07E0)|((rgb>>3)&0x001F));
}
static void pixel(canvas_t *canvas,int x,int y,uint16_t color) {
    if((unsigned)x<320 && (unsigned)y<116) {
        unsigned px=(unsigned)x*canvas->width/320,py=(unsigned)y*canvas->height/116;
        canvas->pixels[py*canvas->width+px]=color;
    }
}
static void draw_line(canvas_t *layer,int x1,int y1,int x2,int y2,
                      uint32_t color,int width,uint8_t opacity) {
    (void)layer; (void)opacity;
    uint16_t c=pixel_color(color);
    int dx=abs(x2-x1),sx=x1<x2?1:-1,dy=-abs(y2-y1),sy=y1<y2?1:-1;
    int error=dx+dy,low=-(width/2),high=low+width;
    for(;;) {
        for(int oy=low;oy<high;oy++)
            for(int ox=low;ox<high;ox++) pixel(layer,x1+ox,y1+oy,c);
        if(x1==x2 && y1==y2) break;
        int twice=2*error;
        if(twice>=dy) { error+=dy; x1+=sx; }
        if(twice<=dx) { error+=dx; y1+=sy; }
    }
}
static void draw_circle(canvas_t *layer,int x,int y,int radius,
                        uint32_t fill,uint32_t border,uint8_t opacity) {
    (void)layer; (void)opacity;
    uint16_t f=pixel_color(fill),b=pixel_color(border);
    for(int yy=-radius;yy<=radius;yy++) for(int xx=-radius;xx<=radius;xx++) {
        int distance=xx*xx+yy*yy;
        if(distance<=radius*radius)
            pixel(layer,x+xx,y+yy,distance>(radius-1)*(radius-1)?b:f);
    }
}

/* Local (x,y): x across the bore, y outwards along its axis. */
typedef struct { canvas_t *layer; float x,y,ux,uy,r; } cylinder_view_t;
static point_t cylinder_point(const cylinder_view_t *v,float x,float y) {
    return (point_t){(int32_t)lroundf(v->x+v->r*(-v->uy*x+v->ux*y)),
                        (int32_t)lroundf(v->y+v->r*(v->ux*x+v->uy*y))};
}
static void cylinder_line(const cylinder_view_t *v,float x,float y,float x2,float y2,
                          uint32_t color,int width) {
    point_t a=cylinder_point(v,x,y),b=cylinder_point(v,x2,y2);
    draw_line(v->layer,a.x,a.y,b.x,b.y,color,width,255);
}
static void draw_number(canvas_t *layer,int x,int y,unsigned n) {
    (void)layer;
    static const uint16_t digits[]={0x7B6F,0x2492,0x73E7,0x73CF,0x5BC9,
                                   0x79CF,0x79EF,0x7249,0x7BEF,0x7BCF};
    unsigned value=n+1;
    unsigned count=value>=10?2:1;
    for(unsigned i=0;i<count;i++) {
        unsigned digit=count==2 && i==0?value/10:value%10;
        for(int row=0;row<5;row++) for(int col=0;col<3;col++)
            if(digits[digit] & (1U<<(14-row*3-col))) {
                int px=x-(int)count*4+(int)i*8+col*2;
                int py=y-5+row*2;
                uint16_t color=pixel_color(0x8292A5);
                pixel(layer,px,py,color); pixel(layer,px+1,py,color);
                pixel(layer,px,py+1,color); pixel(layer,px+1,py+1,color);
            }
    }
}
static void draw_cylinder(const cylinder_view_t *v,const ev_profile_t *p,unsigned n) {
    ev_motion_t m=ev_motion(p,n,v->layer->cycle);
    const uint32_t colors[]={0xFFAF52,0x9D657A,0x58BACB,0xAB91D1};
    uint32_t chamber=v->layer->running?colors[m.stage]:0x243342;
    int bore=(int)lroundf(v->r*1.45f);
    /* Cylinder jacket, combustion chamber, piston crown and two rings. */
    cylinder_line(v,-.95f,1.9f,-.95f,5.35f,0xBA7199,2);
    cylinder_line(v,.95f,1.9f,.95f,5.35f,0xBA7199,2);
    cylinder_line(v,-.95f,5.35f,.95f,5.35f,0xDF9BBB,3);
    cylinder_line(v,0,m.piston+.45f,0,5.0f,chamber,bore);
    cylinder_line(v,-.72f,m.piston,.72f,m.piston,0xE1E8EE,(int)(v->r*.8f)+1);
    cylinder_line(v,-.70f,m.piston+.30f,.70f,m.piston+.30f,0x778D9E,1);
    cylinder_line(v,-.70f,m.piston-.15f,.70f,m.piston-.15f,0x778D9E,1);
    /* Intake/exhaust valves open only on their respective strokes. */
    float lift=.38f*sinf(m.stroke*3.141592654f);
    float intake=m.stage==2?lift:0,exhaust=m.stage==1?lift:0;
    cylinder_line(v,-.55f,5.85f,-.55f,5.12f-intake,0x71D4DC,2);
    cylinder_line(v,-.80f,5.12f-intake,-.30f,5.12f-intake,0x71D4DC,2);
    cylinder_line(v,.55f,5.85f,.55f,5.12f-exhaust,0xFFB56B,2);
    cylinder_line(v,.30f,5.12f-exhaust,.80f,5.12f-exhaust,0xFFB56B,2);
    point_t pin=cylinder_point(v,m.x,m.y),wrist=cylinder_point(v,0,m.piston);
    cylinder_line(v,0,0,m.x,m.y,0x60788E,(int)v->r);
    draw_line(v->layer,pin.x,pin.y,wrist.x,wrist.y,0xD5DFE6,3,255);
    draw_circle(v->layer,pin.x,pin.y,2,0x15202C,0x91A7BA,255);
    draw_circle(v->layer,wrist.x,wrist.y,1,0x15202C,0xC9D9E4,255);
    /* Slow-motion flame is in phase with the piston; live firing is the top lamp. */
    if(v->layer->running && m.cycle<.055f) {
        point_t spark=cylinder_point(v,0,5.1f);
        draw_circle(v->layer,spark.x,spark.y,2,0xFFF0AA,0xFFF7D5,255);
    }
    point_t number=cylinder_point(v,0,7.0f);
    draw_number(v->layer,number.x,number.y,n);
}
void ev_canvas_render(uint16_t *pixels,unsigned width,unsigned height,
                      unsigned profile,float cycle,bool running) {
    if(!pixels || !width || !height || width>320 || height>116 || !isfinite(cycle)) return;
    canvas_t canvas={pixels,width,height,cycle,running};
    canvas_t *layer=&canvas;
    area_t area={0,0,319,115};
    uint16_t background=pixel_color(0x090D12);
    for(unsigned i=0;i<width*height;i++) pixels[i]=background;
    unsigned index=profile<EV_PROFILES?profile:0;
    const ev_profile_t *p=&ev_profiles[index];
    engine_layout_t layout=profile_layout(index);
    float cx=(area.x1+area.x2)*.5f;
    float cy=area.y1+81;
    /* Sparse guides keep the mechanism legible without excessive draw commands. */
    draw_line(layer,area.x1+12,area.y1+103,area.x2-12,area.y1+103,0x21303D,1,255);
    if(layout==ENGINE_LAYOUT_INLINE) {
        float spacing=p->cylinders>1?fminf(46,272.0f/p->cylinders):0;
        float first=cx-spacing*(p->cylinders-1)*.5f;
        draw_line(layer,first-14,cy,first+spacing*(p->cylinders-1)+14,cy,0x34485B,4,255);
        for(unsigned n=0;n<p->cylinders;n++) {
            float x=first+n*spacing;
            draw_circle(layer,x,cy,10,0x16222E,0x3C5264,255);
            cylinder_view_t v={layer,x,cy,0,-1,9};
            draw_cylinder(&v,p,n);
        }
    } else if(layout==ENGINE_LAYOUT_V) {
        unsigned pairs=p->cylinders/2;
        float spacing=fminf(100,288.0f/pairs);
        float angle=!strcmp(p->name,"vtwin")?45:(!strcmp(p->name,"v10")?72:
                    (p->cylinders==8 || !strcmp(p->name,"vtwin90"))?90:60);
        float half=angle*3.141592654f/360;
        float radius=fminf(9.5f,spacing/(12*sinf(half)+4));
        float first=cx-spacing*(pairs-1)*.5f;
        draw_line(layer,first-12,cy,first+spacing*(pairs-1)+12,cy,0x34485B,4,255);
        for(unsigned pair=0;pair<pairs;pair++) {
            float x=first+pair*spacing;
            draw_circle(layer,x,cy,(int)radius+2,0x16222E,0x3C5264,255);
            for(unsigned bank=0;bank<2;bank++) {
                cylinder_view_t v={layer,x,cy,(bank?1:-1)*sinf(half),-cosf(half),radius};
                draw_cylinder(&v,p,pair*2+bank);
            }
        }
    } else {
        unsigned pairs=p->cylinders/2;
        float spacing=30;
        float first=area.y1+52-spacing*(pairs-1)*.5f;
        draw_line(layer,cx,first-12,cx,first+(pairs-1)*spacing+12,0x34485B,4,255);
        for(unsigned pair=0;pair<pairs;pair++) {
            float y=first+pair*spacing;
            draw_circle(layer,cx,y,9,0x16222E,0x3C5264,255);
            for(unsigned bank=0;bank<2;bank++) {
                cylinder_view_t v={layer,cx,y,bank?1:-1,0,9};
                draw_cylinder(&v,p,pair*2+bank);
            }
        }
    }
}
