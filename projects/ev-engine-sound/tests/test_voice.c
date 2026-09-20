#include "engine_voice.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void seconds(ev_engine_t *e, int n) {
    int16_t pcm[EV_BLOCK];
    for(int i=0;i<n*EV_RATE/EV_BLOCK;i++) ev_render(e,pcm,EV_BLOCK);
}
static unsigned profile_index(const char *name) {
    for(unsigned i=0;i<EV_PROFILES;i++)
        if(!strcmp(ev_profiles[i].name,name)) return i;
    assert(!"missing test profile");
    return 0;
}
int main(void) {
    ev_engine_t e; ev_init(&e);
    assert(EV_MAX_CYLINDERS==12);
    assert(ev_profiles[profile_index("v12")].cylinders==12);
    for(unsigned p=0;p<EV_PROFILES;p++) {
        assert(ev_profiles[p].cylinders>=1);
        assert(ev_profiles[p].cylinders<=EV_MAX_CYLINDERS);
        bool order_seen[EV_MAX_CYLINDERS]={false};
        for(unsigned c=0;c<ev_profiles[p].cylinders;c++) {
            assert(ev_profiles[p].firing[c]>=0.0f);
            assert(ev_profiles[p].firing[c]<1.0f);
            if(c) assert(ev_profiles[p].firing[c]>ev_profiles[p].firing[c-1]);
            assert(ev_profiles[p].firing_order[c]<ev_profiles[p].cylinders);
            assert(!order_seen[ev_profiles[p].firing_order[c]]);
            order_seen[ev_profiles[p].firing_order[c]]=true;
        }
    }
    assert(fabsf(e.control.volume-.60f)<.001f);
    int16_t pcm[EV_BLOCK];
    ev_render(&e,pcm,EV_BLOCK);
    for(unsigned i=0;i<EV_BLOCK;i++) assert(pcm[i]==0);
    const char *bad[]={"", "volume nan", "rpm inf", "volume 101", "throttle -1", "start garbage", "profile nope", "gear 7", "gear -1", "gear 2x", "redline 16001", "redline 100", "rpm 3000x", "rpm 15000", "stop extra words"};
    ev_control_t old=e.control;
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        assert(ev_command(&e.control,bad[i])==-1);
        assert(!memcmp(&old,&e.control,sizeof(old)));
    }
    assert(ev_command(&e.control,"ping")==1);
    assert(ev_command(&e.control,"status")==2);
    assert(ev_command(&e.control,"gear N")==0 && e.control.gear==0);
    assert(ev_command(&e.control,"gear 6")==0 && e.control.gear==6);
    assert(ev_command(&e.control,"profile triple270")==0 &&
           e.control.profile==profile_index("triple270"));
    assert(ev_command(&e.control,"redline 15000")==0 && ev_redline(&e.control)==15000);
    e.control.running=true; e.control.throttle=1; e.control.rpm=0; e.control.gear=1;
    ev_set_control(&e,&e.control); seconds(&e,10);
    assert(e.rpm>14000 && e.rpm<=15005);
    assert(ev_command(&e.control,"redline default")==0 && ev_redline(&e.control)==11500);
    for(unsigned p=0;p<EV_PROFILES;p++) {
        ev_init(&e);
        ev_control_t c={.running=true,.profile=p,.volume=1,.rpm=3600};
        ev_set_control(&e,&c); seconds(&e,5);
        uint64_t before=e.firings;
        seconds(&e,2);
        assert(fabs((double)(e.firings-before)-60*ev_profiles[p].cylinders)<=1);
        assert(fabsf(e.rpm-3600)<5);
        ev_render(&e,pcm,EV_BLOCK);
        int peak=0;
        for(unsigned i=0;i<EV_BLOCK;i++) {
            int v=pcm[i]<0?-pcm[i]:pcm[i]; if(v>peak) peak=v;
            assert(v<30000);
        }
        assert(peak>100);
        c.running=false; ev_set_control(&e,&c); seconds(&e,2);
        ev_render(&e,pcm,EV_BLOCK);
        for(unsigned i=0;i<EV_BLOCK;i++) assert(pcm[i]==0);
        assert(isfinite(e.resonator1));
    }
    /* Sequential gearbox: an upshift causes a finite ignition cut and a lower
     * steady throttle-mode RPM. Fixed-RPM mode remains an explicit override. */
    ev_init(&e);
    ev_control_t geared={.running=true,.profile=0,.volume=.2f,.throttle=.6f,.gear=1};
    ev_set_control(&e,&geared); seconds(&e,8);
    float first_rpm=e.rpm;
    geared.gear=6; ev_set_control(&e,&geared);
    assert(e.shift_samples>0);
    seconds(&e,1);
    assert(e.shift_samples==0);
    seconds(&e,7);
    assert(e.rpm<first_rpm*.7f);
    geared.rpm=4200; ev_set_control(&e,&geared); seconds(&e,8);
    assert(fabsf(e.rpm-4200)<5);
    /* Uneven firing intervals: 270/450 and 315/405 degrees at fixed RPM. */
    const char *uneven_twins[]={"twin270","vtwin"};
    for(unsigned model=0;model<2;model++) {
        unsigned p=profile_index(uneven_twins[model]);
        ev_init(&e); ev_control_t c={.running=true,.profile=p,.volume=.2f,.rpm=3600};
        ev_set_control(&e,&c); e.rpm=3600;
        int events[6], found=0;
        for(int i=0;found<6 && i<10000;i++) {
            uint64_t before=e.firings; ev_render(&e,pcm,1);
            if(e.firings!=before) events[found++]=i;
        }
        assert(found==6);
        int short_gap=model==0?400:467, long_gap=model==0?667:600;
        for(int i=1;i<6;i++) {
            int gap=events[i]-events[i-1];
            assert(abs(gap-short_gap)<=1 || abs(gap-long_gap)<=1);
        }
    }
    /* The selectable 270-degree triple is a T-plane-style uneven pattern:
     * 270/270/180 degrees across one 720-degree four-stroke cycle. */
    ev_init(&e);
    ev_control_t triple={.running=true,.profile=profile_index("triple270"),
                         .volume=.2f,.rpm=3600};
    ev_set_control(&e,&triple); e.rpm=3600;
    int triple_events[7],triple_found=0;
    for(int i=0;triple_found<7 && i<10000;i++) {
        uint64_t before=e.firings; ev_render(&e,pcm,1);
        if(e.firings!=before) triple_events[triple_found++]=i;
    }
    assert(triple_found==7);
    for(int i=1;i<7;i++) {
        int gap=triple_events[i]-triple_events[i-1];
        assert(abs(gap-400)<=1 || abs(gap-267)<=1);
    }
    /* Long run, max RPM, repeated model switches: bounds and finite state. */
    for(unsigned p=0;p<EV_PROFILES;p++) {
        ev_control_t c={.running=true,.profile=p,.volume=1,.throttle=1};
        ev_set_control(&e,&c); seconds(&e,30);
        assert(isfinite(e.rpm) && isfinite(e.resonator1));
        if(e.rpm>ev_profiles[p].redline_rpm+5)
            fprintf(stderr,"profile=%s rpm=%f redline=%f\n",ev_profiles[p].name,
                    e.rpm,ev_profiles[p].redline_rpm);
        assert(e.rpm<=ev_profiles[p].redline_rpm+5);
    }
    puts("PASS: 1-12 cylinders, parser, ignition cadence, uneven intervals, RPM response, stop, long run");
    return 0;
}
