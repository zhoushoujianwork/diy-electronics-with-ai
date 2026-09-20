#include "engine_voice.h"
#include <math.h>
#include <string.h>

const ev_profile_t ev_profiles[EV_PROFILES] = {
    {"single",     "360 CRANK",    1,{0},                              {0},             1300, 8000,105},
    {"twin180",    "180 P-TWIN",   2,{0,180.0f/720},                    {0,1},           1200,10000,155},
    {"twin270",    "270 P-TWIN",   2,{0,270.0f/720},                    {0,1},           1200, 9000,145},
    {"twin360",    "360 P-TWIN",   2,{0,360.0f/720},                    {0,1},           1100, 8500,125},
    {"vtwin",      "45 V-TWIN",    2,{0,315.0f/720},                    {0,1},            900, 5500, 85},
    {"vtwin90",    "90 V-TWIN",    2,{0,270.0f/720},                    {0,1},           1100,10500,165},
    {"triple120",  "120 EVEN",     3,{0,240.0f/720,480.0f/720},         {0,2,1},         1200,12000,185},
    {"triple270",  "270 T-PLANE",  3,{0,270.0f/720,540.0f/720},         {0,2,1},         1150,11500,175},
    {"inline4",    "EVEN INLINE",  4,{0,180.0f/720,360.0f/720,540.0f/720},
                                                                         {0,2,3,1},       1400,11000,210},
    {"crossplane4","CROSSPLANE",   4,{0,270.0f/720,450.0f/720,540.0f/720},
                                                                         {0,2,1,3},       1200,14000,195},
    {"inline5",    "INLINE FIVE",  5,{0,144.0f/720,288.0f/720,432.0f/720,
                                           576.0f/720},                  {0,1,3,4,2}, 900, 7500,205},
    {"inline6",    "INLINE SIX",   6,{0,120.0f/720,240.0f/720,360.0f/720,
                                           480.0f/720,600.0f/720},       {0,4,2,5,3,1},800,7500,215},
    {"v6_60",      "60 V-SIX",     6,{0,120.0f/720,240.0f/720,360.0f/720,
                                           480.0f/720,600.0f/720},       {0,3,1,4,2,5},850,8000,190},
    {"flat6",      "FLAT SIX",     6,{0,120.0f/720,240.0f/720,360.0f/720,
                                           480.0f/720,600.0f/720},       {0,3,1,4,2,5},900,8500,230},
    {"flatplane8", "FLATPLANE V8", 8,{0, 90.0f/720,180.0f/720,270.0f/720,
                                           360.0f/720,450.0f/720,540.0f/720,
                                           630.0f/720},                  {0,4,1,5,2,6,3,7},1000,9000,245},
    {"crossplane8","CROSSPLANE V8",8,{0, 90.0f/720,180.0f/720,270.0f/720,
                                           360.0f/720,450.0f/720,540.0f/720,
                                           630.0f/720},                  {0,4,3,7,2,6,1,5},700,7000,120},
    {"v10",        "72 V-TEN",    10,{0, 72.0f/720,144.0f/720,216.0f/720,
                                           288.0f/720,360.0f/720,432.0f/720,
                                           504.0f/720,576.0f/720,648.0f/720},
                                                                         {0,5,2,7,4,9,1,6,3,8},900,9000,255},
    {"v12",        "60 V-TWELVE", 12,{0, 60.0f/720,120.0f/720,180.0f/720,
                                           240.0f/720,300.0f/720,360.0f/720,
                                           420.0f/720,480.0f/720,540.0f/720,
                                           600.0f/720,660.0f/720},
                                                                         {0,6,1,7,2,8,3,9,4,10,5,11},650,8500,265}
};

static float clamp(float x, float lo, float hi) {
    return isfinite(x) ? fminf(hi, fmaxf(lo, x)) : lo;
}

/* Same virtual road load at the same throttle: higher gears pull the engine
 * to a lower steady RPM. Neutral remains a free-revving engine. */
static const float gear_rpm_scale[EV_GEARS+1]={1.00f,1.00f,.82f,.68f,.58f,.50f,.44f};

float ev_redline(const ev_control_t *control) {
    unsigned profile=control && control->profile<EV_PROFILES?control->profile:0;
    float base=ev_profiles[profile].redline_rpm;
    if(!control || control->redline_rpm<=0 || !isfinite(control->redline_rpm)) return base;
    return clamp(control->redline_rpm,ev_profiles[profile].idle_rpm+500.0f,EV_MAX_RPM);
}

static void resonance(ev_engine_t *e) {
    const float r = .985f;
    e->resonator_a = 2*r*cosf(6.283185307f * ev_profiles[e->control.profile].resonance_hz / EV_RATE);
    e->resonator_b = r*r;
}

void ev_init(ev_engine_t *e) {
    memset(e, 0, sizeof(*e));
    e->control.volume = .60f;
    e->random = 0x12345678;
    resonance(e);
}

void ev_set_control(ev_engine_t *e, const ev_control_t *c) {
    ev_control_t next = *c;
    if (next.profile >= EV_PROFILES) next.profile = 0;
    next.throttle = clamp(next.throttle, 0, 1);
    next.volume = clamp(next.volume, 0, 1);
    if(next.gear>EV_GEARS) next.gear=EV_GEARS;
    if(next.redline_rpm>0)
        next.redline_rpm=clamp(next.redline_rpm,
                               ev_profiles[next.profile].idle_rpm+500.0f,EV_MAX_RPM);
    next.rpm = clamp(next.rpm, 0, ev_redline(&next));
    if (next.profile != e->control.profile) {
        /* Reset incompatible resonance state under a fresh gain ramp. */
        e->gain = e->envelope = e->resonator1 = e->resonator2 = 0;
        e->lowpass = e->dc_x = e->dc_y = e->cycle = 0;
        e->control = next;
        resonance(e);
    }
    if(next.gear!=e->control.gear) e->shift_samples=(unsigned)(EV_RATE*120/1000);
    e->control = next;
}

const char *ev_phase_name(ev_phase_t phase) {
    static const char *names[]={"OFF","STARTING","IDLE","ACCEL","HOLD","COAST","STOPPING"};
    return (unsigned)phase<sizeof(names)/sizeof(names[0])?names[phase]:"OFF";
}

void ev_render(ev_engine_t *e, int16_t *pcm, size_t count) {
    const ev_profile_t *p = &ev_profiles[e->control.profile];
    const float redline=ev_redline(&e->control);
    const float dt=1.0f/EV_RATE;
    for (size_t i=0; i<count; ++i) {
        /* Time constants are in seconds on both the 16 kHz board and 32 kHz host. */
        if(e->control.running) {
            if(e->start_age<1.0f) e->start_age+=dt;
        } else e->start_age=0;
        float requested_load=e->control.running?e->control.throttle:0;
        e->load+=(requested_load-e->load)*dt/(requested_load>e->load?.085f:.19f);
        float ignition=clamp(e->start_age/.55f,0,1);
        ignition=ignition*ignition*(3-2*ignition);
        float target=e->control.rpm>0?e->control.rpm:
            (p->idle_rpm+e->load*(redline-p->idle_rpm)*gear_rpm_scale[e->control.gear])*ignition;
        if(!e->control.running) target=0;
        /* Carry sub-ULP steps instead of snapping to a slowly moving target:
         * snapping would bypass flywheel inertia on throttle release. */
        float step=(target-e->rpm)*dt/(target>e->rpm?.24f:.48f)+e->rpm_residual;
        float next_rpm=e->rpm+step;
        e->rpm_residual=step-(next_rpm-e->rpm);
        e->rpm=next_rpm;
        float target_gain = e->control.running ? e->control.volume : 0;
        if(e->shift_samples) {
            /* A short ignition cut makes a sequential shift audible while the
             * flywheel response supplies the corresponding RPM fall/rise. */
            target_gain*=.18f;
            --e->shift_samples;
        }
        e->gain += (target_gain-e->gain)*dt/(target_gain>e->gain?.045f:.075f);
        float lift=clamp((e->load-requested_load)*2.5f,0,1);
        e->overrun+=(lift-e->overrun)*dt/(lift>e->overrun?.025f:.32f);
        float delta = e->rpm / (120.0f * EV_RATE);
        float pulse = 0;
        for (unsigned c=0; c<p->cylinders; ++c) {
            float distance = p->firing[c] - e->cycle;
            if (distance <= 0) distance += 1;
            if (distance <= delta && e->rpm > 50 && e->control.running) {
                e->random ^= e->random << 13;
                e->random ^= e->random >> 17;
                e->random ^= e->random << 5;
                pulse += .85f + .15f * (float)(e->random & 65535)/65535;
                ++e->firings;
                e->last_cylinder=p->firing_order[c];
            }
        }
        e->cycle += delta;
        if (e->cycle >= 1) e->cycle -= 1;
        e->envelope = e->envelope*.94f + pulse;
        float res = pulse*.04f + e->resonator_a*e->resonator1 - e->resonator_b*e->resonator2;
        e->resonator2 = e->resonator1;
        e->resonator1 = res;
        e->random ^= e->random << 13;
        e->random ^= e->random >> 17;
        e->random ^= e->random << 5;
        float noise = (float)(e->random & 65535)/32767.5f - 1;
        /* Brief starter whirr fades into combustion; no per-sample trig or heap. */
        e->starter_phase+=(65.0f+80.0f*ignition)*dt;
        if(e->starter_phase>=1) e->starter_phase-=1;
        float starter=(1.0f-fabsf(4.0f*e->starter_phase-2.0f))*.035f;
        float starter_mix=e->control.running && e->control.rpm==0?4*ignition*(1-ignition):0;
        float induction=noise*e->load*e->load*.025f;
        float raw = res*(.6f+e->overrun*.10f) +
            e->envelope*(.3f+noise*(.18f+e->overrun*.08f)) + induction + starter*starter_mix;
        e->lowpass += (.12f + .25f*e->load)*(raw-e->lowpass);
        /* DC blocker, then soft saturation and gain ramp. No heap in render. */
        float y = e->lowpass - e->dc_x + .995f*e->dc_y;
        e->dc_x = e->lowpass;
        e->dc_y = y;
        y *= 1.5f + e->load*1.5f;
        y = y/(1+fabsf(y));
        pcm[i] = (int16_t)(clamp(y*e->gain, -1, 1)*30000);
        ++e->frames;
    }
    if(!e->control.running) e->phase=e->rpm>40?EV_PHASE_STOPPING:EV_PHASE_OFF;
    else if(e->start_age<.55f) e->phase=EV_PHASE_STARTING;
    else if(e->overrun>.035f || (e->control.throttle<.02f && e->rpm>p->idle_rpm+250)) e->phase=EV_PHASE_COAST;
    else if(e->control.rpm==0 && e->control.throttle<.02f) e->phase=EV_PHASE_IDLE;
    else if(e->control.throttle-e->load>.025f ||
            e->rpm<(e->control.rpm>0?e->control.rpm:
                p->idle_rpm+e->control.throttle*(redline-p->idle_rpm)*gear_rpm_scale[e->control.gear])-150)
        e->phase=EV_PHASE_ACCEL;
    else e->phase=EV_PHASE_HOLD;
}
