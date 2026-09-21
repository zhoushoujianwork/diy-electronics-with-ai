#include "powertrain_canvas.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum { W=EV_POWERTRAIN_WIDTH,H=EV_POWERTRAIN_HEIGHT };
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
    {'A',0x2bed},{'B',0x6bae},{'C',0x3923},{'D',0x6b6e},{'E',0x79a7},
    {'F',0x79a4},{'G',0x39ab},{'H',0x5bed},{'I',0x7497},{'J',0x124a},
    {'K',0x5bad},{'L',0x4927},{'M',0x5fed},{'N',0x5fed},{'O',0x2b6a},
    {'P',0x6ba4},{'Q',0x2b6b},{'R',0x6bad},{'S',0x388e},{'T',0x7492},
    {'U',0x5b6f},{'V',0x5b6a},{'W',0x5bfd},{'X',0x5a8a},{'Y',0x5a92},
    {'Z',0x72a7},{'0',0x7b6f},
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

static void engine_assembly(const ev_powertrain_state_t *state) {
    unsigned bars=state->cylinders>6?6:state->cylinders;
    if(!bars) bars=1;
    int bank_width=(int)bars*13+8;
    int bank_x=19+(94-bank_width)/2;
    uint32_t profile_color=0x3A8B83+(state->profile%3)*0x13080A;

    text3("ENGINE",9,7,1,0x5F7182);
    rect(12,15,145,80,0x0B1219); rect(13,16,143,78,0x111A22);
    rect(17,88,134,5,0x222D37); rect(25,93,18,5,0x394550);
    rect(126,93,18,5,0x394550);

    /* Cam cover, individual cylinder heads and plug leads. */
    rect(bank_x,25,bank_width,8,profile_color);
    rect(bank_x+4,22,bank_width-8,4,0x7DB4AC);
    rect(bank_x+8,23,bank_width-18,1,0xB7D8D2);
    for(unsigned i=0;i<bars;i++) {
        int x=bank_x+5+(int)i*13;
        bool firing=state->running && i==state->last_cylinder%bars;
        rect(x,34,10,25,firing?0xDB713D:0x586773);
        for(int y=37;y<57;y+=4) rect(x-2,y,14,1,0xA5B0B8);
        rect(x+3,30,3,5,firing?0xFFD16B:0xCBD3D8);
        line(x+4,30,57,18+(int)(i&1)*3,1,0xC44740);
        if(firing) rect(x+2,40,6,10,0xF3A64B);
    }
    rect(bank_x-3,59,bank_width+6,8,0x394650);
    for(int x=bank_x;x<bank_x+bank_width;x+=8) rect(x,61,5,2,0x7E8A94);

    /* Cast crankcase, oil sight glass, starter and rotating crank pulley. */
    rect(24,66,118,20,0x56636D); rect(29,69,108,13,0x75818A);
    rect(33,72,56,7,0x313C45); text3("EV",55,73,1,0xE9BC62);
    float angle=state->phase*6.283185307f;
    disc(115,76,13,0x303A42); ring(115,76,11,9,0xA5AFB6);
    disc(115,76,4,0xD89A45);
    for(int i=0;i<4;i++) {
        float a=angle+(float)i*1.570796327f;
        line(115+(int)(5*cosf(a)),76+(int)(5*sinf(a)),
             115+(int)(9*cosf(a)),76+(int)(9*sinf(a)),2,0xD7DEE2);
    }
    rect(16,68,12,13,0x343F48); rect(13,71,5,7,0xB7803B);
    rect(61,84,42,4,0xA4AFB7); rect(69,88,5,5,0x525F69);

    /* Intake trumpets and animated air pulses. */
    for(unsigned i=0;i<(bars>4?4:bars);i++) {
        int y=36+(int)i*8;
        line(bank_x-4,y,13,y-3,2,0x586773); rect(8,y-6,7,7,0xB6C0C6);
        if(state->running && state->throttle>.05f) rect(4+(int)(state->phase*5),y-4,2,2,0x4CC7D1);
    }
}

static void throttle_grip(float throttle) {
    if(throttle<0) throttle=0;
    if(throttle>1) throttle=1;
    text3("THROTTLE",191,7,1,0x5F7182);
    /* Housing, metal tube, ribbed rubber grip and bright end cap. */
    line(200,24,307,24,4,0xA7B1B8);
    rect(208,14,27,21,0x26313A); rect(211,16,21,17,0x44515C);
    disc(215,20,2,0xD9E0E4); disc(228,29,2,0xD9E0E4);
    rect(234,13,68,22,0x151C22); rect(238,15,62,18,0x252E35);
    for(int x=241;x<297;x+=6) rect(x,15,2,18,0x0B1015);
    rect(300,14,7,20,0xD57A34); rect(303,17,4,14,0xF0A04E);
    /* Twist index, cable and opening bar. */
    int marker=239+(int)(throttle*57.f);
    rect(marker,10,3,5,0xFFD06A); line(220,34,188,48,1,0x73818D);
    line(188,48,154,45,1,0x73818D); rect(235,39,72,5,0x1F2A33);
    rect(235,39,(int)(72*throttle),5,throttle>.75f?0xF06A45:0x2DE2A6);
    const char *state=throttle<.05f?"IDLE":throttle>.75f?"WIDE":"OPEN";
    uint32_t state_color=throttle>.75f?0xF08A61:0x5CCFA8;
    text3(state,191,38,1,state_color);
}

static void exhaust(unsigned kind,bool running,float phase) {
    text3("EXHAUST",172,53,1,0x5F7182);
    /* Four polished headers merge into the selected silencer. */
    for(int i=0;i<4;i++) {
        line(142,42+i*7,160+i*3,63+i*3,2,i<2?0xB5C0C7:0x77848E);
        line(160+i*3,63+i*3,190,74,2,0x87949E);
    }
    rect(184,70,18,9,0x606D77); rect(188,72,14,5,0xB5BEC4);
    if(kind==0) {
        rect(199,65,96,22,0x67727C); rect(203,67,87,4,0xAAB3BB);
        rect(208,73,68,10,0x78858F); text3("OEM",235,75,1,0xE1E6E9);
        disc(283,74,2,0xE5EBEF); disc(283,81,2,0xE5EBEF);
        rect(294,69,9,14,0xBEC6CB); rect(301,72,6,8,0x252D34);
    } else if(kind==1) {
        for(int y=0;y<24;y++) rect(199+y/5,63+y,98-y/2,1,y%3?0x252B31:0x414950);
        rect(203,65,5,19,0x12171C); rect(216,69,65,13,0x14191E);
        text3("AKRA",240,73,1,0xF0F2F3);
        line(215,70,220,76,2,0xE34B3E); line(220,76,214,80,2,0xE34B3E);
        rect(294,69,8,14,0xD34B3F); rect(300,72,7,8,0x171C21);
    } else if(kind==2) {
        for(int y=0;y<24;y++) rect(199+y/7,63+y,98-y/3,1,(y&1)?0xA67B42:0xC99B56);
        rect(205,65,6,20,0x5A4025); rect(282,66,7,18,0xEEE5CE);
        rect(218,69,59,13,0x742523); text3("YOSH",239,73,1,0xF7E7B1);
        rect(294,69,8,14,0xDCAE5A); rect(300,72,7,8,0x3A2C20);
    } else if(kind==3) {
        rect(197,60,103,30,0xA70F24); rect(202,59,94,4,0xDCE3E7);
        rect(202,87,94,4,0xBFC8CE); rect(198,65,5,20,0x770B1A);
        rect(295,64,6,22,0xED4B50); rect(207,64,5,21,0xF25D5D);
        line(214,85,279,62,2,0xFFF5E5); line(218,89,286,64,2,0xFFF5E5);
        rect(227,68,55,15,0xC2182D); text3("COLA",245,73,1,0xFFF5E5);
        disc(217,67,2,0xFFF5E5); disc(289,82,2,0xFFF5E5); disc(221,82,1,0xFFF5E5);
        rect(214,60,23,3,0x929CA3); rect(220,60,11,1,0x2E363C);
        rect(300,67,7,16,0xC3CCD1); rect(304,71,4,8,0x353D43);
    } else {
        line(198,75,302,75,10,0x626E78); line(201,72,301,72,3,0xADB6BD);
        rect(216,70,5,11,0x4A70A0); rect(221,70,5,11,0xA65B5E);
        rect(226,70,5,11,0xD17A49); text3("OPEN",252,73,1,0xF0B16E);
        rect(299,68,8,15,0x404A52); rect(303,70,5,11,0x12171C);
    }
    if(running) {
        int drift=(int)(phase*20.f)%20;
        uint32_t smoke=kind==4?0xD87943:0x586570;
        rect(312-drift/3,67-drift/4,5,4,smoke); rect(316-drift/2,57-drift/5,3,3,0x3A4651);
        if(kind==3) rect(310-drift/4,82,2,2,0xDDE5E8);
    }
}

void ev_powertrain_render(uint16_t *pixels,const ev_powertrain_state_t *state) {
    canvas=pixels;
    rect(0,0,W,H,0x070B11);
    rect(0,103,W,2,0x263544);
    for(int x=0;x<W;x+=20) rect(x,106,12,2,0x151F29);
    engine_assembly(state);
    throttle_grip(state->throttle);
    exhaust(state->exhaust,state->running,state->phase);
}
