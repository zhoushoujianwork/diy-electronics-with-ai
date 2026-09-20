#include "stick_controls.h"
#include "engine_canvas.h"
#include "engine_voice.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

static void controls(void) {
    ev_stick_controls_t s={0};
    assert(!ev_stick_poll(&s,0,true,false));
    assert(!ev_stick_poll(&s,10,false,false)); /* bounce */
    assert(!ev_stick_poll(&s,20,true,false));
    assert(!ev_stick_poll(&s,49,true,false));
    assert(ev_stick_poll(&s,50,true,false)==EV_KEY_REV_ON);
    assert(!ev_stick_poll(&s,60,false,false));
    assert(ev_stick_poll(&s,90,false,false)==EV_KEY_REV_OFF);
    assert(!ev_stick_poll(&s,100,false,true));
    assert(!ev_stick_poll(&s,130,false,true));
    assert(!ev_stick_poll(&s,200,false,false));
    assert(ev_stick_poll(&s,230,false,false)==EV_KEY_NEXT);
    assert(!ev_stick_poll(&s,300,false,true));
    assert(!ev_stick_poll(&s,330,false,true));
    assert(ev_stick_poll(&s,1030,false,true)==(EV_KEY_PAGE|EV_KEY_REV_OFF));
    assert(s.page==EV_STICK_EXHAUST);
    assert(!ev_stick_poll(&s,1100,false,false));
    assert(!ev_stick_poll(&s,1130,false,false)); /* no plus after long hold */
    assert(!ev_stick_poll(&s,1200,true,false));
    assert(ev_stick_poll(&s,1230,true,false)==EV_KEY_MINUS);
    assert(!ev_stick_poll(&s,1300,false,false));
    assert(!ev_stick_poll(&s,1330,false,false));
    assert(!ev_stick_poll(&s,1400,false,true));
    assert(!ev_stick_poll(&s,1430,false,true));
    assert(!ev_stick_poll(&s,1500,false,false));
    assert(ev_stick_poll(&s,1530,false,false)==EV_KEY_PLUS);
    /* Combo stop must never restart when one finger releases first. */
    s=(ev_stick_controls_t){0};
    ev_stick_poll(&s,0,true,false);
    assert(ev_stick_poll(&s,30,true,false)==EV_KEY_REV_ON);
    ev_stick_poll(&s,50,true,true);
    assert(ev_stick_poll(&s,80,true,true)==(EV_KEY_REV_OFF|EV_KEY_STOP));
    assert(!ev_stick_poll(&s,1000,true,true));
    ev_stick_poll(&s,1100,true,false);
    assert(!ev_stick_poll(&s,1130,true,false));
    ev_stick_poll(&s,1200,false,false);
    assert(!ev_stick_poll(&s,1230,false,false));
    assert(!s.blocked);
    ev_stick_poll(&s,1300,true,false);
    assert(ev_stick_poll(&s,1330,true,false)==EV_KEY_REV_ON);
    /* Clock wrap and hold through boot are safe. */
    s=(ev_stick_controls_t){.blocked=true};
    assert(!ev_stick_poll(&s,0,true,false));
    assert(!ev_stick_poll(&s,40,true,false));
    ev_stick_poll(&s,50,false,false);
    assert(!ev_stick_poll(&s,80,false,false));
    assert(!s.blocked);
    s=(ev_stick_controls_t){0};
    ev_stick_poll(&s,UINT32_MAX-10,true,false);
    assert(ev_stick_poll(&s,25,true,false)==EV_KEY_REV_ON);
    /* All menu pages cycle without leaking plus/profile actions. */
    s=(ev_stick_controls_t){0};
    for(unsigned i=0;i<EV_STICK_PAGE_COUNT;i++) {
        unsigned t=i*1000;
        ev_stick_poll(&s,t,false,true); ev_stick_poll(&s,t+30,false,true);
        assert(ev_stick_poll(&s,t+730,false,true)==(EV_KEY_PAGE|EV_KEY_REV_OFF));
        assert(s.page==(ev_stick_page_t)((i+1)%EV_STICK_PAGE_COUNT));
        ev_stick_poll(&s,t+800,false,false);
        assert(!ev_stick_poll(&s,t+830,false,false));
    }
}
static void canvas(void) {
    const unsigned sizes[][2]={{320,116},{240,80},{1,1},{135,60}};
    for(unsigned size=0;size<4;size++) {
        unsigned w=sizes[size][0],h=sizes[size][1],n=w*h;
        uint16_t *p=malloc((n+2)*sizeof(*p)); assert(p);
        for(unsigned profile=0;profile<EV_PROFILES;profile++) {
            for(unsigned phase=0;phase<12;phase++) {
                p[0]=0xbeef; p[n+1]=0xcafe;
                ev_canvas_render(p+1,w,h,profile,phase/12.0f,phase!=0);
                assert(p[0]==0xbeef && p[n+1]==0xcafe);
            }
        }
        free(p);
    }
    uint16_t p=0xbeef;
    ev_canvas_render(&p,321,116,0,0,true); assert(p==0xbeef);
    ev_canvas_render(&p,1,1,0,NAN,true); assert(p==0xbeef);
    ev_canvas_render(NULL,1,1,0,0,true);
}
int main(void) { controls(); canvas(); puts("Stick controls and canvas bounds OK"); return 0; }
