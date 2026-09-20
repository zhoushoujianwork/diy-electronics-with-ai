#pragma once
#include "engine_voice.h"
#include <math.h>

/* cycle 0..1 is a slow-motion 720-degree cycle; firing is compression TDC.
 * Unit crank radius and a 3.5-radius rod maintain a fixed connecting-rod length. */
typedef struct { float x, y, piston, stroke; unsigned stage; float cycle; } ev_motion_t;
static inline ev_motion_t ev_motion(const ev_profile_t *p,unsigned cylinder,float cycle) {
    float firing=0;
    for(unsigned i=0;i<p->cylinders;i++)
        if(p->firing_order[i]==cylinder) { firing=p->firing[i]; break; }
    float local=cycle-firing;
    local-=floorf(local);
    float angle=local*12.566370614f;
    float x=sinf(angle),y=cosf(angle);
    float piston=y+sqrtf(12.25f-x*x);
    return (ev_motion_t){x,y,piston,(4.5f-piston)*.5f,(unsigned)(local*4),local};
}
