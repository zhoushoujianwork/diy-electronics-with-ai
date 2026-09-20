#include "motorcycle_canvas.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum { W=EV_MOTORCYCLE_WIDTH,H=EV_MOTORCYCLE_HEIGHT };
static uint16_t *canvas;

static uint16_t rgb565(uint32_t rgb) {
    return (uint16_t)((((rgb>>16)&0xf8)<<8)|(((rgb>>8)&0xfc)<<3)|((rgb&0xf8)>>3));
}

static void rect(int x,int y,int w,int h,uint32_t color) {
    if(x<0) { w+=x; x=0; }
    if(y<0) { h+=y; y=0; }
    if(x+w>W) w=W-x;
    if(y+h>H) h=H-y;
    if(w<=0 || h<=0) return;
    uint16_t c=rgb565(color);
    for(int py=y;py<y+h;py++)
        for(int px=x;px<x+w;px++) canvas[py*W+px]=c;
}

static void line(int x0,int y0,int x1,int y1,int thick,uint32_t color) {
    int dx=abs(x1-x0),sx=x0<x1?1:-1;
    int dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    for(;;) {
        rect(x0-thick/2,y0-thick/2,thick,thick,color);
        if(x0==x1 && y0==y1) break;
        int e2=2*err;
        if(e2>=dy) { err+=dy; x0+=sx; }
        if(e2<=dx) { err+=dx; y0+=sy; }
    }
}

static void disc(int cx,int cy,int radius,uint32_t color) {
    int rr=radius*radius;
    for(int y=-radius;y<=radius;y++) for(int x=-radius;x<=radius;x++)
        if(x*x+y*y<=rr) rect(cx+x,cy+y,1,1,color);
}

static void ring(int cx,int cy,int outer,int inner,uint32_t color) {
    int oo=outer*outer,ii=inner*inner;
    for(int y=-outer;y<=outer;y++) for(int x=-outer;x<=outer;x++) {
        int d=x*x+y*y;
        if(d<=oo && d>=ii) rect(cx+x,cy+y,1,1,color);
    }
}

/* 3x5 uppercase pixel font, packed left-to-right in the low 15 bits. */
typedef struct { char ch; uint16_t bits; } glyph_t;
static const glyph_t font[]={
    {'A',0x2bed},{'C',0x3923},{'E',0x79a7},{'H',0x5bed},{'I',0x7497},
    {'K',0x5bad},{'L',0x4927},{'M',0x5fed},{'O',0x2b6a},{'R',0x6bad},
    {'S',0x388e},{'T',0x7492},{'V',0x5b6a},{'Y',0x5a92},{'0',0x7b6f},
    {'1',0x2c97},{'2',0x62a7},{'3',0x628e},{'4',0x5bc9},{'5',0x798e},
    {'-',0x01c0},{'.',0x0002},{' ',0x0000}
};

static uint16_t glyph_bits(char ch) {
    for(size_t i=0;i<sizeof(font)/sizeof(font[0]);i++) if(font[i].ch==ch) return font[i].bits;
    return 0;
}

static void text3(const char *text,int x,int y,int scale,uint32_t color) {
    for(;*text;text++,x+=4*scale) {
        uint16_t bits=glyph_bits(*text);
        for(int row=0;row<5;row++) for(int col=0;col<3;col++)
            if(bits&(1u<<(14-(row*3+col)))) rect(x+col*scale,y+row*scale,scale,scale,color);
    }
}

static void wheel(int cx,int cy,float angle,bool front) {
    disc(cx,cy,27,0x05080C);
    ring(cx,cy,27,24,0x2C333B);       /* tyre sidewall */
    ring(cx,cy,25,24,0x5B6570);       /* tyre highlight */
    ring(cx,cy,21,19,0xA8B1B8);       /* alloy rim */
    ring(cx,cy,18,17,0x35414C);
    for(int i=0;i<6;i++) {
        float a=angle+(float)i*1.04719755f;
        line(cx+(int)(6*cosf(a)),cy+(int)(6*sinf(a)),
             cx+(int)(17*cosf(a)),cy+(int)(17*sinf(a)),1,0x73818D);
    }
    disc(cx,cy,10,0x4C5964);
    ring(cx,cy,9,7,0xBAC1C6);
    disc(cx,cy,4,0xD89A45);
    if(front) {
        rect(cx+7,cy-8,4,10,0xCB493C); /* brake caliper */
        rect(cx+8,cy-6,1,5,0xF0A15A);
    } else {
        ring(cx,cy,13,11,0xB28A45);   /* rear sprocket */
        for(int i=0;i<8;i++) {
            float a=(float)i*.78539816f;
            rect(cx+(int)(12*cosf(a)),cy+(int)(12*sinf(a)),2,2,0xE1B85D);
        }
    }
}

static void exhaust(unsigned kind,bool running,float phase) {
    /* Header and link pipe follow the lower frame to the left-facing outlet. */
    line(158,66,154,79,3,0x67737E);
    line(154,79,124,72,4,0x87939D);
    line(126,72,114,67,3,0xB6BEC5);
    if(kind==0) {
        /* OEM: long satin silencer, heat shield and two mounting bolts. */
        line(114,67,83,62,3,0x8B969F);
        rect(69,56,43,15,0x67727C); rect(67,59,5,9,0xBBC3C9);
        rect(73,58,34,3,0xAAB3BB); rect(78,63,25,2,0x4A555F);
        disc(105,60,1,0xE5EBEF); disc(105,67,1,0xE5EBEF);
        rect(82,60,19,7,0x77838D); text3("OEM",85,61,1,0xD3DADE);
    } else if(kind==1) {
        /* Carbon tapered can, red triangular badge and AKRA pixel wordmark. */
        for(int y=0;y<16;y++) rect(69+y/4,53+y,45-y/2,1,y%3?0x252B31:0x3B4249);
        rect(66,57,6,10,0xBFC6CB); rect(72,54,3,14,0x15191E);
        line(107,55,116,64,3,0x8B969F);
        rect(82,57,21,8,0x15191E); text3("AKRA",84,59,1,0xECEFF1);
        line(78,57,81,62,2,0xE34B3E); line(81,62,77,64,2,0xE34B3E);
        rect(68,59,3,6,0xD84A3E);
    } else if(kind==2) {
        /* Brushed titanium Yoshimura-style can with red/gold badge. */
        for(int y=0;y<17;y++) rect(69+y/6,53+y,44-y/3,1,(y&1)?0xA67B42:0xC99B56);
        rect(66,57,6,10,0xD7DDE1); rect(75,54,3,15,0x4F3A25);
        rect(103,55,5,13,0xEEE5CE); line(108,59,116,66,3,0x8D969E);
        rect(80,57,22,9,0x7B2524); text3("YOSH",82,59,1,0xF7E7B1);
        rect(68,59,3,6,0xDCAE5A);
    } else if(kind==3) {
        /* Oversized fizzy-drink can: lid, pull tab, seam, bubbles and COLA. */
        rect(65,49,47,26,0xA70F24); rect(68,48,42,3,0xDCE3E7);
        rect(68,73,42,3,0xBFC8CE); rect(65,54,4,16,0x7A0B1A);
        rect(109,53,4,18,0xED4B50); rect(72,52,4,19,0xF25D5D);
        line(76,70,103,53,2,0xF7F0DE); line(79,73,107,56,2,0xF7F0DE);
        rect(78,57,27,11,0xC2182D); text3("COLA",81,59,1,0xFFF5E5);
        disc(76,54,2,0xFFF5E5); disc(103,69,2,0xFFF5E5); rect(95,53,2,2,0xFFF5E5);
        rect(80,49,14,2,0x929CA3); rect(84,49,7,1,0x2E363C); /* pull tab */
        rect(61,56,6,12,0xC3CCD1); rect(60,59,3,6,0x353D43);
    } else {
        /* No silencer: welded straight pipe with blue/orange heat tint. */
        line(116,67,70,64,6,0x626E78); line(116,65,70,62,2,0xADB6BD);
        rect(66,60,7,8,0x59646E); rect(65,60,3,8,0x161B20);
        rect(69,61,3,6,0x4A70A0); rect(73,62,4,5,0xA65B5E);
        text3("OPEN",83,57,1,0xE17D48);
    }
    if(running) {
        int drift=(int)(phase*20.f)%20;
        uint32_t smoke=kind==4?0xD87943:0x586570;
        rect(57-drift,58,5,4,smoke); rect(47-drift/2,52,3,3,0x3A4651);
        if(kind==3) rect(52-drift,66,2,2,0xDDE5E8);
    }
}

void ev_motorcycle_render(uint16_t *pixels,const ev_motorcycle_state_t *state) {
    static const uint32_t tanks[]={0xD1493F,0x27877B,0x477BC0,0x905FB0,0xC18032,0xA33C59};
    canvas=pixels;
    uint32_t tank=tanks[state->profile%(sizeof(tanks)/sizeof(tanks[0]))];
    rect(0,0,W,H,0x070B11);
    /* Night garage backdrop and floor, kept quiet enough for the motorcycle. */
    rect(0,4,W,1,0x121A24); rect(0,20,W,1,0x0F1821);
    for(int x=0;x<W;x+=32) rect(x,5,1,15,0x0D141D);
    rect(0,96,W,2,0x303D49);
    for(int x=0;x<W;x+=20) rect(x,100,12,2,0x18222C);
    rect(40,92,239,3,0x0D1218); rect(56,95,210,2,0x111820);

    const int rx=70,fx=249,wy=73;
    float angle=state->phase*6.283185307f;
    wheel(rx,wy,angle,false); wheel(fx,wy,angle,true);

    /* Rear mudguard, swing arm, chain and twin-sided frame. */
    line(46,55,70,45,3,0x2E3944); line(70,45,91,51,3,0x2E3944);
    line(74,69,145,76,7,0x282F37); line(75,67,146,74,2,0x909BA4);
    line(76,79,145,80,2,0xC49A4F); line(78,81,145,82,1,0x6D552F);
    line(86,70,132,41,6,0xD47B32); line(132,41,164,73,6,0xD47B32);
    line(164,73,86,70,5,0xA8582A); line(164,73,210,47,6,0xD47B32);
    line(133,43,117,68,3,0xF0A04E); line(133,43,163,72,2,0x6F321E);

    /* Front end: fork tubes, fender, headlamp, bars and two mirrors. */
    line(211,45,247,73,6,0x303942); line(215,43,251,72,3,0xB9C3CA);
    line(218,43,224,24,4,0x9DA8B0); line(216,26,238,23,3,0xC0C8CE);
    line(224,25,205,15,2,0x828D96); line(232,24,248,13,2,0x828D96);
    rect(198,10,12,8,0x3E4953); rect(200,11,8,5,0x7B8A96); /* left mirror */
    rect(246,8,13,8,0x3E4953); rect(248,9,9,5,0x7B8A96);  /* right mirror */
    rect(212,31,18,13,0x252D35); rect(216,33,13,8,0xE6C85F);
    rect(228,34,5,6,0xFFF0A5); rect(215,29,10,2,0x64C7D2); /* instrument */
    line(229,58,252,51,3,0x444F59); line(239,48,258,49,3,0x333C44);

    /* Tail, stitched seat and stepped pillion pad. */
    rect(91,31,56,8,0x1D242B); rect(98,27,42,5,0x313A42);
    rect(102,29,34,1,0x727C84); rect(108,32,3,1,0x89939A);
    rect(119,32,3,1,0x89939A); rect(130,32,3,1,0x89939A);
    line(89,35,79,42,4,0xB83C35); rect(75,39,8,6,0xE04D43);

    /* Sculpted fuel tank with highlight, knee recess and EV badge. */
    for(int y=0;y<20;y++) {
        int inset=y<4?4-y:y/5;
        rect(126+inset,38+y,61-inset*2,1,tank);
    }
    line(133,39,175,39,2,0xF08A66); line(137,42,169,42,2,0xEF9A78);
    rect(136,47,38,2,0x752F2E); rect(146,48,24,8,0x26313A);
    text3("EV",151,50,1,0xF0D37B); rect(179,44,5,10,0x1B232B);

    /* Engine: finned head, cylinders, cases, clutch cover and glowing firing bar. */
    rect(128,56,50,27,0x252D35); rect(132,55,40,4,0x717C85);
    for(int y=59;y<69;y+=3) rect(131,y,43,1,0xA0A9B0);
    unsigned bars=state->cylinders>6?6:state->cylinders;
    if(!bars) bars=1;
    for(unsigned i=0;i<bars;i++) {
        int x=134+(int)i*6;
        bool firing=state->running && i==state->last_cylinder%bars;
        rect(x,60-(firing?3:0),4,13+(firing?3:0),firing?0xF1A34A:0x46535E);
        rect(x+1,61-(firing?3:0),1,10,0x151C22);
    }
    rect(132,72,42,11,0x69747D); disc(162,77,8,0x89949C);
    ring(162,77,6,5,0x343E47); disc(142,77,4,0x414C55);
    rect(129,84,51,3,0xB1BAC0); rect(151,87,5,5,0x5D6871);

    /* Controls and tiny functional details. */
    line(145,83,132,91,2,0x9BA5AC); rect(126,90,9,3,0x404B54);
    line(166,83,181,90,2,0x9BA5AC); rect(179,89,9,3,0x404B54);
    line(105,40,101,55,3,0x66727C); rect(97,52,8,3,0xABB4BB);
    rect(115,42,4,4,0xD5DEE3); rect(188,60,5,3,0xF0A24D);

    exhaust(state->exhaust,state->running,state->phase);
}
