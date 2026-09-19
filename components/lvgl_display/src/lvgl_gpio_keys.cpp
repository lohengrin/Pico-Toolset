#include "pico_toolset/lvgl_gpio_keys.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include <cstdio>

namespace pico_toolset {

namespace {
LvglGpioKeysConfig g_config;
bool g_active_low = true;
bool g_auto = false;
struct Idle { bool up = false, down = false; } g_idle[8];

void read_pair(uint pin, bool& up, bool& down) {
    gpio_pull_up(pin);
    sleep_us(50);
    up = gpio_get(pin);
    gpio_pull_down(pin);
    sleep_us(50);
    down = gpio_get(pin);
}

void keypad_read_cb(lv_indev_t*, lv_indev_data_t* data) {
    data->key = 0;
    data->state = LV_INDEV_STATE_RELEASED;
    for (size_t i = 0; i < g_config.key_count; ++i) {
        bool pressed;
        if (g_auto) {
            bool up, down;
            read_pair(g_config.keys[i].pin, up, down);
            pressed = up != g_idle[i].up || down != g_idle[i].down;
        } else {
            pressed = gpio_get(g_config.keys[i].pin) != g_active_low;
        }
        if (pressed) {
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
    }

    g_auto = config.polarity == LvglGpioKeyPolarity::kAuto && config.key_count <= 8;
    g_active_low = config.polarity != LvglGpioKeyPolarity::kActiveHigh;
    if (g_auto) {
        for (size_t i = 0; i < config.key_count; ++i) {
            read_pair(config.keys[i].pin, g_idle[i].up, g_idle[i].down);
            printf("gpio keys: GP%u idle (pull-up, pull-down) = (%d, %d)\n", config.keys[i].pin, g_idle[i].up,
                   g_idle[i].down);
        }
    } else {
        for (size_t i = 0; i < config.key_count; ++i) {
            if (g_active_low) gpio_pull_up(config.keys[i].pin); else gpio_pull_down(config.keys[i].pin);
        }
    }

    lv_group_t* group = lv_group_create();
    lv_group_set_default(group);
    lv_indev_t* keypad = lv_indev_create();
    lv_indev_set_type(keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(keypad, keypad_read_cb);
    lv_indev_set_group(keypad, group);
}

} // namespace pico_toolset
