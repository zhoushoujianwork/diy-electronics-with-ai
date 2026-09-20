#include "engine_motion.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int last_sample, max_step;
static void render(ev_engine_t *e,float duration) {
    int16_t pcm[EV_BLOCK];
    unsigned remaining=(unsigned)(duration*EV_RATE);
    while(remaining) {
        unsigned n=remaining>EV_BLOCK?EV_BLOCK:remaining;
        ev_render(e,pcm,n);
        for(unsigned i=0;i<n;i++) {
            int step=abs((int)pcm[i]-last_sample);
            if(step>max_step) max_step=step;
            last_sample=pcm[i];
            assert(abs((int)pcm[i])<30000);
        }
        remaining-=n;
    }
    assert(isfinite(e->rpm) && isfinite(e->gain));
}
int main(void) {
    for(unsigned p=0;p<EV_PROFILES;p++) {
        for(unsigned cylinder=0;cylinder<ev_profiles[p].cylinders;cylinder++) {
            for(unsigned frame=0;frame<720;frame++) {
                ev_motion_t m=ev_motion(&ev_profiles[p],cylinder,frame/720.0f);
                assert(m.stroke>=-.00001f && m.stroke<=1.00001f);
                assert(m.stage<4);
                /* Physical invariant: constant rod length throughout the cycle. */
                assert(fabsf(hypotf(m.x,m.piston-m.y)-3.5f)<.00001f);
            }
        }
        ev_engine_t e; ev_init(&e);
        ev_control_t c={.running=true,.profile=p,.volume=.6f};
        last_sample=0;
        ev_set_control(&e,&c); render(&e,.2f);
        assert(e.phase==EV_PHASE_STARTING);
        assert(e.rpm<ev_profiles[p].idle_rpm*.5f);
        render(&e,2.8f); assert(e.phase==EV_PHASE_IDLE);
        c.throttle=1; ev_set_control(&e,&c); render(&e,.08f);
        assert(e.phase==EV_PHASE_ACCEL && e.load<.9f);
        render(&e,3); assert(e.phase==EV_PHASE_HOLD);
        float high=e.rpm;
        c.throttle=0; ev_set_control(&e,&c); render(&e,.1f);
        if(e.phase!=EV_PHASE_COAST || e.rpm<=high*.8f)
            fprintf(stderr,"coast model=%u phase=%s high=%.1f rpm=%.1f load=%.3f overrun=%.3f\n",p,ev_phase_name(e.phase),high,e.rpm,e.load,e.overrun);
        assert(e.phase==EV_PHASE_COAST && e.rpm>high*.8f);
        assert(e.overrun>0 && e.load>0);
        render(&e,4); assert(e.phase==EV_PHASE_IDLE);
        c.running=false; ev_set_control(&e,&c); render(&e,2);
        assert(last_sample==0 && e.phase==EV_PHASE_OFF);
        c.running=true; ev_set_control(&e,&c); render(&e,.1f);
        assert(e.phase==EV_PHASE_STARTING);
    }
    const ev_profile_t *p=&ev_profiles[8]; /* inline4: outer and inner pairs */
    for(unsigned frame=0;frame<720;frame++) {
        float cycle=frame/720.0f;
        assert(fabsf(ev_motion(p,0,cycle).piston-ev_motion(p,3,cycle).piston)<.00001f);
        assert(fabsf(ev_motion(p,1,cycle).piston-ev_motion(p,2,cycle).piston)<.00001f);
    }
    assert(max_step<10000); /* No abrupt full-scale edge during starts/lifts/stops. */
    printf("PASS: %d Hz startup/throttle/coast/restart, 18 profiles, fixed rods, max PCM step=%d\n",EV_RATE,max_step);
}
