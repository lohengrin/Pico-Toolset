#include "pico_toolset/reset_buttons.h"

#include "hardware/gpio.h"
#include "hardware/structs/watchdog.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

#include <algorithm>

namespace pico_toolset {

namespace {
// Can't arise from a plain power-on (scratch resets to 0), so
// consume_pending_watchdog_tag() uses it to tell "tagged reboot" from
// "cold power-on".
constexpr uint32_t kRebootMagic = 0xB00110C0;
} // namespace

bool DebouncedButtons::init(std::span<const uint8_t> pins, int debounce_frames) {
    if (pins.size() > static_cast<size_t>(kMaxButtons)) return false;

    m_count = static_cast<int>(pins.size());
    m_debounce_frames = debounce_frames;
    for (int i = 0; i < m_count; ++i) {
        m_pins[i] = pins[i];
        m_press_streak[i] = 0;
        gpio_init(m_pins[i]);
        gpio_set_dir(m_pins[i], GPIO_IN);
        gpio_pull_down(m_pins[i]); // active-HIGH: closes to 3V3, not GND
    }
    return true;
}

int DebouncedButtons::poll() {
    for (int i = 0; i < m_count; ++i) {
        bool pressed = gpio_get(m_pins[i]); // active-high
        m_press_streak[i] = pressed ? m_press_streak[i] + 1 : 0;
        if (m_press_streak[i] == m_debounce_frames) {
            return i; // fires once: the streak keeps growing past this on later polls
        }
    }
    return -1;
}

std::string DebouncedButtons::raw_state() const {
    std::string state;
    for (int i = 0; i < m_count; ++i) {
        state += gpio_get(m_pins[i]) ? '1' : '0'; // active-high: '1' = pressed
    }
    return state;
}

void watchdog_reboot_with_tag(uint32_t tag) {
    watchdog_hw->scratch[0] = kRebootMagic;
    watchdog_hw->scratch[1] = tag;
    watchdog_reboot(0, 0, 0);
    while (true) {
        tight_loop_contents(); // wait for reboot to take effect
    }
}

bool consume_pending_watchdog_tag(uint32_t& out_tag) {
    if (watchdog_hw->scratch[0] != kRebootMagic) return false;
    watchdog_hw->scratch[0] = 0; // clear: a later plain power-cycle reads back false
    out_tag = watchdog_hw->scratch[1];
    return true;
}

} // namespace pico_toolset
