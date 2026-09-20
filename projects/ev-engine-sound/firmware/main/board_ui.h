#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "engine_voice.h"

typedef enum {
    BOARD_UI_PROFILE,
    BOARD_UI_ENGINE_TOGGLE,
    BOARD_UI_VOLUME_DELTA,
    BOARD_UI_REDLINE_DELTA,
    BOARD_UI_REDLINE_SET,
    BOARD_UI_REV_PRESS,
    BOARD_UI_REV_RELEASE,
    BOARD_UI_ENGINE_STOP,
} board_ui_action_t;

typedef struct {
    unsigned profile;
    unsigned redline_rpm;
    float rpm;
    float throttle;
    float volume;
    unsigned gear;
    bool running;
    bool fault;
    uint64_t firings;
    ev_phase_t phase;
    float load;
    unsigned last_cylinder;
} board_ui_state_t;

typedef void (*board_ui_action_cb_t)(board_ui_action_t action,int value,void *context);
typedef bool (*board_ui_state_cb_t)(board_ui_state_t *state,void *context);

esp_err_t board_ui_init(i2c_master_bus_handle_t shared_i2c,
                        board_ui_action_cb_t action_cb,
                        board_ui_state_cb_t state_cb,
                        void *context);
unsigned board_ui_stack_high_water_mark(void);
esp_err_t board_ui_report(void);
