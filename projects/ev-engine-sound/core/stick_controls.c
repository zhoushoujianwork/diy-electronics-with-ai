#include "stick_controls.h"

unsigned ev_stick_poll(ev_stick_controls_t *s,uint32_t now,bool a,bool b) {
    bool raw[2]={a,b},pressed[2]={false,false},released[2]={false,false};
    unsigned events=0;
    for(unsigned i=0;i<2;i++) {
        if(raw[i]!=s->raw[i]) { s->raw[i]=raw[i]; s->changed[i]=now; }
        if(raw[i]!=s->down[i] && (uint32_t)(now-s->changed[i])>=30) {
            s->down[i]=raw[i]; pressed[i]=raw[i]; released[i]=!raw[i];
        }
    }
    if(s->blocked) {
        if(!s->down[0] && !s->down[1] && !a && !b) s->blocked=false;
        return 0;
    }
    if(s->down[0] && s->down[1]) {
        s->blocked=true; s->rev=false; s->long_b=false;
        return EV_KEY_REV_OFF|EV_KEY_STOP;
    }
    if(pressed[1]) { s->b_since=now; s->long_b=false; }
    if(s->down[1] && !s->long_b && (uint32_t)(now-s->b_since)>=700) {
        s->long_b=true;
        s->page=(ev_stick_page_t)((s->page+1)%3);
        events|=EV_KEY_PAGE|EV_KEY_REV_OFF; s->rev=false;
    }
    if(released[1] && !s->long_b) events|=s->page==EV_STICK_MAIN?EV_KEY_NEXT:EV_KEY_PLUS;
    if(pressed[0]) {
        if(s->page==EV_STICK_MAIN) { events|=EV_KEY_REV_ON; s->rev=true; }
        else events|=EV_KEY_MINUS;
    }
    if(released[0] && s->rev) { events|=EV_KEY_REV_OFF; s->rev=false; }
    return events;
}
