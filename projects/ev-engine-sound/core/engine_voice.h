#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef ESP_PLATFORM
#include "board_config.h"
#define EV_RATE EV_AUDIO_SAMPLE_RATE
#elif !defined(EV_RATE)
#define EV_RATE 32000
#endif
#define EV_BLOCK 256
#define EV_MAX_CYLINDERS 12
#define EV_PROFILES 18
#define EV_EXHAUSTS 5
#define EV_GEARS 6
#define EV_MAX_RPM 16000

typedef struct {
    const char *name;
    const char *ui_name;
    unsigned cylinders;
    float firing[EV_MAX_CYLINDERS]; /* positions in a four-stroke, 720-degree cycle */
    uint8_t firing_order[EV_MAX_CYLINDERS]; /* cylinder index for each timed firing event */
    float idle_rpm, redline_rpm, resonance_hz;
} ev_profile_t;
extern const ev_profile_t ev_profiles[EV_PROFILES];

typedef struct {
    const char *name;
    const char *ui_name;
    float resonance_scale, resonance_decay;
    float resonator_mix, pulse_mix, noise_mix, induction_mix;
    float filter_idle, filter_load, drive, overrun_mix;
} ev_exhaust_t;
extern const ev_exhaust_t ev_exhausts[EV_EXHAUSTS];

typedef enum {
    EV_PHASE_OFF, EV_PHASE_STARTING, EV_PHASE_IDLE, EV_PHASE_ACCEL,
    EV_PHASE_HOLD, EV_PHASE_COAST, EV_PHASE_STOPPING
} ev_phase_t;
const char *ev_phase_name(ev_phase_t phase);

typedef struct {
    bool running;
    unsigned profile;
    unsigned exhaust;
    float throttle; /* 0..1 */
    float volume;   /* 0..1 */
    float rpm;      /* 0 = throttle model, otherwise requested RPM */
    unsigned gear;  /* 0 = neutral, 1..EV_GEARS = sequential gearbox */
    float redline_rpm; /* 0 = profile default, otherwise adjustable limit */
} ev_control_t;

typedef struct {
    ev_control_t control;
    float rpm, cycle, gain, load, envelope;
    float resonator1, resonator2, resonator_a, resonator_b;
    float lowpass, dc_x, dc_y;
    float start_age, starter_phase, overrun, rpm_residual;
    ev_phase_t phase;
    unsigned last_cylinder;
    uint32_t random;
    unsigned shift_samples;
    uint64_t frames, firings;
} ev_engine_t;

void ev_init(ev_engine_t *e);
void ev_set_control(ev_engine_t *e, const ev_control_t *control);
void ev_render(ev_engine_t *e, int16_t *pcm, size_t count);
float ev_redline(const ev_control_t *control);
/* -1 invalid, 0 state change, 1 ping, 2 status, 3 bootloader request. */
int ev_command(ev_control_t *c, const char *line);
