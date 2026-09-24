#include "controls.h"
int boot_button_update(boot_button_t *b, bool low, uint64_t now_ms) {
    if (low != b->raw) { b->raw = low; b->changed_ms = now_ms; }
    if (b->pressed != b->raw && now_ms - b->changed_ms >= 30) {
        b->pressed = b->raw;
        return b->pressed ? 1 : -1;
    }
    return 0;
}
