#include "powertrain_canvas.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Compact RGB565 interpretation of Ange Yaghi's Engine Simulator UI.
 * Palette, cutaway conventions and ignition layout follow the MIT-licensed
 * upstream project at commit 85f7c3b959a908ed5232ede4f1a4ac7eafe6b630.
 * See docs/third-party/engine-sim.md.
 */

enum { W=EV_POWERTRAIN_WIDTH,H=EV_POWERTRAIN_HEIGHT };
enum {
    BG=0x0E1012, FG=0xFFFFFF, GRID=0x777B7E, DIM=0x303336,
    PINK=0xF394BE, RED=0xEE4445, ORANGE=0xF4802A,
    YELLOW=0xFDBD2E, BLUE=0x77CEE0
};
static uint16_t *canvas;

static uint16_t rgb565(unsigned rgb) {
    return (uint16_t)((((rgb>>16)&0xf8)<<8)|(((rgb>>8)&0xfc)<<3)|((rgb&0xf8)>>3));
}

static void rect(int x,int y,int w,int h,unsigned color) {
    if(x<0) { w+=x; x=0; }
    if(y<0) { h+=y; y=0; }
    if(x+w>W) w=W-x;
    if(y+h>H) h=H-y;
    if(w<=0 || h<=0) return;
    uint16_t c=rgb565(color);
    for(int py=y;py<y+h;py++) for(int px=x;px<x+w;px++) canvas[py*W+px]=c;
}

static void line(int x0,int y0,int x1,int y1,int thick,unsigned color) {
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

static void disc(int cx,int cy,int radius,unsigned color) {
    int rr=radius*radius;
    for(int y=-radius;y<=radius;y++) for(int x=-radius;x<=radius;x++)
        if(x*x+y*y<=rr) rect(cx+x,cy+y,1,1,color);
}

static void ring(int cx,int cy,int outer,int inner,unsigned color) {
    int oo=outer*outer,ii=inner*inner;
    for(int y=-outer;y<=outer;y++) for(int x=-outer;x<=outer;x++) {
        int d=x*x+y*y;
        if(d<=oo && d>=ii) rect(cx+x,cy+y,1,1,color);
    }
}

static int edge(int ax,int ay,int bx,int by,int px,int py) {
    return (px-ax)*(by-ay)-(py-ay)*(bx-ax);
}

static void triangle(int ax,int ay,int bx,int by,int cx,int cy,unsigned color) {
    int minx=ax<bx?(ax<cx?ax:cx):(bx<cx?bx:cx);
    int maxx=ax>bx?(ax>cx?ax:cx):(bx>cx?bx:cx);
    int miny=ay<by?(ay<cy?ay:cy):(by<cy?by:cy);
    int maxy=ay>by?(ay>cy?ay:cy):(by>cy?by:cy);
    int winding=edge(ax,ay,bx,by,cx,cy);
    for(int y=miny;y<=maxy;y++) for(int x=minx;x<=maxx;x++) {
        int e0=edge(ax,ay,bx,by,x,y),e1=edge(bx,by,cx,cy,x,y),e2=edge(cx,cy,ax,ay,x,y);
        if((winding>=0 && e0>=0 && e1>=0 && e2>=0) ||
           (winding<0 && e0<=0 && e1<=0 && e2<=0)) rect(x,y,1,1,color);
    }
}

/* 5x7 uppercase display font, matching the upstream UI's compact technical lettering. */
typedef struct { char ch; unsigned char row[7]; } glyph_t;
static const glyph_t font[]={
    {'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},
    {'C',{14,17,16,16,16,17,14}},{'D',{30,17,17,17,17,17,30}},
    {'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},
    {'G',{14,17,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},
    {'I',{31,4,4,4,4,4,31}},{'J',{7,2,2,2,18,18,12}},
    {'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
    {'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},
    {'O',{14,17,17,17,17,17,14}},{'P',{30,17,17,30,16,16,16}},
    {'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},
    {'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},
    {'U',{17,17,17,17,17,17,14}},{'V',{17,17,17,17,17,10,4}},
    {'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},
    {'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},
    {'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},
    {'2',{14,17,1,2,4,8,31}},{'3',{30,1,1,14,1,1,30}},
    {'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},
    {'6',{14,16,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},
    {'8',{14,17,17,14,17,17,14}},{'9',{14,17,17,15,1,1,14}},
    {'-',{0,0,0,31,0,0,0}},{'.',{0,0,0,0,0,12,12}},
    {'/',{1,2,2,4,8,8,16}},{' ',{0,0,0,0,0,0,0}}
};

static const unsigned char *glyph(char ch) {
    for(size_t i=0;i<sizeof(font)/sizeof(font[0]);i++) if(font[i].ch==ch) return font[i].row;
    return font[sizeof(font)/sizeof(font[0])-1].row;
}

static void text5(const char *text,int x,int y,unsigned color) {
    for(;*text;text++,x+=6) {
        const unsigned char *rows=glyph(*text);
        for(int row=0;row<7;row++) for(int col=0;col<5;col++)
            if(rows[row]&(1u<<(4-col))) rect(x+col,y+row,1,1,color);
    }
}

static void frame(int x,int y,int w,int h,const char *title) {
    rect(x,y,w,1,GRID); rect(x,y+h-1,w,1,GRID);
    rect(x,y,1,h,GRID); rect(x+w-1,y,1,h,GRID);
    text5(title,x+5,y+4,FG);
}

static void bank(int crank_x,int crank_y,float angle,float travel,bool firing,bool left) {
    float ux=cosf(angle),uy=sinf(angle);
    int pinx=crank_x+(int)(8.f*cosf(travel));
    int piny=crank_y+(int)(8.f*sinf(travel));
    int wristx=crank_x+(int)(33.f*ux);
    int wristy=crank_y+(int)(33.f*uy);
    int crownx=crank_x+(int)(46.f*ux);
    int crowny=crank_y+(int)(46.f*uy);
    int nx=(int)(-uy*8.f),ny=(int)(ux*8.f);

    line(crank_x+(int)(23*ux)+nx,crank_y+(int)(23*uy)+ny,
         crank_x+(int)(54*ux)+nx,crank_y+(int)(54*uy)+ny,4,PINK);
    line(crank_x+(int)(23*ux)-nx,crank_y+(int)(23*uy)-ny,
         crank_x+(int)(54*ux)-nx,crank_y+(int)(54*uy)-ny,4,PINK);
    line(pinx,piny,wristx,wristy,5,0xD9D9D9);
    disc(pinx,piny,4,0xAFAFAF); disc(pinx,piny,2,0x777777);
    line(wristx+nx,wristy+ny,wristx-nx,wristy-ny,9,FG);
    line(crownx+nx,crowny+ny,crownx-nx,crowny-ny,7,FG);
    disc(wristx,wristy,2,BG);

    int hx=crank_x+(int)(59.f*ux),hy=crank_y+(int)(59.f*uy);
    line(hx+nx,hy+ny,hx-nx,hy-ny,8,PINK);
    int valve_x=hx+(left?nx/2:-nx/2),valve_y=hy+(left?ny/2:-ny/2);
    line(valve_x,valve_y,valve_x+(int)(8*ux),valve_y+(int)(8*uy),2,left?BLUE:YELLOW);
    disc(hx-(left?nx/2:-nx/2),hy-(left?ny/2:-ny/2),3,left?YELLOW:BLUE);
    if(firing) { disc(crownx,crowny,5,ORANGE); disc(crownx,crowny,2,YELLOW); }
}

static void engine_cutaway(const ev_powertrain_state_t *state) {
    frame(0,0,172,108,"ENGINE CUTAWAY");
    char metadata[12];
    snprintf(metadata,sizeof(metadata),"%uC",state->cylinders);
    text5(metadata,149,4,GRID);
    float a=state->phase*6.283185307f;
    int active=(int)(state->last_cylinder&1u);
    bank(85,83,-2.30f,a,state->running && active==0,true);
    bank(85,83,-0.84f,a+3.14159265f,state->running && active==1,false);
    disc(85,83,19,0xB0B0B0); disc(85,83,10,0x999999);
    int jx=85+(int)(8*cosf(a)),jy=83+(int)(8*sinf(a));
    disc(jx,jy,6,0xC6C6C6); disc(jx,jy,3,0x777777);
    text5("IGN",8,96,GRID);
    unsigned count=state->cylinders;
    if(count<1) count=1;
    if(count>6) count=6;
    for(unsigned i=0;i<count;i++) {
        int x=38+(int)i*(88/(count>1?(int)count-1:1));
        bool hot=state->running && i==state->last_cylinder%count;
        ring(x,99,4,3,DIM);
        disc(x,99,hot?3:2,hot?FG:0x3E3E3E);
        if(hot) disc(x,99,1,ORANGE);
    }
    text5("LIVE",142,96,state->running?ORANGE:GRID);
}

static void exhaust(unsigned kind,bool running,float phase,float throttle) {
    frame(172,0,148,108,"EXHAUST");
    line(178,27,190,39,3,PINK); line(178,39,193,43,3,PINK);
    line(190,39,207,49,5,0xD2D4D5);

    if(kind==0) {
        text5("STOCK",283,4,GRID);
        line(210,48,298,48,24,0x63676B); line(216,39,292,39,4,0x9CA0A3);
        rect(298,40,10,17,0xCDD0D2); rect(306,44,9,10,0x292C2F);
        text5("OEM",248,45,FG);
    } else if(kind==1) {
        text5("AKRA",289,4,GRID);
        triangle(207,34,303,39,291,68,0x292C2F);
        triangle(207,34,291,68,211,65,0x34373A);
        for(int x=218;x<286;x+=12) line(x,37,x+20,64,1,0x555A5D);
        rect(293,40,11,25,RED); rect(302,46,13,13,0x222527);
        text5("CF",250,48,FG);
    } else if(kind==2) {
        text5("YOSHI",283,4,GRID);
        triangle(207,36,301,32,293,68,0xC89B58);
        triangle(207,36,293,68,211,65,0xD2A563);
        line(209,36,299,33,2,0xF0DFBD);
        rect(212,36,7,30,0x795832); rect(287,34,10,33,0xEADCBE);
        rect(296,43,19,16,0x352B20); text5("TI",250,48,BG);
    } else if(kind==3) {
        text5("TIN CAN",272,4,GRID);
        rect(205,31,96,40,0xBD1831); rect(209,28,92,5,0xDCE0E2);
        rect(209,70,92,5,0xAEB4B7); rect(205,36,5,30,0x831020);
        ring(222,32,7,5,0x777C80); rect(219,29,13,2,0xB9BEC1);
        line(216,67,290,34,2,FG); line(230,73,305,41,2,FG);
        rect(300,38,7,28,0xE54B59); rect(305,45,11,13,0x34373A);
        text5("COLA",243,47,FG);
    } else {
        text5("OPEN",289,4,GRID);
        line(207,49,307,49,11,0x74787B); line(210,45,306,45,3,0xD2D4D5);
        rect(236,42,4,15,BLUE); rect(243,42,4,15,PINK); rect(250,42,4,15,ORANGE);
        rect(304,41,11,17,0x292C2F); text5("NO CAN",244,63,FG);
    }

    text5("FLOW",178,80,GRID);
    line(178,97,313,97,1,DIM);
    if(throttle<0) throttle=0;
    if(throttle>1) throttle=1;
    float drive=.55f+.45f*throttle;
    int amp=running?(int)((kind==4?8:kind==3?6:4)*drive):0;
    int previous_y=97;
    for(int x=178;x<=313;x++) {
        float wave=sinf((float)(x-178)*0.18f+phase*6.283185307f);
        int y=97-(int)(wave*(float)amp*(0.20f+0.80f*(x-178)/135.f));
        line(x-1,previous_y,x,y,1,kind==4?RED:ORANGE);
        previous_y=y;
    }
}

void ev_powertrain_render(uint16_t *pixels,const ev_powertrain_state_t *state) {
    canvas=pixels;
    rect(0,0,W,H,BG);
    engine_cutaway(state);
    exhaust(state->exhaust,state->running,state->phase,state->throttle);
}
