#include <assert.h>
#include "controls.h"
int main(void) {
    boot_button_t b={0};
    assert(boot_button_update(&b,false,0)==0);
    assert(boot_button_update(&b,true,10)==0);
    assert(boot_button_update(&b,false,15)==0);
    assert(boot_button_update(&b,true,20)==0);
    assert(boot_button_update(&b,true,49)==0);
    assert(boot_button_update(&b,true,50)==1);
    assert(b.pressed);
    assert(boot_button_update(&b,true,5000)==0);
    assert(boot_button_update(&b,false,5010)==0);
    assert(boot_button_update(&b,true,5020)==0);
    assert(boot_button_update(&b,false,5030)==0);
    assert(boot_button_update(&b,false,5060)==-1);
    assert(!b.pressed);
    assert(boot_button_update(&b,false,10000)==0);
    /* Long uptimes must not wrap after the 32-bit millisecond boundary. */
    uint64_t t=UINT64_C(1)<<32;
    assert(boot_button_update(&b,true,t+1)==0);
    assert(boot_button_update(&b,true,t+31)==1);
    return 0;
}
