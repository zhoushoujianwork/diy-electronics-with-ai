#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { bool raw, pressed; uint64_t changed_ms; } boot_button_t;
/* Returns +1 press, -1 release, 0 no debounced edge. */
int boot_button_update(boot_button_t *b, bool low, uint64_t now_ms);
