#include "pico_toolset/usb_hid_host.h"
#include "pico_toolset/usb_hid_keymap.h"

#include "hardware/dma.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "pio_usb.h"
#include "tusb.h"
#include "class/hid/hid.h"
#include "host/usbh_pvt.h"

#include <cstdio>
#include <cstring>

namespace pico_toolset {

namespace {

constexpr uint8_t kTuhRhport = 1; // see tusb_config.h

// --- HID report-descriptor classification (see TOM6809) ---
// Minimal structurally-correct HID item walk: prefix bits7-4 = tag, bits3-2 =
// type (0 Main / 1 Global / 2 Local / 3 res), bits1-0 = size code (3 = 4
// bytes). Returns true only when a top-level Collection(Application) carries
// the requested (page, usage) -- e.g. (Generic Desktop, Gamepad 0x05).
bool looks_like_hid_application(const uint8_t* desc, uint16_t len, uint16_t target_usage_page,
                                uint16_t target_usage) {
    uint16_t usage_page = 0;
    uint16_t pending_usage = 0;
    for (uint16_t i = 0; i < len;) {
        uint8_t prefix = desc[i];
        if (prefix == 0xFE) { // long item (rare) -- skip
            if (static_cast<uint16_t>(i + 1) >= len) break;
            i = static_cast<uint16_t>(i + 3 + desc[i + 1]);
            continue;
        }
        uint8_t size_code = prefix & 0x03;
        uint8_t size = (size_code == 3) ? 4 : size_code;
        uint8_t type = (prefix >> 2) & 0x03;
        uint8_t tag = (prefix >> 4) & 0x0F;
        if (static_cast<uint16_t>(i + 1 + size) > len) break;

        uint32_t data = 0;
        for (uint8_t b = 0; b < size; ++b)
            data |= static_cast<uint32_t>(desc[i + 1 + b]) << (8 * b);

        if (type == 1 && tag == 0x0) {
            usage_page = static_cast<uint16_t>(data);
        } else if (type == 2 && tag == 0x0) {
            pending_usage = static_cast<uint16_t>(data);
        } else if (type == 0 && tag == 0xA) {
            if (data == 0x01 /* Application */ && usage_page == target_usage_page &&
                pending_usage == target_usage) {
                return true;
            }
        }
        if (type == 0) {
            pending_usage = 0; // Main items clear local state (HID 6.2.2.8)
        }
        i = static_cast<uint16_t>(i + 1 + size);
    }
    return false;
}

bool looks_like_joystick_report_descriptor(const uint8_t* desc, uint16_t len) {
    return looks_like_hid_application(desc, len, 0x01, 0x05); // Generic Desktop, Gamepad
}
bool looks_like_mouse_report_descriptor(const uint8_t* desc, uint16_t len) {
    return looks_like_hid_application(desc, len, 0x01, 0x02); // Generic Desktop, Mouse
}

// Boot-Protocol keyboard keypress [] -> bitmasks used by the double-buffered
// key state.
inline void set_bit(uint8_t* bits, const uint8_t* usages, uint8_t n) {
    for (uint8_t i = 0; i < n; ++i) {
        if (usages[i] < UsbHidHost::kMaxKeyUsages) bits[usages[i] >> 3] |= 1u << (usages[i] & 7);
    }
}

// Sony's DualSense (PS5 controller) is a genuine USB HID gamepad, but its
// report descriptor doesn't reliably pass looks_like_joystick_report_descriptor()
// on real hardware (TOM6809 finding) -- detected by VID/PID instead. Its
// report layout is nothing like the generic byte0=X/byte1=Y/byte2-bit0=fire
// fallback: reports are report-ID-prefixed, the D-pad is a 4-bit hat switch,
// and face buttons sit in specific bit positions -- cross-checked against
// Linux's hid-playstation.c and community reverse-engineering of the
// DualSense USB format.
constexpr uint16_t kSonyVid = 0x054C;
constexpr uint16_t kDualSenseProductId = 0x0CE6;     // DualSense (PS5 controller)
constexpr uint16_t kDualSenseEdgeProductId = 0x0DF2; // DualSense Edge

bool is_dualsense_vid_pid(uint16_t vid, uint16_t pid) {
    return vid == kSonyVid && (pid == kDualSenseProductId || pid == kDualSenseEdgeProductId);
}

// Parses one DualSense USB input report (report ID 0x01, included since the
// device is a numbered-report HID collection) into the same GamepadState
// shape the generic/XInput paths produce. Byte offsets cross-checked against
// Linux's hid-playstation.c: byte1=left X, byte2=left Y (both 0-255, centered
// ~128), byte8 low nibble=D-pad hat (0=N,1=NE,2=E,3=SE,4=S,5=SW,6=W,7=NW,
// 8=released), byte8 bit5=Cross (mapped to kBtA -- the primary face button).
GamepadState parse_dualsense_report(const uint8_t* report, uint16_t len) {
    GamepadState s;
    if (len < 9) return s; // too short to contain even the D-pad/Cross byte
    s.present = true;

    s.lx = report[1];
    s.ly = report[2];
    uint8_t dpad = report[8] & 0x0F;
    bool cross_pressed = (report[8] & 0x20) != 0;

    if (dpad == 0 || dpad == 1 || dpad == 7) s.buttons |= kBtUp;
    if (dpad == 1 || dpad == 2 || dpad == 3) s.buttons |= kBtRight;
    if (dpad == 3 || dpad == 4 || dpad == 5) s.buttons |= kBtDown;
    if (dpad == 5 || dpad == 6 || dpad == 7) s.buttons |= kBtLeft;
    if (cross_pressed) s.buttons |= kBtA;

    return s;
}

// --- XInput vendor-class driver ---
// Xbox 360 wired pads (and clones) don't enumerate as HID; one control
// interface, class=0xFF/subclass=0x5D/protocol=0x01, one interrupt IN
// endpoint with 20-byte input reports. Registered via
// usbh_app_driver_get_cb() so mere claim of that interface doesn't collide
// with the HID driver.
constexpr uint8_t kXInputSubclass = 0x5D;
constexpr uint8_t kXInputProtocol = 0x01;
constexpr uint16_t kXInputReportLen = 20;

struct XInputDevice {
    uint8_t dev_addr = 0;
    uint8_t ep_in = 0;
    uint8_t report_buf[kXInputReportLen]{};
};
XInputDevice g_xinput_devices[2];

XInputDevice* find_xinput_device(uint8_t dev_addr) {
    for (auto& d : g_xinput_devices)
        if (d.dev_addr == dev_addr) return &d;
    return nullptr;
}
XInputDevice* allocate_xinput_device(uint8_t dev_addr) {
    for (auto& d : g_xinput_devices)
        if (d.dev_addr == 0) { d.dev_addr = dev_addr; return &d; }
    return nullptr;
}

bool xinput_init() { return true; }
bool xinput_deinit() { return true; }

bool xinput_open(uint8_t /*rhport*/, uint8_t dev_addr, tusb_desc_interface_t const* itf_desc, uint16_t max_len) {
    if (itf_desc->bInterfaceClass != TUSB_CLASS_VENDOR_SPECIFIC ||
        itf_desc->bInterfaceSubClass != kXInputSubclass ||
        itf_desc->bInterfaceProtocol != kXInputProtocol) {
        return false; // not an XInput control interface -- leave unclaimed
    }

    XInputDevice* dev = allocate_xinput_device(dev_addr);
    if (dev == nullptr) return false;

    auto const* p = reinterpret_cast<uint8_t const*>(itf_desc);
    uint8_t const* end = p + max_len;
    p = tu_desc_next(p);

    uint8_t ep_in = 0;
    while (p < end && ep_in == 0) {
        if (tu_desc_type(p) == TUSB_DESC_ENDPOINT) {
            auto const* ep = reinterpret_cast<tusb_desc_endpoint_t const*>(p);
            if (tu_edpt_dir(ep->bEndpointAddress) == TUSB_DIR_IN && tuh_edpt_open(dev_addr, ep)) {
                ep_in = ep->bEndpointAddress;
            }
        }
        p = tu_desc_next(p);
    }
    if (ep_in == 0) { dev->dev_addr = 0; return false; }

    dev->ep_in = ep_in;
    if (auto* self = UsbHidHost::instance()) self->on_xinput_mount(dev_addr);
    return true;
}

bool xinput_set_config(uint8_t dev_addr, uint8_t /*itf_num*/) {
    XInputDevice* dev = find_xinput_device(dev_addr);
    if (dev == nullptr) return false;
    usbh_driver_set_config_complete(dev_addr, 0);
    usbh_edpt_xfer(dev_addr, dev->ep_in, dev->report_buf, kXInputReportLen);
    return true;
}

bool xinput_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
    XInputDevice* dev = find_xinput_device(dev_addr);
    if (dev != nullptr && ep_addr == dev->ep_in && result == XFER_RESULT_SUCCESS) {
        if (auto* self = UsbHidHost::instance())
            self->on_xinput_report(dev_addr, dev->report_buf, static_cast<uint16_t>(xferred_bytes));
        // Re-arm only on success -- re-arming on a stall/unplug wedges the
        // host stack (TOM6809 real-hardware finding).
        usbh_edpt_xfer(dev_addr, dev->ep_in, dev->report_buf, kXInputReportLen);
    }
    return true;
}

void xinput_close(uint8_t dev_addr) {
    if (XInputDevice* dev = find_xinput_device(dev_addr)) {
        *dev = XInputDevice{};
        if (auto* self = UsbHidHost::instance()) self->on_xinput_unmount(dev_addr);
    }
}

const usbh_class_driver_t kXInputDriver = {
    "XINPUT", xinput_init, xinput_deinit, xinput_open, xinput_set_config, xinput_xfer_cb, xinput_close,
};

} // namespace

// Small registry of mounted keyboard dev_addrs so the keyboard count can be
// decremented accurately on unplug (tuh_hid_umount_cb gives no protocol).
constexpr uint8_t kMaxKeyboardRegs = 8;
uint8_t g_keyboard_addrs[kMaxKeyboardRegs]{};
bool register_keyboard(uint8_t dev_addr) {
    for (uint8_t& a : g_keyboard_addrs) {
        if (a == dev_addr) return true;
        if (a == 0) { a = dev_addr; return true; }
    }
    return false;
}
bool unregister_keyboard(uint8_t dev_addr) {
    bool found = false;
    for (uint8_t& a : g_keyboard_addrs) {
        if (a == dev_addr) { a = 0; found = true; }
    }
    return found;
}

// Invoked by TinyUSB's host stack at tuh_init() time to collect any
// application-registered class drivers beyond its built-ins.
extern "C" usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count) {
    *driver_count = 1;
    return &kXInputDriver;
}

UsbHidHost* UsbHidHost::s_instance = nullptr;

// --- TinyUSB HID host callbacks (rhport 1) ---

extern "C" void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report,
                                 uint16_t desc_len) {
    // Classification ported to exactly match TOM6809's own original
    // PicoUsbHidInput.cpp (the proven, real-hardware-validated driver this
    // component replaced) after two real-hardware regressions here:
    //
    // 1. tuh_hid_get_protocol() returns the negotiated BOOT(0)/REPORT(1)
    //    protocol *mode*, not the interface's declared type --
    //    tuh_hid_interface_protocol() is the one that returns
    //    NONE(0)/KEYBOARD(1)/MOUSE(2). This host defaults new devices to
    //    boot mode, so the original `get_protocol() ==
    //    HID_ITF_PROTOCOL_KEYBOARD (1)` mistake compared "is this device in
    //    report mode" against "is this a keyboard" -- always false, so no
    //    keyboard interface was ever recognized as one.
    // 2. is_joystick/is_mouse must be mutually exclusive with is_keyboard
    //    (and with each other) the same way the original does it -- without
    //    the exclusions, a device whose descriptor happens to trip more than
    //    one heuristic could be mis-classified.
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    bool is_keyboard = (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD);

    // DualSense identified by VID/PID first, independent of the descriptor
    // heuristic below: its descriptor doesn't reliably trip
    // looks_like_joystick_report_descriptor() (TOM6809 real-hardware
    // finding), even though it's a known, specific device identifiable
    // another way. No false-positive risk: only two exact (vid,pid) pairs
    // match.
    uint16_t vid = 0, pid = 0;
    bool is_dualsense = tuh_vid_pid_get(dev_addr, &vid, &pid) && is_dualsense_vid_pid(vid, pid);

    // Boot protocol alone can't tell a real gamepad apart from any other
    // non-keyboard, non-mouse HID interface (HID_ITF_PROTOCOL_NONE covers
    // both) -- confirm against the device's own report descriptor instead.
    bool is_joystick = !is_keyboard && itf_protocol != HID_ITF_PROTOCOL_MOUSE &&
                        (is_dualsense || looks_like_joystick_report_descriptor(desc_report, desc_len));
    // A real USB mouse: boot protocol MOUSE, or a descriptor whose top-level
    // collection is (Generic Desktop, Mouse). Many mice never negotiate boot
    // protocol MOUSE (itf_protocol stays NONE) -- the descriptor heuristic is
    // what actually recognizes those, matching the original.
    bool is_mouse = !is_keyboard && !is_joystick &&
                    (itf_protocol == HID_ITF_PROTOCOL_MOUSE ||
                     looks_like_mouse_report_descriptor(desc_report, desc_len));
    if (auto* self = UsbHidHost::instance())
        self->on_mount(dev_addr, instance, is_keyboard, is_joystick, is_mouse, is_dualsense);
    // Re-arm the report queue unconditionally (matching the original) -- or
    // no reports (not even the first) will ever be delivered, including for
    // an interface on_mount() didn't recognize as anything.
    tuh_hid_receive_report(dev_addr, instance);
}

extern "C" void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    if (auto* self = UsbHidHost::instance())
        self->on_hid_unmount(dev_addr, instance, unregister_keyboard(dev_addr));
}

extern "C" void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report,
                                           uint16_t len) {
    if (auto* self = UsbHidHost::instance()) {
        // Matches the original: dispatch keyboard by itf_protocol (reliable
        // -- keyboards do negotiate boot protocol KEYBOARD), but dispatch
        // mouse by the tracked dev_addr/instance from on_mount() rather than
        // re-checking itf_protocol==MOUSE here. A mouse recognized via the
        // descriptor heuristic (see tuh_hid_mount_cb above) never has
        // itf_protocol==MOUSE, so re-deriving it at report time would send
        // every one of its reports to on_gamepad_report() instead --
        // confirmed on real hardware as the reason a mounted mouse's cursor
        // never moved.
        uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
        if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
            self->on_keyboard_report(report, len);
        } else if (self->matches_mouse(dev_addr, instance)) {
            self->on_mouse_report(report, len);
        } else {
            self->on_gamepad_report(report, len, dev_addr, instance);
        }
    }
    // Re-arm for the next report -- TinyUSB delivers exactly one report per
    // tuh_hid_receive_report() call.
    tuh_hid_receive_report(dev_addr, instance);
}

bool UsbHidHost::init(const UsbHidConfig& config) {
    m_config = config;
    s_instance = this;

    if (m_config.max_gamepads > kMaxGamepadSlots) m_config.max_gamepads = kMaxGamepadSlots;
    m_gamepad_count = 0;

    if (m_config.run_on_core1) {
        multicore_reset_core1();
        static uint32_t core1_stack[4096]; // 16 KB, per TOM6809's stack guidance
        multicore_launch_core1_with_stack(core1_entry, core1_stack, sizeof(core1_stack));
    } else {
        host_stack_setup();
    }
    return true;
}

void UsbHidHost::core1_entry() { host_stack_setup(); }

void UsbHidHost::host_stack_setup() {
    UsbHidHost* self = instance();
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = self->m_config.pin_dp;
    // Current Pico-PIO-USB uses one PIO (block) for both TX and RX halves.
    pio_cfg.pio_tx_num = self->m_config.pio_num;
    pio_cfg.pio_rx_num = self->m_config.pio_num;

    // PIO_USB_DEFAULT_CONFIG's tx_ch defaults to a *hardcoded* DMA channel
    // index (0, see pio_usb_configuration.h's PIO_USB_DMA_TX_DEFAULT), and
    // pio_usb.c's pio_usb_bus_init() claims it via dma_claim_mask(1<<tx_ch)
    // -- panics ("DMA channel N is already claimed") if another driver
    // already claimed it before this runs (TOM6809 real-hardware finding:
    // hit this against both a DVI/HDMI video driver's own DMA channels and
    // an LCD driver's DMA-backed pixel writes, depending on which else was
    // active). dma_claim_unused_channel(true) alone still panics identically
    // -- it *claims* the channel itself, and pio_usb_bus_init()'s own
    // dma_claim_mask() then tries to claim the same already-ours channel
    // again. Fix: unclaim it immediately after finding it (a peek-then-
    // release, not a hold) so it's genuinely free again when
    // pio_usb_bus_init() claims it for real. A TOCTOU race exists in theory,
    // but nothing else claims a DMA channel during this single-threaded
    // boot-time window in practice.
    pio_cfg.tx_ch = static_cast<uint8_t>(dma_claim_unused_channel(true));
    dma_channel_unclaim(pio_cfg.tx_ch);

    tuh_configure(kTuhRhport, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    tuh_init(kTuhRhport);
    self->m_initialized = true;

    while (true) {
        tuh_task();
    }
}

void UsbHidHost::task() { tuh_task(); }

// --- Keyboard ---

void UsbHidHost::on_mount(uint8_t dev_addr, uint8_t instance, bool is_keyboard, bool is_joystick, bool is_mouse,
                          bool is_dualsense) {
    // tuh_hid_mount_cb() re-arms the report queue unconditionally after this
    // call returns (matching the original driver) -- no need to do it here
    // per-branch, and the original never did.
    if (m_config.enable_keyboard && is_keyboard) {
        if (register_keyboard(dev_addr)) {
            __atomic_fetch_add(&m_keyboard_count, 1, __ATOMIC_RELAXED);
        }
    } else if (m_config.enable_mouse && is_mouse && m_mouse_dev_addr == 0) {
        m_mouse_dev_addr = dev_addr;
        m_mouse_instance = instance;
        m_mouse.present = true;
    } else if (m_config.enable_gamepad && is_joystick) {
        allocate_gamepad_slot(dev_addr, instance, false, is_dualsense);
    } else if (m_config.enable_keyboard || m_config.enable_mouse || m_config.enable_gamepad) {
        // Unrelated HID interface (e.g. a media-key collection) -- ignore.
    }
}

void UsbHidHost::on_hid_unmount(uint8_t dev_addr, uint8_t instance, bool is_keyboard) {
    if (is_keyboard) {
        __atomic_fetch_sub(&m_keyboard_count, 1, __ATOMIC_RELAXED);
    }
    if (m_mouse_dev_addr == dev_addr && m_mouse_instance == instance) {
        m_mouse = MouseState{};
        m_mouse_dev_addr = 0;
        m_mouse_instance = 0;
    }
    for (auto& slot : m_gamepads) {
        if (slot.in_use && slot.dev_addr == dev_addr && slot.instance == instance) {
            slot = GamepadSlot{};
            m_gamepad_count = 0;
            for (auto& s : m_gamepads)
                if (s.in_use) m_gamepad_count++;
        }
    }
}

void UsbHidHost::on_keyboard_report(const uint8_t* report, uint16_t len) {
    if (len < 2) return;
    uint8_t modifier = report[0];
    uint8_t keys[6] = {0};
    uint8_t n = (len - 2 < 6) ? static_cast<uint8_t>(len - 2) : 6;
    for (uint8_t i = 0; i < n; ++i) keys[i] = report[2 + i];

    const uint8_t active = m_key_state_active;
    const uint8_t inactive = active ^ 1;
    memset(m_key_state[inactive], 0, sizeof(m_key_state[inactive]));

    set_bit(m_key_state[inactive], keys, n);
    m_key_state_active = inactive;

    m_modifiers[inactive] = modifier;
    m_modifiers_active = inactive;

    // Edge detection for consume_key_press.
    memset(m_press_edges[inactive], 0, sizeof(m_press_edges[inactive]));
    for (uint8_t i = 0; i < n; ++i) {
        uint8_t k = keys[i];
        if (k >= kMaxKeyUsages) continue;
        bool was_down = (m_prev_key_held[k >> 3] >> (k & 7)) & 1;
        if (!was_down) m_press_edges[inactive][k >> 3] |= 1u << (k & 7);
    }
    memset(m_prev_key_held, 0, sizeof(m_prev_key_held));
    set_bit(m_prev_key_held, keys, n);
    m_press_edges_active = inactive;

    // Typed-ascii edges for up to 6 held keys.
    for (uint8_t i = 0; i < 6; ++i) {
        uint8_t k = keys[i];
        if (k >= kMaxKeyUsages) continue;
        bool was_down = false;
        for (uint8_t j = 0; j < 6; ++j)
            if (m_prev_keycode[j] == k) { was_down = true; break; }
        if (!was_down) {
            uint8_t ch = hid_usage_to_ascii(k, modifier, m_config.keymap_index);
            if (ch) push_typed_ascii(ch);
        }
    }
    memcpy(m_prev_keycode, keys, 6);
}

bool UsbHidHost::is_key_down(uint8_t hid_usage_id) const {
    if (hid_usage_id >= kMaxKeyUsages) return false;
    return (m_key_state[m_key_state_active][hid_usage_id >> 3] >> (hid_usage_id & 7)) & 1;
}

bool UsbHidHost::is_modifier_down(uint8_t mask) const {
    return (m_modifiers[m_modifiers_active] & mask) != 0;
}

bool UsbHidHost::consume_key_press(uint8_t hid_usage_id) {
    if (hid_usage_id >= kMaxKeyUsages) return false;
    return (m_press_edges[m_press_edges_active][hid_usage_id >> 3] >> (hid_usage_id & 7)) & 1;
}

uint8_t UsbHidHost::consume_typed_ascii_char() {
    if (m_ascii_head == m_ascii_tail) return 0;
    uint8_t ch = m_ascii_queue[m_ascii_tail];
    m_ascii_tail = static_cast<uint8_t>((m_ascii_tail + 1) % kAsciiQueueSize);
    return ch;
}

void UsbHidHost::push_typed_ascii(uint8_t ch) {
    uint8_t next = static_cast<uint8_t>((m_ascii_head + 1) % kAsciiQueueSize);
    if (next == m_ascii_tail) return; // full -- drop
    m_ascii_queue[m_ascii_head] = ch;
    m_ascii_head = next;
}

// --- Mouse ---

void UsbHidHost::on_mouse_report(const uint8_t* report, uint16_t len) {
    // Boot protocol: TinyUSB enumerates every HID interface in boot protocol
    // by default and this host never calls tuh_hid_set_protocol(), so the
    // mouse report is always the fixed 3-byte boot layout -- byte0=buttons
    // (bit0=left, bit1=right), byte1=dx, byte2=dy (signed 8-bit) -- with NO
    // report-ID prefix (boot protocol reports are never numbered). Reports
    // may be longer than 3 bytes (padding to endpoint size, a wheel byte);
    // the extra bytes are ignored. Ported from TOM6809's own original driver
    // after a real-hardware regression here: this component previously read
    // dx/dy from bytes 1-2 but buttons from byte 3 (gated behind len>=5),
    // which never matches a real boot-protocol mouse -- left-click never
    // registered because that byte offset is never populated.
    if (len < 3) return;
    uint8_t buttons = report[0];
    int8_t dx = static_cast<int8_t>(report[1]);
    int8_t dy = static_cast<int8_t>(report[2]);
    m_mouse.left_button = (buttons & 0x01) != 0;
    m_mouse.right_button = (buttons & 0x02) != 0;
    m_mouse.x += dx;
    m_mouse.y += dy;
    if (m_mouse.x < 0) m_mouse.x = 0;
    if (m_mouse.y < 0) m_mouse.y = 0;
    if (m_mouse.x > m_config.mouse_max_x) m_mouse.x = m_config.mouse_max_x;
    if (m_mouse.y > m_config.mouse_max_y) m_mouse.y = m_config.mouse_max_y;
}

UsbHidHost::MouseState UsbHidHost::mouse_state() const { return m_mouse; }

// --- Gamepad ---

int UsbHidHost::allocate_gamepad_slot(uint8_t dev_addr, uint8_t instance, bool is_xinput, bool is_dualsense) {
    for (size_t i = 0; i < m_config.max_gamepads; ++i) {
        if (m_gamepads[i].in_use) continue;
        GamepadSlot& slot = m_gamepads[i];
        slot = GamepadSlot{};
        slot.in_use = true;
        slot.is_xinput = is_xinput;
        slot.is_dualsense = is_dualsense;
        slot.dev_addr = dev_addr;
        slot.instance = instance;
        slot.mapped_index = m_gamepad_count;
        m_gamepad_count++;
        return static_cast<int>(i);
    }
    return -1;
}

void UsbHidHost::on_gamepad_report(const uint8_t* report, uint16_t len, uint8_t dev_addr, uint8_t instance) {
    // Only feed the slot that actually owns (dev_addr, instance).
    for (auto& slot : m_gamepads) {
        if (!slot.in_use || slot.is_xinput || slot.dev_addr != dev_addr || slot.instance != instance)
            continue;

        if (slot.is_dualsense) {
            slot.state = parse_dualsense_report(report, len);
            return;
        }

        GamepadState& s = slot.state;
        s.present = true;
        s.is_xinput = false;

        // Simple fallback byte layout: assume byte0 = left X, byte1 = left Y,
        // byte2 bit0 = fire. Many cheap pads don't declare a proper Gamepad
        // top-level collection, so this is best-effort.
        if (len >= 2) {
            s.lx = report[0];
            s.ly = report[1];
        }
        if (len >= 3) {
            s.buttons = (report[2] & 0x01) ? kBtA : 0;
        }
        return;
    }
    (void)instance;
}

void UsbHidHost::on_xinput_mount(uint8_t dev_addr) {
    allocate_gamepad_slot(dev_addr, 0, true);
}

void UsbHidHost::on_xinput_report(uint8_t dev_addr, const uint8_t* report, uint16_t len) {
    if (len < 14) return; // XInput input packet layout: header + packet + 14 bytes
    for (auto& slot : m_gamepads) {
        if (!slot.in_use || !slot.is_xinput || slot.dev_addr != dev_addr) continue;
        GamepadState& s = slot.state;
        s.present = true;
        s.is_xinput = true;

        // XInput 360 wired input report (offset 2 = DPAD+bface byte).
        uint8_t b0 = report[2];
        s.buttons = 0;
        if (b0 & 0x01) s.buttons |= kBtUp;
        if (b0 & 0x02) s.buttons |= kBtDown;
        if (b0 & 0x04) s.buttons |= kBtLeft;
        if (b0 & 0x08) s.buttons |= kBtRight;
        if (b0 & 0x10) s.buttons |= kBtStart;
        if (b0 & 0x20) s.buttons |= kBtBack;
        if (b0 & 0x40) s.buttons |= kBtLS;
        if (b0 & 0x80) s.buttons |= kBtRS;

        uint8_t b1 = report[3];
        if (b1 & 0x01) s.buttons |= kBtLB;
        if (b1 & 0x02) s.buttons |= kBtRB;
        if (b1 & 0x04) s.buttons |= kBtGuide;

        // face buttons
        if (b1 & 0x10) s.buttons |= kBtA;
        if (b1 & 0x20) s.buttons |= kBtB;
        if (b1 & 0x40) s.buttons |= kBtX;
        if (b1 & 0x80) s.buttons |= kBtY;

        // Real-hardware regression fix, cross-checked against Linux's xpad
        // driver and TOM6809's own original XInput parser: this component
        // had the trigger and stick byte ranges swapped/misaligned. The
        // actual wired Xbox 360 layout is byte4=LT, byte5=RT (single bytes,
        // 0-255), bytes6-7=left stick X (int16 LE), bytes8-9=left stick Y,
        // bytes10-11=right stick X, bytes12-13=right stick Y -- reading
        // triggers as sticks (constant near-zero values map to just under
        // this struct's 128-centered dead zone) is exactly what produced a
        // constant "stick pushed up-left" reading regardless of actual input.
        s.lt = report[4];
        s.rt = report[5];
        auto scale_axis = [](int16_t raw, bool invert) -> uint8_t {
            int32_t v = invert ? -static_cast<int32_t>(raw) : raw;
            return static_cast<uint8_t>((v >> 8) + 128);
        };
        int16_t lx16 = static_cast<int16_t>(report[6] | (report[7] << 8));
        int16_t ly16 = static_cast<int16_t>(report[8] | (report[9] << 8));
        int16_t rx16 = static_cast<int16_t>(report[10] | (report[11] << 8));
        int16_t ry16 = static_cast<int16_t>(report[12] | (report[13] << 8));
        // XInput's raw Y is positive-up; this struct's shared dead-zone
        // convention (see PicoUsbHidInput::get_joystick_state()) treats
        // higher values as "down", so the Y axes are inverted here to match.
        s.lx = scale_axis(lx16, false);
        s.ly = scale_axis(ly16, true);
        s.rx = scale_axis(rx16, false);
        s.ry = scale_axis(ry16, true);
        return;
    }
}

void UsbHidHost::on_xinput_unmount(uint8_t dev_addr) {
    for (auto& slot : m_gamepads) {
        if (slot.in_use && slot.is_xinput && slot.dev_addr == dev_addr) {
            slot = GamepadSlot{};
            m_gamepad_count = 0;
            for (auto& s : m_gamepads)
                if (s.in_use) m_gamepad_count++;
        }
    }
}

GamepadState UsbHidHost::gamepad_state(size_t index) const {
    GamepadState result;
    for (const auto& slot : m_gamepads) {
        if (slot.in_use && slot.mapped_index == index) {
            result = slot.state;
            result.present = true;
            return result;
        }
    }
    return result;
}

uint8_t UsbHidHost::connected_keyboard_count() const { return m_keyboard_count; }
uint8_t UsbHidHost::connected_gamepad_count() const { return m_gamepad_count; }

} // namespace pico_toolset