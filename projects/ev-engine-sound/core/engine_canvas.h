#pragma once
#include <stdbool.h>
#include <stdint.h>

/* RGB565, tightly packed; caller owns width*height pixels. 1..320 x 1..116.
 * Cycle is a slow-motion 720-degree cycle. Reentrant, no allocation or LVGL dependency. */
void ev_canvas_render(uint16_t *pixels,unsigned width,unsigned height,
                      unsigned profile,float cycle,bool running);
