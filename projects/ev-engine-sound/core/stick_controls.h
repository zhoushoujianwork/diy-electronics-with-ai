#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { EV_STICK_MAIN, EV_STICK_VOLUME, EV_STICK_REDLINE } ev_stick_page_t;
enum {
    EV_KEY_REV_ON=1, EV_KEY_REV_OFF=2, EV_KEY_NEXT=4, EV_KEY_STOP=8,
    EV_KEY_MINUS=16, EV_KEY_PLUS=32, EV_KEY_PAGE=64
};
typedef struct {
    bool raw[2], down[2], long_b, blocked, rev;
    uint32_t changed[2], b_since;
    ev_stick_page_t page;
} ev_stick_controls_t;
/* Poll every 10ms. 30ms debounce, 700ms B hold; unsigned clocks may wrap.
 * B hold cycles MAIN -> VOLUME -> REDLINE -> MAIN. In settings A-/B+.
 * A+B immediately releases throttle and requests stop, latching until both up. */
unsigned ev_stick_poll(ev_stick_controls_t *s,uint32_t now,bool a,bool b);
