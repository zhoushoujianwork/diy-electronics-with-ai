#include "engine_voice.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void u16(FILE *f, uint16_t v) { fputc(v&255,f); fputc(v>>8,f); }
static void u32(FILE *f, uint32_t v) { u16(f,v&65535); u16(f,v>>16); }
int main(int argc, char **argv) {
    if(argc!=3) {
        fprintf(stderr,"Usage: %s PROFILE output.wav\nProfiles:",argv[0]);
        for(unsigned i=0;i<EV_PROFILES;i++) fprintf(stderr," %s",ev_profiles[i].name);
        fputc('\n',stderr);
        return 2;
    }
    ev_engine_t e; ev_init(&e);
    char command[80]; snprintf(command,sizeof(command),"profile %s",argv[1]);
    if(ev_command(&e.control,command)!=0) return 2;
    ev_control_t control=e.control;
    ev_init(&e); ev_set_control(&e,&control);
    FILE *f=fopen(argv[2],"wb"); if(!f) { perror("fopen"); return 1; }
    const unsigned total=EV_RATE*10;
    fwrite("RIFF",1,4,f); u32(f,36+total*2); fwrite("WAVEfmt ",1,8,f);
    u32(f,16); u16(f,1); u16(f,1); u32(f,EV_RATE); u32(f,EV_RATE*2);
    u16(f,2); u16(f,16); fwrite("data",1,4,f); u32(f,total*2);
    int16_t pcm[EV_BLOCK];
    for(unsigned frame=0;frame<total;frame+=EV_BLOCK) {
        float t=(float)frame/EV_RATE;
        control.running=t<9;
        control.volume=.6f;
        control.throttle=t<2 ? 0 : t<6 ? (t-2)/4 : t<7 ? 1 : 0;
        ev_set_control(&e,&control);
        unsigned count=total-frame<EV_BLOCK?total-frame:EV_BLOCK;
        ev_render(&e,pcm,count);
        for(unsigned i=0;i<count;i++) u16(f,(uint16_t)pcm[i]);
    }
    int failed=ferror(f); if(fclose(f)) failed=1;
    if(failed) { perror("write WAV"); return 1; }
    printf("%s: 10 s, %u Hz, mono PCM16, firings=%llu -> %s\n",argv[1],EV_RATE,(unsigned long long)e.firings,argv[2]);
    return 0;
}
