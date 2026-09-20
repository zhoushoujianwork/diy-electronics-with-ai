#include "motorcycle_canvas.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { PIXELS=EV_MOTORCYCLE_WIDTH*EV_MOTORCYCLE_HEIGHT };

static unsigned differing_pixels(const uint16_t *a,const uint16_t *b) {
    unsigned count=0;
    for(unsigned i=0;i<PIXELS;i++) if(a[i]!=b[i]) count++;
    return count;
}

int main(void) {
    static uint16_t guarded[PIXELS+2],baseline[PIXELS],variant[PIXELS];
    guarded[0]=0x1357; guarded[PIXELS+1]=0x2468;
    ev_motorcycle_state_t state={
        .profile=1,.cylinders=2,.exhaust=0,.last_cylinder=1,
        .running=true,.phase=.18f
    };
    ev_motorcycle_render(&guarded[1],&state);
    assert(guarded[0]==0x1357 && guarded[PIXELS+1]==0x2468);
    memcpy(baseline,&guarded[1],sizeof(baseline));

    ev_motorcycle_render(variant,&state);
    assert(memcmp(baseline,variant,sizeof(baseline))==0);
    for(unsigned exhaust=1;exhaust<5;exhaust++) {
        state.exhaust=exhaust;
        ev_motorcycle_render(variant,&state);
        assert(differing_pixels(baseline,variant)>100);
    }
    state.exhaust=0; state.phase=.63f;
    ev_motorcycle_render(variant,&state);
    assert(differing_pixels(baseline,variant)>80);
    puts("motorcycle canvas tests passed");
    return 0;
}
