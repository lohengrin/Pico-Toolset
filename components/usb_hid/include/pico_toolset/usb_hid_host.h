#pragma once

#include <cstdint>
#include <cstddef>

#include "usb_hid_gamepad.h"
#include "usb_hid_keymap.h"

#include "pico/stdlib.h"

namespace pico_toolset {

// Configuration for the PIO-USB HID host. pin_dp/pio_num/run_on_core1 are
// board+use-case wiring (which pin is D+, which PIO block is free, whether
// core1 is available or already owned by e.g. a DVI driver) with no working
// default -- see usb_hid_configs.h for known-good presets (e.g.
// configs::usb_hid::kWaveshareRp2350PiZeroLcd), or set them yourself for a board/use
// case without one yet.
struct UsbHidConfig {
    uint8_t pin_dp;                   // PIO-USB D+ pin (D- = D+1) -- must be set
    uint     pio_num;                 // PIO block (keep free of other PIO users) -- must be set
    bool     run_on_core1;            // Dedicate core1 to the USB host stack -- must be set (false if core1 is already owned elsewhere, e.g. a DVI driver)
    uint32_t core1_stack_size = 4096; // Core1 stack in words (16 KB)
    uint8_t  rhport = 1;              // TinyUSB root hub port for PIO-USB (1)
    uint8_t  max_hid_interfaces = 4;  // CFG_TUH_HID
    uint8_t  max_devices = 4;         // CFG_TUH_DEVICE_MAX
    bool     enable_keyboard = true;
    bool     enable_mouse = true;
    bool     enable_gamepad = true;   // HID gamepads
    bool     enable_xinput = true;    // XInput pads (merged into GamepadState)
    uint8_t  max_gamepads = 4;        // Gamepad slots
    // Absolute mouse cursor is clamped to [0, mouse_max_x] x [0, mouse_max_y]
    // -- set these to the consuming app's own canonical cursor space (e.g.
    // its display resolution minus 1). Defaults match TOM6809's 640x480
    // canonical space.
    int      mouse_max_x = 639;
    int      mouse_max_y = 479;
    // Layout used by consume_typed_ascii_char(): an index into kKeymaps.
    // kDefaultKeymapIndex (PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP) by default;
    // pick any compiled-in layout at runtime, e.g. with keymap_by_name().
    uint8_t keymap_index = kDefaultKeymapIndex;
    // Keyboard LED/lock-state management -- generic behavioral options (no
    // board wiring involved), see UsbHidHost's own doc comment. NumLock is
    // considered "on" until physically toggled -- there is no USB-HID way
    // to query a freshly-mounted keyboard's actual lock-key state, so this
    // is this host's own convention (matching most desktop OSes' default).
    bool numlock_initial_state = true;
    // Plays a NumLock->CapsLock->ScrollLock->CapsLock->NumLock->off identify
    // sequence on the keyboard's own LEDs when it mounts, before settling
    // into the real, managed state. Purely cosmetic -- set false to skip
    // straight to the managed state.
    bool led_boot_animation = true;
};

// USB HID host over a Pico-PIO-USB port. Runs the TinyUSB host stack on a
// dedicated core (core1 by default) and exposes level-triggered keyboard
// state, an absolute mouse cursor, and merged HID/XInput gamepad states.
//
// The host stack is timing-critical (1 ms SOF cadence) -- do NOT poll the
// stack from a busy emulation/render loop; dedicate a core (default) or, on
// builds where core1 belongs to DVI, keep run_on_core1=false and call task()
// from core0's loop.
class UsbHidHost {
public:
    // Fixed-size state constants (used by the app-facing API below).
    static constexpr uint16_t kMaxKeyUsages = 256;
    static constexpr uint8_t kAsciiQueueSize = 32;
    static constexpr uint8_t kMaxGamepadSlots = 4;

    // Bring up the TinyUSB host stack per `config`.
    bool init(const UsbHidConfig& config);

    // One non-blocking tuh_task() poll. MUST be called from the same core
    // that init() brought the stack up on. When run_on_core1, core1 calls
    // this in its own loop automatically -- this method is only needed for
    // core0 (HDMI/DVI) builds.
    static void task();

    // --- Keyboard (level-triggered, double-buffered, cross-core safe) ---
    bool is_key_down(uint8_t hid_usage_id) const;
    bool is_modifier_down(uint8_t mask) const;

    // Edge-triggered: true once per physical press/release edge.
    bool consume_key_press(uint8_t hid_usage_id);
    // One typed ASCII character per physical press, in the config's keymap
    // layout (UsbHidConfig::keymap_index, see usb_hid_keymap.h); 0 if none.
    uint8_t consume_typed_ascii_char();

    // --- Keyboard lock-key state (NumLock/CapsLock/ScrollLock) ---
    // Toggled on each physical press of the corresponding key and pushed to
    // every mounted keyboard's own LEDs (see UsbHidConfig::numlock_initial_state/
    // led_boot_animation) -- reading these is the only way a consumer can
    // know NumLock's state, since USB HID gives no other way to query it.
    bool numlock_on() const { return m_numlock_on; }
    bool capslock_on() const { return m_capslock_on; }
    bool scrolllock_on() const { return m_scrolllock_on; }

    // --- Mouse ---
    struct MouseState {
        bool present = false;
        int x = 0, y = 0;            // clamped absolute cursor
        bool left_button = false;
        bool right_button = false;
    };
    MouseState mouse_state() const;

    // --- Gamepad (HID + XInput merged) ---
    GamepadState gamepad_state(size_t index) const;

    // --- Device counts (diagnostics) ---
    uint8_t connected_keyboard_count() const;
    uint8_t connected_gamepad_count() const;

    // Internal -- called by the free TinyUSB callbacks in the .cpp. Runs on
    // the core owning the stack.
    static UsbHidHost* instance() { return s_instance; }
    void on_mount(uint8_t dev_addr, uint8_t instance, bool is_keyboard, bool is_joystick_like, bool is_mouse,
                  bool is_dualsense = false);
    void on_hid_unmount(uint8_t dev_addr, uint8_t instance, bool is_keyboard);
    void on_keyboard_report(const uint8_t* report, uint16_t len);
    void on_mouse_report(const uint8_t* report, uint16_t len);
    void on_gamepad_report(const uint8_t* report, uint16_t len, uint8_t dev_addr, uint8_t instance);
    void on_xinput_mount(uint8_t dev_addr);
    void on_xinput_report(uint8_t dev_addr, const uint8_t* report, uint16_t len);
    void on_xinput_unmount(uint8_t dev_addr);

    // Whether dev_addr/instance is the currently-tracked mouse -- used by
    // tuh_hid_report_received_cb() to route a mouse's reports regardless of
    // its declared itf_protocol (see that callback's own doc comment).
    bool matches_mouse(uint8_t dev_addr, uint8_t instance) const {
        return m_mouse.present && m_mouse_dev_addr == dev_addr && m_mouse_instance == instance;
    }

private:
    struct HidLayout {
        uint8_t x_byte = 0, y_byte = 1, button_byte = 2, fire_bit = 0;
        bool valid = false;
    };

    struct GamepadSlot {
        bool in_use = false;
        bool is_xinput = false;
        bool is_dualsense = false;
        uint8_t dev_addr = 0;
        uint8_t instance = 0;
        HidLayout layout{};
        GamepadState state{};
        size_t mapped_index = 0; // index into gamepad_state()
    };

    int allocate_gamepad_slot(uint8_t dev_addr, uint8_t instance, bool is_xinput, bool is_dualsense = false);

    void push_typed_ascii(uint8_t ch);

    static void host_stack_setup();
    static void core1_entry();

    // --- Keyboard LED/lock-state management (usb_hid_host.cpp) ---
    // Animation steps for the boot identify sequence, in playback order;
    // Done means "not animating" (steady managed state).
    enum class LedAnimStep : uint8_t { NumLock1, CapsLock1, ScrollLock, CapsLock2, NumLock2, Off, Settle, Done };
    uint8_t current_led_mask() const;
    void apply_led_mask(uint8_t mask); // no-op if unchanged since the last call
    void start_led_animation();
    void update_leds(); // called every task() tick

    static UsbHidHost* s_instance;

    UsbHidConfig m_config{};
    bool m_initialized = false;

    // Double-buffered key state (core1 writes / core0 reads).
    uint8_t m_key_state[2][kMaxKeyUsages / 8]{};
    volatile uint8_t m_key_state_active = 0;
    volatile uint8_t m_modifiers[2]{};
    volatile uint8_t m_modifiers_active = 0;
    volatile uint8_t m_keyboard_count = 0;

    // Edge-triggered key press queue (consume_key_press).
    uint8_t m_press_edges[2][kMaxKeyUsages / 8]{};
    volatile uint8_t m_press_edges_active = 0;
    uint8_t m_prev_key_held[kMaxKeyUsages / 8]{};

    // Typed-ASCII ring buffer.
    uint8_t m_ascii_queue[kAsciiQueueSize]{};
    volatile uint8_t m_ascii_head = 0;
    volatile uint8_t m_ascii_tail = 0;
    uint8_t m_prev_keycode[6]{};

    MouseState m_mouse{};
    uint8_t m_mouse_dev_addr = 0;
    uint8_t m_mouse_instance = 0;

    GamepadSlot m_gamepads[4]{};
    uint8_t m_gamepad_count = 0;

    // Keyboard lock-key state + LED animation (see usb_hid_host.cpp).
    bool m_numlock_on = true;
    bool m_capslock_on = false;
    bool m_scrolllock_on = false;
    LedAnimStep m_led_anim_step = LedAnimStep::Done;
    uint64_t m_led_anim_next_us = 0;
    uint8_t m_led_mask_sent = 0xFF; // sentinel -- no real mask has this value
};

} // namespace pico_toolset
