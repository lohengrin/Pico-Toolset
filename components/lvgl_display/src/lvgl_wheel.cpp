#include "pico_toolset/lvgl_wheel.h"

namespace pico_toolset {

namespace {

// The first scrollable ancestor (or the object itself) of what is hit at `point` below `root`.
lv_obj_t* scrollable_below(lv_obj_t* root, lv_point_t point) {
    for (lv_obj_t* o = lv_indev_search_obj(root, &point); o; o = lv_obj_get_parent(o)) {
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_SCROLLABLE) && (lv_obj_get_scroll_top(o) > 0 || lv_obj_get_scroll_bottom(o) > 0))
            return o;
    }
    return nullptr;
}

} // namespace

bool lvgl_scroll_under(lv_point_t point, int notches, int step_px) {
    if (notches == 0) return false;
    lv_display_t* display = lv_display_get_default();
    lv_obj_t* const roots[] = {lv_display_get_layer_top(display), lv_display_get_screen_active(display)};
    for (lv_obj_t* root : roots) {
        if (lv_obj_t* list = scrollable_below(root, point)) {
            lv_obj_scroll_by_bounded(list, 0, notches * step_px, LV_ANIM_OFF);
            return true;
        }
    }
    return false;
}

} // namespace pico_toolset
