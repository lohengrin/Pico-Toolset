#pragma once

#include "lvgl.h"

namespace pico_toolset {

// Scrolls the scrollable object under `point` (a list, a text area, ...) by `notches` mouse-wheel
// steps of `step_px` pixels each; positive = wheel up = the content moves down (shows what is above).
// The top layer is searched first (an overlay or page shown over the screen), then the active screen.
// Returns true when something was scrolled. Does nothing if the pointer is not over a scrollable
// object that has room to scroll.
bool lvgl_scroll_under(lv_point_t point, int notches, int step_px = 60);

} // namespace pico_toolset
