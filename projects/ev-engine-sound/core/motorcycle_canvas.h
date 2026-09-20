#pragma once

#include <stdbool.h>
#include <stdint.h>

#define EV_MOTORCYCLE_WIDTH 320
#define EV_MOTORCYCLE_HEIGHT 108

typedef struct {
    unsigned profile;
    unsigned cylinders;
    unsigned exhaust;
    unsigned last_cylinder;
    bool running;
    float phase; /* 0..1, visual wheel/crank phase */
} ev_motorcycle_state_t;

/* Render the LCKFB home-page motorcycle into a tightly packed RGB565 canvas.
 * The renderer is deterministic, allocation-free and shared by firmware and
 * the desktop preview tool. */
void ev_motorcycle_render(uint16_t *pixels,const ev_motorcycle_state_t *state);

