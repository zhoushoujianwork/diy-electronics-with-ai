#include "engine_voice.h"
#include "motorcycle_canvas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void rgb565_to_rgb(uint16_t value,unsigned char out[3]) {
    out[0]=(unsigned char)(((value>>11)&31)*255/31);
    out[1]=(unsigned char)(((value>>5)&63)*255/63);
    out[2]=(unsigned char)((value&31)*255/31);
}

static int write_scaled_ppm(const char *path,const uint16_t *pixels,int scale) {
    FILE *file=fopen(path,"wb");
    if(!file) return 1;
    fprintf(file,"P6\n%d %d\n255\n",EV_MOTORCYCLE_WIDTH*scale,EV_MOTORCYCLE_HEIGHT*scale);
    for(int y=0;y<EV_MOTORCYCLE_HEIGHT;y++) for(int sy=0;sy<scale;sy++)
        for(int x=0;x<EV_MOTORCYCLE_WIDTH;x++) {
            unsigned char rgb[3];
            rgb565_to_rgb(pixels[y*EV_MOTORCYCLE_WIDTH+x],rgb);
            for(int sx=0;sx<scale;sx++) fwrite(rgb,1,sizeof(rgb),file);
        }
    return fclose(file)!=0;
}

int main(int argc,char **argv) {
    const char *directory=argc>1?argv[1]:".";
    uint16_t *pixels=calloc(EV_MOTORCYCLE_WIDTH*EV_MOTORCYCLE_HEIGHT,sizeof(*pixels));
    if(!pixels) return 1;
    int failed=0;
    for(unsigned exhaust=0;exhaust<EV_EXHAUSTS;exhaust++) {
        ev_motorcycle_state_t state={
            .profile=1,
            .cylinders=ev_profiles[1].cylinders,
            .exhaust=exhaust,
            .last_cylinder=1,
            .running=true,
            .phase=.18f
        };
        ev_motorcycle_render(pixels,&state);
        char path[512];
        snprintf(path,sizeof(path),"%s/motorcycle-%s.ppm",directory,ev_exhausts[exhaust].name);
        if(write_scaled_ppm(path,pixels,3)) {
            fprintf(stderr,"failed to write %s\n",path);
            failed=1;
        } else printf("wrote %s\n",path);
    }
    free(pixels);
    return failed;
}
