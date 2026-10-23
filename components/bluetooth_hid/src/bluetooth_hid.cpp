// BLE HID-over-GATT host on BTstack's hids_host. Flow follows BTstack's
// hog_host_demo (scan for the HID service, connect, SM pairing, hids_host),
// extended with several simultaneous devices, a whitelist-based reconnect of
// bonded devices, persistent device records, and generic report parsing into
// keyboard / mouse / GamepadState.
#include "pico_toolset/bluetooth_hid.h"

#include <cstring>

#include "btstack.h"
#include "btstack_tlv.h"
#include "ble/gatt-service/hids_host.h"
#ifdef ENABLE_CLASSIC
#include "pico_toolset/bluetooth_classic.h"
#endif
#include "pico/async_context.h"
#include "pico/cyw43_arch.h"

namespace pico_toolset {

namespace {

constexpr uint32_t kTlvTag = (static_cast<uint32_t>('P') << 24) | (static_cast<uint32_t>('H') << 16) |
                             (static_cast<uint32_t>('I') << 8) | 'D';
constexpr size_t kMaxConnected = BluetoothHid::kMaxConnected;
constexpr size_t kMaxBonded = BluetoothHid::kMaxBonded;
constexpr uint32_t kConnectTimeoutMs = 10'000;
constexpr uint32_t kReconnectDelayMs = 500;

// Stored::addr_type value marking a Bluetooth Classic device (LE types are 0..3).
constexpr uint8_t kClassicAddrType = 0xFE;
// Stored.kinds bit (above the public bt_hid_kind bits): this pad has to be served over our own L2CAP
// channels because its SDP record is a stub (see start_raw()).
constexpr uint8_t kRawLinkFlag = 0x80;

constexpr uint16_t kPageDesktop = 0x01, kPageSim = 0x02, kPageKeyboard = 0x07, kPageButton = 0x09;

// One bonded device as persisted (32 bytes; the keys themselves are in
// BTstack's own device database).
struct Stored {
    uint8_t addr[6];
    uint8_t addr_type;
    uint8_t kinds;
    char    name[24];
};
static_assert(sizeof(Stored) == 32);

struct Slot {
    bool used = false;
    hci_con_handle_t handle = HCI_CON_HANDLE_INVALID;
    uint16_t hids_cid = 0;       // LE: hids_host client id
    bool classic = false;        // Bluetooth Classic HID connection
    bool raw = false;            // Classic without SDP: raw L2CAP channels, unparsed reports
    uint16_t classic_cid = 0;    // Classic: hid_host connection id
    bool descriptor_ready = false;
    int pad_index = -1;
    Stored dev{};
};

// Report descriptor assumed for Classic pads reached through the raw L2CAP fallback (their SDP
// records are unusable). It is the descriptor read from the "SFR Gamepad" (vendor 0x2E2C): the
// common Android-style layout with report id 3 = gamepad (15 buttons, hat, X/Y/Z/Rz, brake,
// accelerator); ids 1, 2 and 4 are keyboard, mouse and media keys.
constexpr uint8_t kRawPadDescriptor[] = {
    0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x85, 0x01, 0x75, 0x01, 0x95, 0x08,
    0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x03, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00,
    0x26, 0xef, 0x00, 0x05, 0x07, 0x19, 0x15, 0x29, 0xef, 0x81, 0x00, 0xc0,
    0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x85, 0x02, 0x09, 0x01, 0xa1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03,
    0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x03, 0x05, 0x01,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7f, 0x75, 0x08,
    0x95, 0x03, 0x81, 0x06, 0xc0, 0xc0, 0x05, 0x01, 0x09, 0x05, 0xa1, 0x01,
    0x85, 0x03, 0x15, 0x00, 0x25, 0x01, 0x35, 0x00, 0x45, 0x01, 0x75, 0x01,
    0x95, 0x0f, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0f, 0x81, 0x02, 0x95, 0x01,
    0x81, 0x01, 0x05, 0x01, 0x25, 0x07, 0x46, 0x3b, 0x01, 0x75, 0x04, 0x95,
    0x01, 0x65, 0x14, 0x09, 0x39, 0x81, 0x42, 0x65, 0x00, 0x95, 0x01, 0x81,
    0x01, 0x26, 0xff, 0x00, 0x46, 0xff, 0x00, 0x09, 0x30, 0x09, 0x31, 0x09,
    0x32, 0x09, 0x35, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02, 0x05, 0x02, 0x15,
    0x00, 0x26, 0xff, 0x00, 0x09, 0xc5, 0x09, 0xc4, 0x95, 0x02, 0x75, 0x08,
    0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0xa1, 0x02, 0x75, 0x08,
    0x95, 0x04, 0x15, 0x00, 0x35, 0x00, 0x46, 0xff, 0x00, 0x26, 0xff, 0x00,
    0x09, 0x02, 0x91, 0x02, 0xc0, 0xc0, 0x05, 0x0c, 0x09, 0x01, 0xa1, 0x01,
    0x85, 0x04, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x09, 0x40,
    0x0a, 0x23, 0x02, 0x0a, 0x24, 0x02, 0x09, 0xcd, 0x09, 0xb5, 0x09, 0x8f,
    0x09, 0xea, 0x09, 0xe9, 0x81, 0x02, 0x95, 0x28, 0x81, 0x03, 0xc0,
};

uint8_t g_descriptor_storage[1200];
#ifdef ENABLE_CLASSIC
uint8_t g_classic_descriptor_storage[1200];
#endif

struct ContextLock {
    ContextLock() { async_context_acquire_lock_blocking(cyw43_arch_async_context()); }
    ~ContextLock() { async_context_release_lock(cyw43_arch_async_context()); }
};

BtHidDevice to_public(const Stored& s, bool connected, int8_t rssi = 0) {
    BtHidDevice d{};
    std::memcpy(d.addr, s.addr, 6);
    d.addr_type = s.addr_type;
    d.kinds = s.kinds & 0x7F;
    d.rssi = rssi;
    d.connected = connected;
    std::memcpy(d.name, s.name, sizeof(d.name));
    d.name[sizeof(d.name) - 1] = '\0';
    return d;
}

// Scales a HID field to 0-255 using its bit size and logical minimum
// (assumes the field spans the full range of its size, true for the 8-bit
// and 16-bit sticks and the 10-bit triggers of common gamepads).
uint8_t norm_axis(int32_t value, uint8_t size_bits, int32_t logical_min) {
    int64_t v = static_cast<int64_t>(value) - logical_min;
    if (v < 0) v = 0;
    if (size_bits > 8) v >>= (size_bits - 8);
    else if (size_bits < 8) v <<= (8 - size_bits);
    return static_cast<uint8_t>(v > 255 ? 255 : v);
}

// HID button usage -> unified kBt* bit (Xbox-style layout; usage numbers
// follow the Linux gamepad order Xbox controllers use over Bluetooth). Verify
// against gamepad_raw_buttons() for other controllers.
struct ButtonMap { uint8_t usage; uint16_t mask; };
constexpr ButtonMap kButtonMap[] = {
    {1, kBtA}, {2, kBtB}, {4, kBtX}, {5, kBtY}, {7, kBtLB}, {8, kBtRB},
    {11, kBtBack}, {12, kBtStart}, {13, kBtGuide}, {14, kBtLS}, {15, kBtRS},
};

// Top-level application collections of a report descriptor -> kinds.
uint8_t scan_kinds(const uint8_t* descriptor, uint16_t len) {
    uint8_t kinds = 0;
    btstack_hid_descriptor_iterator_t it;
    btstack_hid_descriptor_iterator_init(&it, descriptor, len);
    uint16_t page = 0, usage = 0;
    int depth = 0;
    while (btstack_hid_descriptor_iterator_has_more(&it)) {
        const hid_descriptor_item_t* item = btstack_hid_descriptor_iterator_get_item(&it);
        if (item->item_type == Global && item->item_tag == UsagePage) {
            page = static_cast<uint16_t>(item->item_value);
        } else if (item->item_type == Local && item->item_tag == Usage) {
            usage = static_cast<uint16_t>(item->item_value);
        } else if (item->item_type == Main && item->item_tag == Coll) {
            if (depth == 0 && page == kPageDesktop) {
                if (usage == 0x06) kinds |= bt_hid_kind::Keyboard;
                else if (usage == 0x02) kinds |= bt_hid_kind::Mouse;
                else if (usage == 0x04 || usage == 0x05) kinds |= bt_hid_kind::Gamepad;
            }
            ++depth;
        } else if (item->item_type == Main && item->item_tag == EndColl) {
            if (depth > 0) --depth;
        }
        if (item->item_type == Main) usage = 0;
    }
    return kinds;
}

} // namespace

struct BluetoothHid::Impl {
    BluetoothHid* owner = nullptr;

    Stored stored[kMaxBonded]{};
    size_t stored_count = 0;
    Slot slots[kMaxConnected];

    bool pending = false;       // an outgoing connection is being set up (by address)
    Stored pending_dev{};
    bool wl_connecting = false; // a whitelist connection attempt is outstanding
    bool pending_paired = false; // Classic: bonding already reported as Paired for `pending_dev`
    uint8_t seen[8][6]{};
    size_t seen_count = 0;

    // Classic fallback transport (broken SDP): HID control + interrupt L2CAP channels.
    struct RawLink {
        bool active = false;
        Stored dev{};
        uint16_t ctrl_cid = 0, int_cid = 0;
        bool ctrl_open = false;
        bool quiet = false; // background reconnect attempt: no events when it fails
    } rawlink;
    btstack_timer_source_t classic_retry_timer{};

    btstack_timer_source_t pair_timer{};
    btstack_timer_source_t connect_timer{};
    btstack_timer_source_t reconnect_timer{};
    btstack_timer_source_t bond_timer{};
    btstack_packet_callback_registration_t hci_reg{};
    btstack_packet_callback_registration_t sm_reg{};

    // Input state, written from the BTstack context.
    volatile uint32_t keys[8]{};
    volatile uint32_t key_edges[8]{};
    volatile uint8_t modifiers = 0;
    volatile int mouse_dx = 0, mouse_dy = 0, mouse_wheel = 0;
    volatile bool mouse_buttons[3]{};
    GamepadState pads[kMaxConnected];
    volatile uint32_t pad_raw[kMaxConnected]{};

    const BluetoothHidConfig& cfg() const { return owner->m_cfg; }

    void emit(BtHidEventType type, const Stored& dev, bool connected = false, int8_t rssi = 0,
              uint32_t passkey = 0, uint8_t status = 0, uint8_t reason = 0) {
        if (!cfg().callback) return;
        BtHidEvent e{};
        e.type = type;
        e.device = to_public(dev, connected, rssi);
        e.passkey = passkey;
        e.status = status;
        e.reason = reason;
        cfg().callback(e, cfg().user);
    }

    int find_stored(const uint8_t* addr) const {
        for (size_t i = 0; i < stored_count; ++i)
            if (std::memcmp(stored[i].addr, addr, 6) == 0) return static_cast<int>(i);
        return -1;
    }
    Slot* slot_by_handle(hci_con_handle_t h) {
        for (auto& s : slots) if (s.used && s.handle == h) return &s;
        return nullptr;
    }
    Slot* slot_by_cid(uint16_t cid) {
        for (auto& s : slots) if (s.used && s.hids_cid == cid) return &s;
        return nullptr;
    }
    Slot* slot_by_classic_cid(uint16_t cid) {
        for (auto& s : slots) if (s.used && s.classic && s.classic_cid == cid) return &s;
        return nullptr;
    }
    Slot* slot_by_addr(const uint8_t* addr) {
        for (auto& s : slots) if (s.used && std::memcmp(s.dev.addr, addr, 6) == 0) return &s;
        return nullptr;
    }
    Slot* alloc_slot() {
        for (auto& s : slots) if (!s.used) { s = Slot{}; s.used = true; return &s; }
        return nullptr;
    }
    size_t connected_count() const {
        size_t n = 0;
        for (const auto& s : slots) n += s.used;
        return n;
    }

    // ---- persistence ----
    void save_stored() {
        const btstack_tlv_t* tlv = nullptr;
        void* ctx = nullptr;
        btstack_tlv_get_instance(&tlv, &ctx);
        if (!tlv) return;
        if (stored_count == 0) tlv->delete_tag(ctx, kTlvTag);
        else tlv->store_tag(ctx, kTlvTag, reinterpret_cast<const uint8_t*>(stored),
                            static_cast<uint32_t>(stored_count * sizeof(Stored)));
    }
    void load_stored() {
        const btstack_tlv_t* tlv = nullptr;
        void* ctx = nullptr;
        btstack_tlv_get_instance(&tlv, &ctx);
        stored_count = 0;
        if (!tlv) return;
        const int len = tlv->get_tag(ctx, kTlvTag, reinterpret_cast<uint8_t*>(stored), sizeof(stored));
        if (len > 0) stored_count = static_cast<size_t>(len) / sizeof(Stored);
        if (stored_count > kMaxBonded) stored_count = kMaxBonded;
    }
    // Removes the keys BTstack holds for a device (link key / LE bonding, whitelist entry).
    void drop_bond(const Stored& dev) {
        if (dev.addr_type == kClassicAddrType) {
#ifdef ENABLE_CLASSIC
            gap_drop_link_key_for_bd_addr(const_cast<uint8_t*>(dev.addr));
#endif
        } else {
            gap_whitelist_remove(static_cast<bd_addr_type_t>(dev.addr_type), const_cast<uint8_t*>(dev.addr));
            gap_delete_bonding(static_cast<bd_addr_type_t>(dev.addr_type), const_cast<uint8_t*>(dev.addr));
        }
    }
    void remember(const Stored& dev) {
        int i = find_stored(dev.addr);
        if (i < 0) {
            if (stored_count == kMaxBonded) { // drop the oldest to make room
                drop_bond(stored[0]);
                for (size_t k = 1; k < kMaxBonded; ++k) stored[k - 1] = stored[k];
                stored_count = kMaxBonded - 1;
            }
            i = static_cast<int>(stored_count++);
        }
        stored[i] = dev;
        save_stored();
    }

    // ---- reconnect of bonded devices through the controller's whitelist ----
    void arm_reconnect() {
        if (!cfg().auto_reconnect || owner->m_pairing || pending || wl_connecting) return;
        if (connected_count() >= kMaxConnected) return;
        bool any = false;
        for (size_t i = 0; i < stored_count; ++i) {
            if (stored[i].addr_type == kClassicAddrType) continue; // Classic devices reconnect themselves
            if (slot_by_addr(stored[i].addr)) continue;
            gap_whitelist_add(static_cast<bd_addr_type_t>(stored[i].addr_type), stored[i].addr);
            any = true;
        }
        if (!any) return;
        gap_connect_with_whitelist();
        wl_connecting = true;
    }
    static void reconnect_timeout(btstack_timer_source_t* ts) {
        if (g_impl_ptr()) g_impl_ptr()->arm_reconnect();
        (void)ts;
    }
    void schedule_reconnect() {
        btstack_run_loop_remove_timer(&reconnect_timer);
        btstack_run_loop_set_timer(&reconnect_timer, kReconnectDelayMs);
        btstack_run_loop_set_timer_handler(&reconnect_timer, &Impl::reconnect_timeout);
        btstack_run_loop_add_timer(&reconnect_timer);
    }
    static Impl*& g_impl_ptr() {
        static Impl* p = nullptr;
        return p;
    }

    // ---- pairing scan ----
    void begin_scan() {
        owner->m_pairing = true;
        seen_count = 0;
        if (wl_connecting) { gap_connect_cancel(); wl_connecting = false; }
        gap_set_scan_parameters(1, 48, 48); // active: names often sit in the scan response
        gap_start_scan();
#ifdef ENABLE_CLASSIC
        if (cfg().enable_classic) {
            gap_discoverable_control(1); // lets a pad in pairing mode find and connect to us
            const uint32_t units = cfg().pairing_timeout_ms / 1280;
            BluetoothClassic::start_inquiry(static_cast<uint8_t>(units < 1 ? 1 : units > 48 ? 48 : units), 0x05,
                                            &Impl::inquiry_trampoline, this); // peripherals only
        }
#endif
        btstack_run_loop_remove_timer(&pair_timer);
        btstack_run_loop_set_timer(&pair_timer, cfg().pairing_timeout_ms);
        btstack_run_loop_set_timer_handler(&pair_timer, &Impl::pair_timeout);
        btstack_run_loop_add_timer(&pair_timer);
        emit(BtHidEventType::PairingStarted, Stored{});
    }
    void end_scan() {
        if (!owner->m_pairing) return;
        owner->m_pairing = false;
        btstack_run_loop_remove_timer(&pair_timer);
        gap_stop_scan();
#ifdef ENABLE_CLASSIC
        if (cfg().enable_classic) { BluetoothClassic::stop_inquiry(&Impl::inquiry_trampoline); gap_discoverable_control(0); }
#endif
        emit(BtHidEventType::PairingStopped, Stored{});
        arm_reconnect();
    }
    static void pair_timeout(btstack_timer_source_t*) {
        if (g_impl_ptr()) g_impl_ptr()->end_scan();
    }

    bool seen_before(const uint8_t* addr) {
        for (size_t i = 0; i < seen_count; ++i)
            if (std::memcmp(seen[i], addr, 6) == 0) return true;
        if (seen_count < 8) std::memcpy(seen[seen_count++], addr, 6);
        return false;
    }

    static bool is_hid_advertisement(const uint8_t* ad, uint8_t len) {
        if (ad_data_contains_uuid16(len, ad, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE)) return true;
        for (unsigned i = 0; i + 1 < len;) { // GAP appearance in the HID range 0x03C0-0x03CF
            const unsigned fl = ad[i];
            if (fl == 0 || i + 1 + fl > len) break;
            if (ad[i + 1] == 0x19 && fl >= 3) {
                const uint16_t app = static_cast<uint16_t>(ad[i + 2] | (ad[i + 3] << 8));
                if ((app >> 4) == 0x3C) return true;
            }
            i += fl + 1;
        }
        return false;
    }
    static void extract_name(const uint8_t* ad, uint8_t len, char (&name)[24]) {
        for (unsigned i = 0; i + 1 < len;) {
            const unsigned fl = ad[i];
            if (fl == 0 || i + 1 + fl > len) break;
            const uint8_t type = ad[i + 1];
            if (type == 0x08 || type == 0x09) {
                const size_t n = fl - 1 < sizeof(name) - 1 ? fl - 1 : sizeof(name) - 1;
                std::memcpy(name, &ad[i + 2], n);
                name[n] = '\0';
                if (type == 0x09) return;
            }
            i += fl + 1;
        }
    }

    void on_advertisement(uint8_t* packet) {
        if (!owner->m_pairing || pending) return;
        const uint8_t* ad = gap_event_advertising_report_get_data(packet);
        const uint8_t len = gap_event_advertising_report_get_data_length(packet);
        if (!is_hid_advertisement(ad, len)) return;

        Stored dev{};
        bd_addr_t addr;
        gap_event_advertising_report_get_address(packet, addr);
        std::memcpy(dev.addr, addr, 6);
        dev.addr_type = gap_event_advertising_report_get_address_type(packet);
        extract_name(ad, len, dev.name);
        if (slot_by_addr(dev.addr)) return; // already connected
        if (seen_before(dev.addr)) return;

        const int8_t rssi = static_cast<int8_t>(gap_event_advertising_report_get_rssi(packet));
        emit(BtHidEventType::DeviceFound, dev, false, rssi);
        if (cfg().auto_pair) start_connect(dev);
    }

    // Initiates the link to `dev` (pairing happens once it is up).
    void start_connect(const Stored& dev) {
        pending = true;
        pending_dev = dev;
        if (owner->m_pairing) { // one GAP operation at a time: leave scanning
            owner->m_pairing = false;
            btstack_run_loop_remove_timer(&pair_timer);
            gap_stop_scan();
#ifdef ENABLE_CLASSIC
            if (cfg().enable_classic) BluetoothClassic::stop_inquiry(&Impl::inquiry_trampoline);
#endif
            emit(BtHidEventType::PairingStopped, Stored{});
        }
        if (wl_connecting) { gap_connect_cancel(); wl_connecting = false; }
        emit(BtHidEventType::Connecting, dev);
#ifdef ENABLE_CLASSIC
        if (dev.addr_type == kClassicAddrType) {
            // Pair (and bond) first: some pads only expose their full HID SDP record on an
            // authenticated link, so a plain hid_host_connect() sees a stub record and fails.
            // The HID connection follows when GAP_EVENT_DEDICATED_BONDING_COMPLETED arrives.
            pending_paired = false;
            bd_addr_t addr;
            std::memcpy(addr, dev.addr, 6);
            const uint8_t rc = gap_dedicated_bonding(addr, 0);
            if (rc != ERROR_CODE_SUCCESS) {
                pending = false;
                emit(BtHidEventType::PairingFailed, dev, false, 0, 0, rc);
                arm_reconnect();
            }
            return;
        }
#endif
        btstack_run_loop_remove_timer(&connect_timer);
        btstack_run_loop_set_timer(&connect_timer, kConnectTimeoutMs);
        btstack_run_loop_set_timer_handler(&connect_timer, &Impl::connect_timeout);
        btstack_run_loop_add_timer(&connect_timer);
        gap_connect(dev.addr, static_cast<bd_addr_type_t>(dev.addr_type));
    }
    static void connect_timeout(btstack_timer_source_t*) {
        Impl* im = g_impl_ptr();
        if (!im || !im->pending) return;
        gap_connect_cancel();
        im->pending = false;
        im->emit(BtHidEventType::PairingFailed, im->pending_dev, false, 0, 0, ERROR_CODE_CONNECTION_TIMEOUT);
        im->arm_reconnect();
    }

    // ---- HCI ----
    void on_connection_complete(uint8_t* packet) {
        const uint8_t status = gap_subevent_le_connection_complete_get_status(packet);
        const bool was_pending = pending;
        pending = false;
        wl_connecting = false;
        btstack_run_loop_remove_timer(&connect_timer);
        if (status != ERROR_CODE_SUCCESS) {
            if (was_pending) emit(BtHidEventType::PairingFailed, pending_dev, false, 0, 0, status);
            schedule_reconnect();
            return;
        }
        const hci_con_handle_t handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
        bd_addr_t addr;
        gap_subevent_le_connection_complete_get_peer_address(packet, addr);

        Slot* s = alloc_slot();
        if (!s) {
            gap_disconnect(handle);
            return;
        }
        s->handle = handle;
        const int si = find_stored(addr);
        if (si >= 0) s->dev = stored[si];
        else if (was_pending) s->dev = pending_dev;
        else {
            std::memset(&s->dev, 0, sizeof(s->dev));
            std::memcpy(s->dev.addr, addr, 6);
            s->dev.addr_type = gap_subevent_le_connection_complete_get_peer_address_type(packet);
        }
        emit(BtHidEventType::Connecting, s->dev);
        sm_request_pairing(handle); // pairs a new device, re-encrypts a bonded one
        arm_reconnect();            // keep looking for the other bonded devices
    }

    void clear_inputs_for(Slot& s) {
        if (s.pad_index >= 0) {
            pads[s.pad_index] = GamepadState{};
            pad_raw[s.pad_index] = 0;
        }
        if (s.dev.kinds & bt_hid_kind::Keyboard) {
            for (auto& k : keys) k = 0;
            modifiers = 0;
        }
        if (s.dev.kinds & bt_hid_kind::Mouse) {
            for (auto& b : mouse_buttons) b = false;
        }
    }

    void on_disconnection(uint8_t* packet) {
        const hci_con_handle_t handle = hci_event_disconnection_complete_get_connection_handle(packet);
        Slot* s = slot_by_handle(handle);
        if (!s) return;
        clear_inputs_for(*s);
        const Stored dev = s->dev;
        *s = Slot{};
        emit(BtHidEventType::Disconnected, dev);
        schedule_reconnect();
    }

    // ---- security manager ----
    void on_sm(uint8_t* packet) {
        switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            sm_numeric_comparison_confirm(sm_event_numeric_comparison_request_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER: {
            Slot* s = slot_by_handle(sm_event_passkey_display_number_get_handle(packet));
            emit(BtHidEventType::PasskeyDisplay, s ? s->dev : pending_dev, false, 0,
                 sm_event_passkey_display_number_get_passkey(packet));
            break;
        }
        case SM_EVENT_PAIRING_COMPLETE: {
            Slot* s = slot_by_handle(sm_event_pairing_complete_get_handle(packet));
            if (!s) break;
            const uint8_t status = sm_event_pairing_complete_get_status(packet);
            if (status == ERROR_CODE_SUCCESS) {
                emit(BtHidEventType::Paired, s->dev);
                connect_hids(*s);
            } else {
                const uint8_t reason = status == ERROR_CODE_AUTHENTICATION_FAILURE
                                           ? sm_event_pairing_complete_get_reason(packet) : 0;
                emit(BtHidEventType::PairingFailed, s->dev, false, 0, 0, status, reason);
                gap_disconnect(s->handle);
            }
            break;
        }
        case SM_EVENT_REENCRYPTION_COMPLETE: {
            Slot* s = slot_by_handle(sm_event_reencryption_complete_get_handle(packet));
            if (!s) break;
            const uint8_t status = sm_event_reencryption_complete_get_status(packet);
            if (status == ERROR_CODE_SUCCESS) {
                connect_hids(*s);
            } else {
                // The device lost its half of the bond (factory reset, other host): the
                // stored keys are useless, so drop them and let the user pair again.
                gap_delete_bonding(static_cast<bd_addr_type_t>(s->dev.addr_type), s->dev.addr);
                emit(BtHidEventType::PairingFailed, s->dev, false, 0, 0, status);
                gap_disconnect(s->handle);
            }
            break;
        }
        default:
            break;
        }
    }

    void connect_hids(Slot& s) {
        uint16_t cid = 0;
        const uint8_t rc = hids_host_connect(s.handle, hids_trampoline_fn, HID_PROTOCOL_MODE_REPORT, &cid);
        if (rc != ERROR_CODE_SUCCESS) {
            emit(BtHidEventType::PairingFailed, s.dev, false, 0, 0, rc);
            gap_disconnect(s.handle);
            return;
        }
        s.hids_cid = cid;
    }
    // hids_host delivers connect/disconnect events with packet_type HCI_EVENT_PACKET but
    // HID report notifications with packet_type HCI_EVENT_GATTSERVICE_META, so the packet
    // type must not be filtered here (filtering it dropped every key press).
    static void hids_trampoline_fn(uint8_t, uint16_t, uint8_t* packet, uint16_t) {
        if (g_impl_ptr()) g_impl_ptr()->on_hids(packet);
    }

#ifdef ENABLE_CLASSIC
    // ---- Bluetooth Classic ----
    static void inquiry_trampoline(const ClassicInquiryResult& r, void* user) {
        static_cast<Impl*>(user)->on_inquiry_result(r);
    }
    static void passkey_trampoline(const uint8_t addr[6], uint32_t passkey, void* user) {
        static_cast<Impl*>(user)->on_classic_passkey(addr, passkey);
    }

    void on_inquiry_result(const ClassicInquiryResult& r) {
        if (!owner->m_pairing || pending) return;
        Stored dev{};
        std::memcpy(dev.addr, r.addr, 6);
        dev.addr_type = kClassicAddrType;
        std::memcpy(dev.name, r.name, sizeof(dev.name) < sizeof(r.name) ? sizeof(dev.name) : sizeof(r.name));
        dev.name[sizeof(dev.name) - 1] = '\0';
        if (slot_by_addr(dev.addr) || seen_before(dev.addr)) return;
        if (cfg().callback) {
            BtHidEvent e{};
            e.type = BtHidEventType::DeviceFound;
            e.device = to_public(dev, false, r.rssi_valid ? r.rssi : 0);
            e.class_of_device = r.class_of_device;
            e.vendor_id = r.vendor_id;
            e.product_id = r.product_id;
            cfg().callback(e, cfg().user);
        }
        if (cfg().auto_pair) start_connect(dev);
    }

    void on_classic_passkey(const uint8_t addr[6], uint32_t passkey) { // a Classic keyboard: show the code
        Stored dev = pending ? pending_dev : Stored{};
        std::memcpy(dev.addr, addr, 6);
        emit(BtHidEventType::PasskeyDisplay, dev, false, 0, passkey);
    }

    static void connect_after_bonding(btstack_timer_source_t*) {
        Impl* im = g_impl_ptr();
        if (!im || !im->pending) return;
        uint16_t cid = 0;
        bd_addr_t a;
        std::memcpy(a, im->pending_dev.addr, 6);
        const uint8_t rc = hid_host_connect(a, HID_PROTOCOL_MODE_REPORT, &cid);
        if (rc != ERROR_CODE_SUCCESS) {
            im->pending = false;
            im->emit(BtHidEventType::PairingFailed, im->pending_dev, false, 0, 0, rc);
            im->arm_reconnect();
        }
    }

    void on_classic_hci(uint8_t* packet) {
        switch (hci_event_packet_get_type(packet)) {
        case GAP_EVENT_DEDICATED_BONDING_COMPLETED: {
            if (!pending || pending_dev.addr_type != kClassicAddrType) break;
            bd_addr_t bonded;
            gap_event_dedicated_bonding_completed_get_address(packet, bonded);
            if (std::memcmp(bonded, pending_dev.addr, 6) != 0) break; // another component bonded (e.g. a headset)
            const uint8_t status = gap_event_dedicated_bonding_completed_get_status(packet);
            if (status != ERROR_CODE_SUCCESS) {
                pending = false;
                emit(BtHidEventType::PairingFailed, pending_dev, false, 0, 0, status);
                arm_reconnect();
                break;
            }
            pending_paired = true;
            emit(BtHidEventType::Paired, pending_dev);
            // BTstack is still tearing down the bonding link inside this very event;
            // starting a new connection right here fails ("Command packet sent failed"),
            // so open the HID connection a moment later.
            btstack_run_loop_remove_timer(&bond_timer);
            btstack_run_loop_set_timer(&bond_timer, 500);
            btstack_run_loop_set_timer_handler(&bond_timer, &Impl::connect_after_bonding);
            btstack_run_loop_add_timer(&bond_timer);
            break;
        }
        default:
            break;
        }
    }

    static void raw_l2cap_trampoline(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
        if (g_impl_ptr()) g_impl_ptr()->on_raw_l2cap(packet_type, channel, packet, size);
    }

    void start_raw(const Stored& dev, bool quiet = false) {
        rawlink = RawLink{};
        rawlink.active = true;
        rawlink.quiet = quiet;
        rawlink.dev = dev;
        bd_addr_t a;
        std::memcpy(a, dev.addr, 6);
        const uint8_t rc = l2cap_create_channel(&Impl::raw_l2cap_trampoline, a, PSM_HID_CONTROL, 672, &rawlink.ctrl_cid);
        if (rc != ERROR_CODE_SUCCESS) fail_raw(rc);
    }

    void fail_raw(uint8_t status) {
        if (rawlink.int_cid) l2cap_disconnect(rawlink.int_cid);
        if (rawlink.ctrl_cid) l2cap_disconnect(rawlink.ctrl_cid);
        const Stored dev = rawlink.dev;
        const bool quiet = rawlink.quiet;
        rawlink = RawLink{};
        if (quiet) return; // background attempt; the retry timer tries again later
        pending = false;
        emit(BtHidEventType::PairingFailed, dev, false, 0, 0, status);
        arm_reconnect();
    }

    // Bonded Classic pads: besides waiting for the pad to call us, page them periodically.
    static void classic_retry(btstack_timer_source_t*) {
        if (g_impl_ptr()) g_impl_ptr()->classic_reconnect_tick();
    }
    void schedule_classic_retry(uint32_t ms) {
        btstack_run_loop_remove_timer(&classic_retry_timer);
        btstack_run_loop_set_timer(&classic_retry_timer, ms);
        btstack_run_loop_set_timer_handler(&classic_retry_timer, &Impl::classic_retry);
        btstack_run_loop_add_timer(&classic_retry_timer);
    }
    // A2DP (or anything else) needs the radio: a background page can hold it for ~15 s when the
    // pad is off, which makes a headset connection attempt time out. Cancel the page in flight.
    static void abort_background_page(void*) {
        if (g_impl_ptr()) g_impl_ptr()->cancel_background_page();
    }
    void cancel_background_page() {
        if (!rawlink.active || !rawlink.quiet || rawlink.ctrl_open || !hci_can_send_command_packet_now()) return;
        bd_addr_t a;
        std::memcpy(a, rawlink.dev.addr, 6);
        hci_send_cmd(&hci_create_connection_cancel, a);
    }
    void classic_reconnect_tick() {
        if (!cfg().enable_classic || !cfg().auto_reconnect) return;
        schedule_classic_retry(8000);
        if (owner->m_pairing || pending || rawlink.active || BluetoothClassic::background_paging_inhibited()) return;
        for (size_t i = 0; i < stored_count; ++i) {
            if (stored[i].addr_type != kClassicAddrType || slot_by_addr(stored[i].addr)) continue;
            start_raw(stored[i], true);
            break;
        }
    }

    void on_raw_l2cap(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
        if (packet_type == L2CAP_DATA_PACKET) {
            if (!rawlink.active || channel != rawlink.int_cid || size < 2 || packet[0] != 0xA1) return;
            Slot* s = slot_by_classic_cid(rawlink.int_cid);
            if (!s) return;
            if (cfg().raw_report_callback) {
                const BtHidDevice d = to_public(s->dev, true);
                cfg().raw_report_callback(packet + 1, static_cast<uint16_t>(size - 1), d, cfg().user);
            }
            if (packet[1] == 0x03) // gamepad report of kRawPadDescriptor
                parse_report(*s, kRawPadDescriptor, sizeof(kRawPadDescriptor), packet + 1,
                             static_cast<uint16_t>(size - 1));
            return;
        }
        if (packet_type != HCI_EVENT_PACKET) return;
        switch (hci_event_packet_get_type(packet)) {
        case L2CAP_EVENT_CHANNEL_OPENED: {
            const uint16_t cid = l2cap_event_channel_opened_get_local_cid(packet);
            const uint8_t status = l2cap_event_channel_opened_get_status(packet);
            if (status != ERROR_CODE_SUCCESS) {
                if (rawlink.active) fail_raw(status);
                break;
            }
            if (cid == rawlink.ctrl_cid) {
                rawlink.ctrl_open = true;
                bd_addr_t a;
                std::memcpy(a, rawlink.dev.addr, 6);
                const uint8_t rc = l2cap_create_channel(&Impl::raw_l2cap_trampoline, a, PSM_HID_INTERRUPT, 672,
                                                        &rawlink.int_cid);
                if (rc != ERROR_CODE_SUCCESS) fail_raw(rc);
            } else if (cid == rawlink.int_cid) {
                Slot* s = alloc_slot();
                if (!s) { fail_raw(ERROR_CODE_MEMORY_CAPACITY_EXCEEDED); break; }
                s->classic = true;
                s->raw = true;
                s->classic_cid = rawlink.int_cid;
                s->descriptor_ready = true;
                s->dev = rawlink.dev;
                s->dev.addr_type = kClassicAddrType;
                s->dev.kinds = bt_hid_kind::Gamepad | kRawLinkFlag; // class of device said "gamepad"
                assign_pad_index(*s);
                remember(s->dev);
                pending = false;
                emit(BtHidEventType::Connected, s->dev, true);
            }
            break;
        }
        case L2CAP_EVENT_CHANNEL_CLOSED: {
            const uint16_t cid = l2cap_event_channel_closed_get_local_cid(packet);
            if (cid != rawlink.ctrl_cid && cid != rawlink.int_cid) break;
            const Stored dev = rawlink.dev;
            if (Slot* s = slot_by_classic_cid(rawlink.int_cid)) {
                clear_inputs_for(*s);
                *s = Slot{};
                emit(BtHidEventType::Disconnected, dev);
            }
            if (cid == rawlink.ctrl_cid && rawlink.int_cid) l2cap_disconnect(rawlink.int_cid);
            if (cid == rawlink.int_cid && rawlink.ctrl_cid) l2cap_disconnect(rawlink.ctrl_cid);
            rawlink = RawLink{};
            break;
        }
        default:
            break;
        }
    }

    void on_classic_hid(uint8_t* packet) {
        if (hci_event_packet_get_type(packet) != HCI_EVENT_HID_META) return;
        switch (hci_event_hid_meta_get_subevent_code(packet)) {
        case HID_SUBEVENT_INCOMING_CONNECTION: {
            bd_addr_t addr;
            hid_subevent_incoming_connection_get_address(packet, addr);
            const uint16_t cid = hid_subevent_incoming_connection_get_hid_cid(packet);
            {
                Stored in{};
                std::memcpy(in.addr, addr, 6);
                in.addr_type = kClassicAddrType;
                const int k = find_stored(addr);
                if (k >= 0) in = stored[k];
                emit(BtHidEventType::Connecting, in); // the device is calling us
            }
            // A known raw-link pad (stub SDP record) calling us: a normal accept makes hid_host query
            // that record, decline the pad's interrupt channel meanwhile, and this pad then sends its
            // reports on the channel it was refused. Boot protocol mode makes hid_host skip the SDP
            // query and take both of the pad's channels as they come; the reports are parsed with the
            // built-in descriptor (see HID_SUBEVENT_CONNECTION_OPENED / HID_SUBEVENT_REPORT).
            if (const int k = find_stored(addr); k >= 0 && (stored[k].kinds & kRawLinkFlag) &&
                hid_subevent_incoming_connection_get_status(packet) == ERROR_CODE_SUCCESS) {
                hid_host_accept_connection(cid, HID_PROTOCOL_MODE_BOOT);
                break;
            }
            // A known pad reconnecting, or any HID device while pairing mode is on.
            if (hid_subevent_incoming_connection_get_status(packet) == ERROR_CODE_SUCCESS &&
                (owner->m_pairing || pending || find_stored(addr) >= 0))
                hid_host_accept_connection(cid, HID_PROTOCOL_MODE_REPORT);
            else
                hid_host_decline_connection(cid);
            break;
        }
        case HID_SUBEVENT_CONNECTION_OPENED: {
            const uint8_t status = hid_subevent_connection_opened_get_status(packet);
            bd_addr_t addr;
            hid_subevent_connection_opened_get_bd_addr(packet, addr);
            if (status != ERROR_CODE_SUCCESS) {
                Stored dev = pending ? pending_dev : Stored{};
                std::memcpy(dev.addr, addr, 6);
                dev.addr_type = kClassicAddrType;
                // SDP gave no HID channels (broken SDP server of an old pad, now paired): fall
                // back to the standard HID PSMs.
                if (status == ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE &&
                    ((pending && pending_paired) || find_stored(addr) >= 0) && !rawlink.active) {
                    if (const int k = find_stored(addr); k >= 0 && !(stored[k].kinds & kRawLinkFlag)) {
                        stored[k].kinds |= kRawLinkFlag; // next time it calls us, skip hid_host
                        save_stored();
                    }
                    pending = true;
                    emit(BtHidEventType::Connecting, dev);
                    start_raw(dev);
                    break;
                }
                pending = false;
                emit(BtHidEventType::PairingFailed, dev, false, 0, 0, status);
                arm_reconnect();
                break;
            }
            Slot* s = alloc_slot();
            if (!s) {
                hid_host_disconnect(hid_subevent_connection_opened_get_hid_cid(packet));
                pending = false;
                break;
            }
            s->classic = true;
            s->classic_cid = hid_subevent_connection_opened_get_hid_cid(packet);
            const int si = find_stored(addr);
            if (si >= 0) s->dev = stored[si];
            else if (pending && std::memcmp(pending_dev.addr, addr, 6) == 0) s->dev = pending_dev;
            else std::memcpy(s->dev.addr, addr, 6);
            s->dev.addr_type = kClassicAddrType;
            if (si < 0 && !pending_paired) emit(BtHidEventType::Paired, s->dev); // device-initiated pairing
            pending_paired = false;
            pending = false;
            if (s->dev.kinds & kRawLinkFlag) { // accepted in boot mode: no descriptor will be read
                s->raw = true;
                s->descriptor_ready = true;
                s->dev.kinds = bt_hid_kind::Gamepad | kRawLinkFlag;
                assign_pad_index(*s);
                emit(BtHidEventType::Connected, s->dev, true);
            }
            break;
        }
        case HID_SUBEVENT_DESCRIPTOR_AVAILABLE: {
            Slot* s = slot_by_classic_cid(hid_subevent_descriptor_available_get_hid_cid(packet));
            if (!s || s->raw) break;
            if (hid_subevent_descriptor_available_get_status(packet) != ERROR_CODE_SUCCESS) {
                emit(BtHidEventType::PairingFailed, s->dev, false, 0, 0,
                     hid_subevent_descriptor_available_get_status(packet));
                hid_host_disconnect(s->classic_cid);
                break;
            }
            s->dev.kinds = scan_kinds(hid_descriptor_storage_get_descriptor_data(s->classic_cid),
                                      hid_descriptor_storage_get_descriptor_len(s->classic_cid));
            if (s->dev.kinds & bt_hid_kind::Gamepad) assign_pad_index(*s);
            s->descriptor_ready = true;
            remember(s->dev);
            emit(BtHidEventType::Connected, s->dev, true);
            break;
        }
        case HID_SUBEVENT_REPORT: {
            Slot* s = slot_by_classic_cid(hid_subevent_report_get_hid_cid(packet));
            if (!s || !s->descriptor_ready) break;
            const uint8_t* report = hid_subevent_report_get_report(packet);
            uint16_t len = hid_subevent_report_get_report_len(packet);
            if (len < 1 || report[0] != 0xA1) break; // HID DATA header, input report
            if (s->raw) {
                if (cfg().raw_report_callback) {
                    const BtHidDevice d = to_public(s->dev, true);
                    cfg().raw_report_callback(report + 1, static_cast<uint16_t>(len - 1), d, cfg().user);
                }
                if (len > 1 && report[1] == 0x03) // gamepad report of kRawPadDescriptor
                    parse_report(*s, kRawPadDescriptor, sizeof(kRawPadDescriptor), report + 1, len - 1);
                break;
            }
            parse_report(*s, hid_descriptor_storage_get_descriptor_data(s->classic_cid),
                         hid_descriptor_storage_get_descriptor_len(s->classic_cid), report + 1, len - 1);
            break;
        }
        case HID_SUBEVENT_CONNECTION_CLOSED: {
            Slot* s = slot_by_classic_cid(hid_subevent_connection_closed_get_hid_cid(packet));
            if (!s) break;
            clear_inputs_for(*s);
            const Stored dev = s->dev;
            *s = Slot{};
            emit(BtHidEventType::Disconnected, dev);
            break;
        }
        default:
            break;
        }
    }
#endif

    void assign_pad_index(Slot& s) {
        for (size_t i = 0; i < kMaxConnected && s.pad_index < 0; ++i) {
            bool taken = false;
            for (const auto& o : slots) taken |= (o.used && &o != &s && o.pad_index == static_cast<int>(i));
            if (!taken) s.pad_index = static_cast<int>(i);
        }
    }

    // ---- HID service ----
    void on_hids(uint8_t* packet) {
        if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;
        switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
        case GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED: {
            Slot* s = slot_by_cid(gattservice_subevent_hid_service_connected_get_hids_cid(packet));
            if (!s) break;
            const uint8_t status = gattservice_subevent_hid_service_connected_get_status(packet);
            if (status != ERROR_CODE_SUCCESS) {
                emit(BtHidEventType::PairingFailed, s->dev, false, 0, 0, status);
                gap_disconnect(s->handle);
                break;
            }
            const uint8_t n = gattservice_subevent_hid_service_connected_get_num_instances(packet);
            uint8_t kinds = 0;
            for (uint8_t i = 0; i < n; ++i)
                kinds |= scan_kinds(hids_host_descriptor_storage_get_descriptor_data(s->hids_cid, i),
                                    hids_host_descriptor_storage_get_descriptor_len(s->hids_cid, i));
            s->dev.kinds = kinds;
            if (kinds & bt_hid_kind::Gamepad) assign_pad_index(*s);
            remember(s->dev);
            emit(BtHidEventType::Connected, s->dev, true);
            break;
        }
        case GATTSERVICE_SUBEVENT_HID_REPORT: {
            Slot* s = slot_by_cid(gattservice_subevent_hid_report_get_hids_cid(packet));
            if (s) on_report(*s, gattservice_subevent_hid_report_get_service_index(packet),
                             gattservice_subevent_hid_report_get_report_id(packet),
                             gattservice_subevent_hid_report_get_report(packet),
                             gattservice_subevent_hid_report_get_report_len(packet));
            break;
        }
        default:
            break;
        }
    }

    // `report` is [report id][payload]: hids_host always prepends the id. The parser expects the
    // id byte only when the descriptor uses report ids (id != 0), so otherwise skip it.
    void on_report(Slot& s, uint8_t service_index, uint8_t report_id, const uint8_t* report, uint16_t len) {
        if (report_id == 0 && len > 0) { ++report; --len; }
        parse_report(s, hids_host_descriptor_storage_get_descriptor_data(s.hids_cid, service_index),
                     hids_host_descriptor_storage_get_descriptor_len(s.hids_cid, service_index), report, len);
    }

    // Updates keyboard / mouse / gamepad state from one input report, using the HID report
    // descriptor of the device (LE or Classic).
    void parse_report(Slot& s, const uint8_t* descriptor, uint16_t descriptor_len, const uint8_t* report,
                      uint16_t len) {
        btstack_hid_parser_t parser;
        btstack_hid_parser_init(&parser, descriptor, descriptor_len, HID_REPORT_TYPE_INPUT, report, len);

        bool has_keyboard = false, has_buttons = false;
        uint32_t key_bits[8] = {};
        uint8_t mods = 0;
        uint32_t raw_buttons = 0;
        int hat = -1;
        bool have[6] = {};     // lx ly rx ry lt rt
        uint8_t axis[6] = {};
        int dx = 0, dy = 0, wheel = 0;
        const bool pad_dev = (s.dev.kinds & bt_hid_kind::Gamepad) && s.pad_index >= 0;
        const bool mouse_dev = (s.dev.kinds & bt_hid_kind::Mouse) && !pad_dev;

        while (btstack_hid_parser_has_more(&parser)) {
            uint16_t page, usage;
            int32_t value;
            btstack_hid_parser_get_field(&parser, &page, &usage, &value);
            const uint8_t size = parser.descriptor_usage_item.size;
            const int32_t lmin = parser.descriptor_usage_item.global_logical_minimum;

            if (page == kPageKeyboard) {
                has_keyboard = true;
                if (usage >= 0xE0 && usage <= 0xE7) {
                    if (value) mods |= static_cast<uint8_t>(1u << (usage - 0xE0));
                } else if (usage != 0 && usage < 256 && value) {
                    key_bits[usage >> 5] |= 1u << (usage & 31);
                }
            } else if (page == kPageButton) {
                has_buttons = true;
                if (value && usage >= 1 && usage <= 32) raw_buttons |= 1u << (usage - 1);
            } else if (page == kPageDesktop) {
                switch (usage) {
                case 0x30:
                    if (mouse_dev) dx = value; else { axis[0] = norm_axis(value, size, lmin); have[0] = true; }
                    break;
                case 0x31:
                    if (mouse_dev) dy = value; else { axis[1] = norm_axis(value, size, lmin); have[1] = true; }
                    break;
                case 0x32: axis[2] = norm_axis(value, size, lmin); have[2] = true; break; // Z: right stick X
                case 0x35: axis[3] = norm_axis(value, size, lmin); have[3] = true; break; // Rz: right stick Y
                case 0x38: wheel = value; break;
                case 0x39: {
                    const int h = value - lmin;
                    hat = (h >= 0 && h <= 7) ? h : -1;
                    break;
                }
                default: break;
                }
            } else if (page == kPageSim) {
                if (usage == 0xC5) { axis[4] = norm_axis(value, size, lmin); have[4] = true; } // brake: left trigger
                else if (usage == 0xC4) { axis[5] = norm_axis(value, size, lmin); have[5] = true; } // accelerator
            }
        }

        if (has_keyboard) {
            for (int i = 0; i < 8; ++i) {
                key_edges[i] = key_edges[i] | (key_bits[i] & ~keys[i]);
                keys[i] = key_bits[i];
            }
            modifiers = mods;
        } else if (pad_dev) {
            GamepadState& g = pads[s.pad_index];
            g.present = true;
            uint16_t b = 0;
            for (const auto& m : kButtonMap) if (raw_buttons & (1u << (m.usage - 1))) b |= m.mask;
            if (hat >= 0) {
                if (hat == 7 || hat == 0 || hat == 1) b |= kBtUp;
                if (hat >= 1 && hat <= 3) b |= kBtRight;
                if (hat >= 3 && hat <= 5) b |= kBtDown;
                if (hat >= 5 && hat <= 7) b |= kBtLeft;
            }
            g.buttons = b;
            if (have[0]) g.lx = axis[0];
            if (have[1]) g.ly = axis[1];
            if (have[2]) g.rx = axis[2];
            if (have[3]) g.ry = axis[3];
            if (have[4]) g.lt = axis[4];
            if (have[5]) g.rt = axis[5];
            pad_raw[s.pad_index] = raw_buttons;
        } else if (mouse_dev) {
            mouse_dx = mouse_dx + dx;
            mouse_dy = mouse_dy + dy;
            mouse_wheel = mouse_wheel + wheel;
            mouse_buttons[0] = raw_buttons & 1;
            mouse_buttons[1] = raw_buttons & 2;
            mouse_buttons[2] = raw_buttons & 4;
        }
        (void)has_buttons;
    }
};

// ---- BluetoothHid ------------------------------------------------------------

namespace {
BluetoothHid* g_owner = nullptr;

void hci_trampoline(uint8_t type, uint16_t, uint8_t* packet, uint16_t) {
    if (type == HCI_EVENT_PACKET && g_owner) g_owner->on_hci_event(packet);
}
#ifdef ENABLE_CLASSIC
void classic_hid_trampoline(uint8_t type, uint16_t, uint8_t* packet, uint16_t) {
    if (type == HCI_EVENT_PACKET && g_owner) g_owner->on_hci_event(packet); // HID meta events too
}
#endif
void sm_trampoline(uint8_t type, uint16_t, uint8_t* packet, uint16_t) {
    if (type == HCI_EVENT_PACKET && g_owner) g_owner->on_sm_event(packet);
}
} // namespace

bool BluetoothHid::init(const BluetoothHidConfig& config) {
    if (m_initialised) return true;
    if (hci_get_state() != HCI_STATE_WORKING) return false; // Bluetooth::init() first
    m_cfg = config;
    m_impl = new Impl;
    m_impl->owner = this;
    Impl::g_impl_ptr() = m_impl;
    g_owner = this;

    ContextLock lock;
    gatt_client_init();
    hids_host_init(g_descriptor_storage, sizeof(g_descriptor_storage));
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY); // keyboards: show the passkey, user types it
    // Bonding + MITM + LE Secure Connections: most keyboards refuse an unauthenticated
    // legacy pairing. Mice/gamepads (no I/O) still fall back to Just Works.
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING | SM_AUTHREQ_MITM_PROTECTION |
                                       SM_AUTHREQ_SECURE_CONNECTION);
    // BTstack turns on "Secure Connections only" in sm_init() when SC support is compiled
    // in, which rejects (pairing failed, reason 0x03) every keyboard/gamepad that only
    // speaks legacy LE pairing -- e.g. the Microsoft foldable keyboard. Allow both.
    sm_set_secure_connections_only_mode(false);

    m_impl->hci_reg.callback = &hci_trampoline;
    hci_add_event_handler(&m_impl->hci_reg);
    m_impl->sm_reg.callback = &sm_trampoline;
    sm_add_event_handler(&m_impl->sm_reg);

#ifdef ENABLE_CLASSIC
    if (m_cfg.enable_classic) {
        hid_host_init(g_classic_descriptor_storage, sizeof(g_classic_descriptor_storage));
        hid_host_register_packet_handler(&classic_hid_trampoline);
        // GAP defaults, PIN / SSP answers and inquiry are shared with other Classic users.
        BluetoothClassic::init(m_cfg.pin);
        BluetoothClassic::add_passkey_listener(&Impl::passkey_trampoline, m_impl);
        BluetoothClassic::add_pairing_gate(
            [](void*) { Impl* im = Impl::g_impl_ptr(); return im && (im->owner->m_pairing || im->pending); }, nullptr);
        BluetoothClassic::set_paging_inhibit_listener(&Impl::abort_background_page, nullptr);
    }
#endif

    m_impl->load_stored();
    m_initialised = true;
    m_impl->arm_reconnect();
#ifdef ENABLE_CLASSIC
    if (m_cfg.enable_classic && m_cfg.auto_reconnect) m_impl->schedule_classic_retry(3000);
#endif
    return true;
}

void BluetoothHid::on_hci_event(uint8_t* packet) {
    switch (hci_event_packet_get_type(packet)) {
    case GAP_EVENT_ADVERTISING_REPORT:
        m_impl->on_advertisement(packet);
        break;
    case HCI_EVENT_META_GAP:
        if (hci_event_gap_meta_get_subevent_code(packet) == GAP_SUBEVENT_LE_CONNECTION_COMPLETE)
            m_impl->on_connection_complete(packet);
        break;
    case HCI_EVENT_DISCONNECTION_COMPLETE:
        m_impl->on_disconnection(packet);
        break;
    default:
#ifdef ENABLE_CLASSIC
        if (m_cfg.enable_classic) {
            m_impl->on_classic_hci(packet);
            m_impl->on_classic_hid(packet);
        }
#endif
        break;
    }
}

void BluetoothHid::on_sm_event(uint8_t* packet) { m_impl->on_sm(packet); }
void BluetoothHid::on_hids_event(uint8_t* packet) { m_impl->on_hids(packet); }

void BluetoothHid::start_pairing() {
    if (!m_initialised || m_pairing) return;
    ContextLock lock;
    m_impl->begin_scan();
}

void BluetoothHid::stop_pairing() {
    if (!m_initialised) return;
    ContextLock lock;
    m_impl->end_scan();
}

bool BluetoothHid::pair(const uint8_t addr[6], uint8_t addr_type) {
    if (!m_initialised || m_impl->pending) return false;
    Stored dev{};
    std::memcpy(dev.addr, addr, 6);
    dev.addr_type = addr_type;
    ContextLock lock;
    m_impl->start_connect(dev);
    return true;
}

size_t BluetoothHid::bonded_devices(BtHidDevice* out, size_t max) const {
    if (!m_initialised) return 0;
    ContextLock lock;
    size_t n = 0;
    for (size_t i = 0; i < m_impl->stored_count && n < max; ++i)
        out[n++] = to_public(m_impl->stored[i], m_impl->slot_by_addr(m_impl->stored[i].addr) != nullptr);
    return n;
}

bool BluetoothHid::forget(const uint8_t addr[6]) {
    if (!m_initialised) return false;
    ContextLock lock;
    Impl& im = *m_impl;
    const int i = im.find_stored(addr);
    if (i < 0) return false;
    const Stored dev = im.stored[i];
    if (Slot* s = im.slot_by_addr(addr)) {
        if (dev.addr_type == kClassicAddrType) {
#ifdef ENABLE_CLASSIC
            hid_host_disconnect(s->classic_cid);
#endif
        } else {
            gap_disconnect(s->handle);
        }
    }
    im.drop_bond(dev);
    for (size_t k = static_cast<size_t>(i) + 1; k < im.stored_count; ++k) im.stored[k - 1] = im.stored[k];
    --im.stored_count;
    im.save_stored();
    im.emit(BtHidEventType::Forgotten, dev);
    return true;
}

void BluetoothHid::forget_all() {
    if (!m_initialised) return;
    uint8_t addrs[kMaxBonded][6];
    size_t n;
    {
        ContextLock lock;
        n = m_impl->stored_count;
        for (size_t i = 0; i < n; ++i) std::memcpy(addrs[i], m_impl->stored[i].addr, 6);
    }
    for (size_t i = 0; i < n; ++i) forget(addrs[i]);
}

bool BluetoothHid::is_key_down(uint8_t usage) const {
    return m_impl && (m_impl->keys[usage >> 5] & (1u << (usage & 31)));
}
bool BluetoothHid::is_modifier_down(uint8_t mask) const { return m_impl && (m_impl->modifiers & mask); }
bool BluetoothHid::consume_key_press(uint8_t usage) {
    if (!m_impl) return false;
    ContextLock lock; // the edge bits are set from the BTstack context: no lost presses
    const uint32_t bit = 1u << (usage & 31);
    if (!(m_impl->key_edges[usage >> 5] & bit)) return false;
    m_impl->key_edges[usage >> 5] = m_impl->key_edges[usage >> 5] & ~bit;
    return true;
}

BluetoothHid::MouseState BluetoothHid::mouse_state() const {
    MouseState m;
    if (!m_impl) return m;
    m.present = connected_mouse_count() > 0;
    m.left = m_impl->mouse_buttons[0];
    m.right = m_impl->mouse_buttons[1];
    m.middle = m_impl->mouse_buttons[2];
    return m;
}
void BluetoothHid::consume_mouse_delta(int& dx, int& dy) {
    if (!m_impl) { dx = dy = 0; return; }
    ContextLock lock;
    dx = m_impl->mouse_dx; dy = m_impl->mouse_dy;
    m_impl->mouse_dx = 0; m_impl->mouse_dy = 0;
}
int BluetoothHid::consume_mouse_wheel() {
    if (!m_impl) return 0;
    ContextLock lock;
    const int w = m_impl->mouse_wheel;
    m_impl->mouse_wheel = 0;
    return w;
}

GamepadState BluetoothHid::gamepad_state(size_t index) const {
    if (!m_impl || index >= kMaxConnected) return GamepadState{};
    ContextLock lock; // a consistent copy, not one torn by a report arriving meanwhile
    return m_impl->pads[index];
}
uint32_t BluetoothHid::gamepad_raw_buttons(size_t index) const {
    return (m_impl && index < kMaxConnected) ? m_impl->pad_raw[index] : 0;
}

namespace {
uint8_t count_kind(const BluetoothHid::Impl* im, uint8_t kind);
}

uint8_t BluetoothHid::connected_keyboard_count() const { return m_impl ? count_kind(m_impl, bt_hid_kind::Keyboard) : 0; }
uint8_t BluetoothHid::connected_mouse_count() const { return m_impl ? count_kind(m_impl, bt_hid_kind::Mouse) : 0; }
uint8_t BluetoothHid::connected_gamepad_count() const { return m_impl ? count_kind(m_impl, bt_hid_kind::Gamepad) : 0; }

namespace {
uint8_t count_kind(const BluetoothHid::Impl* im, uint8_t kind) {
    uint8_t n = 0;
    for (const auto& s : im->slots) n += (s.used && (s.dev.kinds & kind)) ? 1 : 0;
    return n;
}
} // namespace

} // namespace pico_toolset
