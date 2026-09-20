#include "engine_voice.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ev_command(ev_control_t *c, const char *line) {
    char cmd[16], arg[32], extra[2];
    int n = sscanf(line, "%15s %31s %1s", cmd, arg, extra);
    ev_control_t next = *c;
    if (n==1) {
        if (!strcmp(cmd,"start")) next.running = true;
        else if (!strcmp(cmd,"stop")) { next.running = false; next.throttle=0; next.rpm=0; }
        else if (!strcmp(cmd,"ping")) return 1;
        else if (!strcmp(cmd,"status")) return 2;
        else if (!strcmp(cmd,"bootloader")) return 3;
        else return -1;
    } else if (n==2) {
        if (!strcmp(cmd,"profile")) {
            unsigned p;
            for (p=0;p<EV_PROFILES;p++) if (!strcmp(arg,ev_profiles[p].name)) break;
            if (p==EV_PROFILES) return -1;
            next.profile=p;
            next.rpm=0;
            next.redline_rpm=0;
        } else if (!strcmp(cmd,"exhaust")) {
            unsigned x;
            for (x=0;x<EV_EXHAUSTS;x++) if (!strcmp(arg,ev_exhausts[x].name)) break;
            if (x==EV_EXHAUSTS) return -1;
            next.exhaust=x;
        } else if (!strcmp(cmd,"gear")) {
            if(!strcmp(arg,"N") || !strcmp(arg,"n")) next.gear=0;
            else {
                char *end;
                long gear=strtol(arg,&end,10);
                if(end==arg || *end || gear<0 || gear>EV_GEARS) return -1;
                next.gear=(unsigned)gear;
            }
        } else if (!strcmp(cmd,"redline")) {
            if(!strcmp(arg,"default")) next.redline_rpm=0;
            else {
                char *end;
                float rpm=strtof(arg,&end);
                float minimum=ev_profiles[next.profile].idle_rpm+500.0f;
                if(end==arg || *end || !isfinite(rpm) || rpm<minimum || rpm>EV_MAX_RPM) return -1;
                next.redline_rpm=rpm;
                if(next.rpm>rpm) next.rpm=rpm;
            }
        } else {
            char *end;
            float v=strtof(arg,&end);
            if (end==arg || *end || !isfinite(v) || v<0) return -1;
            if (!strcmp(cmd,"throttle") && v<=100) { next.throttle=v/100; next.rpm=0; }
            else if (!strcmp(cmd,"volume") && v<=100) next.volume=v/100;
            else if (!strcmp(cmd,"rpm") && v<=ev_redline(&next)) next.rpm=v;
            else return -1;
        }
    } else return -1;
    *c=next;
    return 0;
}
