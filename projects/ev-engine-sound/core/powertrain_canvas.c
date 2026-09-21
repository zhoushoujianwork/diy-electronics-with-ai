#include "powertrain_canvas.h"

#include <math.h>
#include <stddef.h>
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

static void upstream_mark(void) {
    /* The upstream A-mark, redrawn at display resolution from art/assets.blend. */
    triangle(5,9,12,9,7,30,FG);
    triangle(11,9,17,9,14,30,FG);
    rect(7,22,8,3,FG);
    text5("ENGINE",21,6,FG);
    text5("SIM",21,15,GRID);
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
    upstream_mark();
    text5("CUTAWAY",91,6,FG);
    float a=state->phase*6.283185307f;
    int active=(int)(state->last_cylinder&1u);
    bank(73,82,-2.30f,a,state->running && active==0,true);
    bank(73,82,-0.84f,a+3.14159265f,state->running && active==1,false);
    disc(73,82,19,0xB0B0B0); disc(73,82,10,0x999999);
    int jx=73+(int)(8*cosf(a)),jy=82+(int)(8*sinf(a));
    disc(jx,jy,6,0xC6C6C6); disc(jx,jy,3,0x777777);
    line(49,101,97,101,2,GRID);
}

static void ignition(const ev_powertrain_state_t *state) {
    frame(148,0,75,54,"IGNITION");
    unsigned count=state->cylinders;
    if(count<1) count=1;
    if(count>8) count=8;
    int columns=count>4?4:(int)count;
    int rows=(int)((count+3)/4);
    int y0=rows==1?35:29;
    for(unsigned i=0;i<count;i++) {
        int x=160+(int)(i%(unsigned)columns)*(52/(columns>1?columns-1:1));
        int y=y0+(int)(i/(unsigned)columns)*15;
        bool hot=state->running && i==state->last_cylinder%count;
        ring(x,y,6,5,DIM);
        disc(x,y,hot?4:3,hot?FG:0x3E3E3E);
        if(hot) disc(x,y,2,ORANGE);
    }
}

static void throttle(float amount) {
    if(amount<0) amount=0;
    if(amount>1) amount=1;
    frame(148,53,75,55,"THROTTLE");
    line(158,69,158,98,2,FG); line(212,69,212,98,2,FG);
    int plate=84-(int)(amount*8.f);
    line(174,plate+3,198,plate-3,2,FG); disc(186,plate,2,FG);
    rect(165,99,42,4,0x2E3134);
    for(int x=168;x<202;x+=5) rect(x,99,2,4,0x111315);
    rect(205,98,6,6,ORANGE);
}

static void exhaust(unsigned kind,bool running,float phase) {
    frame(222,0,98,108,"EXHAUST");
    line(227,28,238,39,3,PINK); line(227,40,238,43,3,PINK);
    line(238,39,247,49,4,0xD6D6D6); line(238,43,247,49,4,0xBDBDBD);
    line(247,49,258,53,5,0xAFAFAF);

    if(kind==0) {
        text5("STOCK",283,15,GRID);
        line(255,47,301,47,13,0x606468); line(258,43,298,43,3,0xB8BBBD);
        rect(297,43,10,9,0xC9CBCC); rect(305,45,7,5,0x252729);
        text5("OEM",270,57,FG);
    } else if(kind==1) {
        text5("AKRA",289,15,GRID);
        triangle(254,42,301,45,295,58,0x26282A);
        triangle(254,42,295,58,257,58,0x34373A);
        line(261,45,293,53,1,0x555A5D); line(270,43,301,49,1,0x555A5D);
        rect(296,45,8,12,RED); rect(302,48,9,6,0x202224);
        text5("CF",272,49,FG);
    } else if(kind==2) {
        text5("YOSHI",283,15,GRID);
        triangle(254,43,301,40,297,59,0xB78A4D);
        triangle(254,43,297,59,257,58,0xD2A563);
        rect(258,43,4,15,0x6D4C2C); rect(293,42,5,16,0xE7DBBE);
        rect(299,45,12,9,0x34291F); text5("TI",272,49,BG);
    } else if(kind==3) {
        text5("TIN CAN",271,15,GRID);
        rect(252,38,51,27,0xBC1830); rect(254,36,47,3,0xD9DDDF);
        rect(254,65,47,3,0xAEB4B7); rect(252,42,4,19,0x831020);
        line(258,61,294,41,2,FG); line(264,65,300,45,2,FG);
        rect(268,45,25,13,0xA90E24); text5("COLA",269,48,FG);
        ring(262,40,4,3,0x777C80); rect(261,38,7,2,0xB9BEC1);
        rect(300,42,5,20,0xE44B59); rect(304,48,8,8,0x343638);
    } else {
        text5("OPEN",289,15,GRID);
        line(254,50,307,50,10,0x777B7E); line(256,47,306,47,3,0xD2D4D5);
        rect(267,44,4,13,BLUE); rect(272,44,4,13,PINK); rect(277,44,4,13,ORANGE);
        rect(304,44,8,13,0x2B2D2F); text5("NO CAN",266,62,FG);
    }

    text5("FLOW",228,78,FG);
    line(228,96,312,96,1,DIM);
    int amp=running?(kind==4?7:kind==3?5:3):0;
    int previous_y=96;
    for(int x=228;x<=312;x++) {
        float wave=sinf((float)(x-228)*0.22f+phase*6.283185307f);
        int y=96-(int)(wave*(float)amp*(0.25f+0.75f*(x-228)/84.f));
        line(x-1,previous_y,x,y,1,kind==4?RED:ORANGE);
        previous_y=y;
    }
}

void ev_powertrain_render(uint16_t *pixels,const ev_powertrain_state_t *state) {
    canvas=pixels;
    rect(0,0,W,H,BG);
    frame(0,0,149,108,"");
    engine_cutaway(state);
    ignition(state);
    throttle(state->throttle);
    exhaust(state->exhaust,state->running,state->phase);
}
