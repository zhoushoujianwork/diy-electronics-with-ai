#pragma once

#include <stdbool.h>
#include <stdint.h>

#define EV_POWERTRAIN_WIDTH 320
#define EV_POWERTRAIN_HEIGHT 108

typedef struct {
    unsigned profile;
    unsigned cylinders;
    unsigned exhaust;
    unsigned last_cylinder;
    bool running;
    float throttle; /* 0..1, shown by the separate twist grip */
    float phase; /* 0..1, visual wheel/crank phase */
} ev_powertrain_state_t;

/* Render the LCKFB home-page engine, exhaust and throttle rig into a tightly packed RGB565 canvas.
 * The renderer is deterministic, allocation-free and shared by firmware and
 * the desktop preview tool. */
void ev_powertrain_render(uint16_t *pixels,const ev_powertrain_state_t *state);
