#include "pico_toolset/lvgl_gpio_keys.h"

#include "hardware/gpio.h"

namespace pico_toolset {

namespace {
LvglGpioKeysConfig g_config;

void keypad_read_cb(lv_indev_t*, lv_indev_data_t* data) {
    data->key = 0;
    data->state = LV_INDEV_STATE_RELEASED;
    for (size_t i = 0; i < g_config.key_count; ++i) {
        const bool level = gpio_get(g_config.keys[i].pin);
        if (level != g_config.active_low) {
            data->key = g_config.keys[i].lv_key;
            data->state = LV_INDEV_STATE_PRESSED;
            return;
        }
    }
}
} // namespace

void lvgl_gpio_keys_init(const LvglGpioKeysConfig& config) {
    g_config = config;
    for (size_t i = 0; i < config.key_count; ++i) {
        gpio_init(config.keys[i].pin);
        gpio_set_dir(config.keys[i].pin, GPIO_IN);
        if (config.active_low) gpio_pull_up(config.keys[i].pin); else gpio_pull_down(config.keys[i].pin);
    }
    lv_group_t* group = lv_group_create();
    lv_group_set_default(group);
    lv_indev_t* keypad = lv_indev_create();
    lv_indev_set_type(keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(keypad, keypad_read_cb);
    lv_indev_set_group(keypad, group);
}

} // namespace pico_toolset
