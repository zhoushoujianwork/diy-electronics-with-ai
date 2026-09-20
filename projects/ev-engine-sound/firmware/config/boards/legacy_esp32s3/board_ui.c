#include "board_ui.h"

esp_err_t board_ui_init(i2c_master_bus_handle_t shared_i2c,
                        board_ui_action_cb_t action_cb,
                        board_ui_state_cb_t state_cb,
                        void *context) {
    (void)shared_i2c;
    (void)action_cb;
    (void)state_cb;
    (void)context;
    return ESP_ERR_NOT_SUPPORTED;
}

unsigned board_ui_stack_high_water_mark(void) {
    return 0;
}

esp_err_t board_ui_report(void) {
    return ESP_ERR_NOT_SUPPORTED;
}
