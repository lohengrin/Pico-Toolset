#ifdef ENABLE_CLASSIC

#include "pico_toolset/bluetooth_classic.h"

#include <cstring>

#include "btstack.h"

namespace pico_toolset {

namespace {

constexpr int kMaxPasskeyListeners = 2;
constexpr int kMaxInquiryClients = 2;

struct State {
    bool initialised = false;
    const char* pin = "0000";
    btstack_packet_callback_registration_t hci_reg{};
    struct { ClassicPasskeyCallback cb; void* user; } passkey[kMaxPasskeyListeners]{};
    struct InquiryClient { ClassicInquiryCallback cb; void* user; uint8_t major_class; } clients[kMaxInquiryClients]{};
    bool inquiry_running = false; // the radio inquiry
    uint8_t duration = 8;
    bool paging_inhibited = false;
    void (*inhibit_cb)(void*) = nullptr;
    void* inhibit_user = nullptr;
};
State g;

bool any_client() {
    for (const auto& c : g.clients)
        if (c.cb) return true;
    return false;
}

void start_radio_inquiry() {
    if (g.inquiry_running || !any_client()) return;
    if (gap_inquiry_start(g.duration) == 0) g.inquiry_running = true;
}

void handle_inquiry_result(const uint8_t* packet) {
    if (!any_client()) return;
    const uint32_t cod = gap_event_inquiry_result_get_class_of_device(packet);
    ClassicInquiryResult r{};
    bd_addr_t addr;
    gap_event_inquiry_result_get_bd_addr(packet, addr);
    std::memcpy(r.addr, addr, 6);
    r.class_of_device = cod;
    r.rssi_valid = gap_event_inquiry_result_get_rssi_available(packet);
    if (r.rssi_valid) r.rssi = static_cast<int8_t>(gap_event_inquiry_result_get_rssi(packet));
    if (gap_event_inquiry_result_get_name_available(packet)) {
        const uint8_t n = gap_event_inquiry_result_get_name_len(packet);
        const size_t m = n < sizeof(r.name) - 1 ? n : sizeof(r.name) - 1;
        std::memcpy(r.name, gap_event_inquiry_result_get_name(packet), m);
    }
    if (gap_event_inquiry_result_get_device_id_available(packet)) {
        r.vendor_id = gap_event_inquiry_result_get_device_id_vendor_id(packet);
        r.product_id = gap_event_inquiry_result_get_device_id_product_id(packet);
    }
    for (const auto& c : g.clients)
        if (c.cb && (!c.major_class || ((cod >> 8) & 0x1F) == c.major_class)) c.cb(r, c.user);
}

void hci_handler(uint8_t type, uint16_t, uint8_t* packet, uint16_t) {
    if (type != HCI_EVENT_PACKET) return;
    bd_addr_t addr;
    switch (hci_event_packet_get_type(packet)) {
    case GAP_EVENT_INQUIRY_RESULT:
        handle_inquiry_result(packet);
        break;
    case GAP_EVENT_INQUIRY_COMPLETE:
        g.inquiry_running = false;
        start_radio_inquiry(); // another round while a client is still looking
        break;
    case HCI_EVENT_PIN_CODE_REQUEST: // legacy pairing (pre Bluetooth 2.1 devices): fixed PIN
        hci_event_pin_code_request_get_bd_addr(packet, addr);
        gap_pin_code_response(addr, g.pin ? g.pin : "0000");
        break;
    case HCI_EVENT_USER_CONFIRMATION_REQUEST: // SSP numeric comparison / just works: accept
        hci_event_user_confirmation_request_get_bd_addr(packet, addr);
        gap_ssp_confirmation_response(addr);
        break;
    case HCI_EVENT_USER_PASSKEY_NOTIFICATION: // a keyboard: the user types this code on it
        hci_event_user_passkey_notification_get_bd_addr(packet, addr);
        for (auto& l : g.passkey)
            if (l.cb) l.cb(addr, hci_event_user_passkey_notification_get_numeric_value(packet), l.user);
        break;
    case HCI_EVENT_USER_PASSKEY_REQUEST: // we have no keyboard to type the code ourselves
        hci_event_user_passkey_request_get_bd_addr(packet, addr);
        gap_ssp_passkey_negative(addr);
        break;
    default:
        break;
    }
}

} // namespace

void BluetoothClassic::init(const char* pin) {
    if (pin) g.pin = pin;
    if (g.initialised) return;
    g.initialised = true;
    // Let the remote use sniff mode and make us the master, as BTstack's HID and A2DP demos do.
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);
    hci_set_master_slave_policy(HCI_ROLE_MASTER);
    gap_set_bondable_mode(1);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_ONLY);
    gap_ssp_set_authentication_requirement(SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_GENERAL_BONDING);
    // BTstack's default page timeout is ~15 s: a bonded device that is switched off then keeps the
    // radio busy that long on every reconnect attempt, starving other links (a gamepad's reports
    // stopped while the headset was being paged). 5.12 s is the Bluetooth default.
    gap_set_page_timeout(0x2000);
    gap_connectable_control(1); // bonded devices reconnect on their own
    hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR); // names and device ids in inquiry results
    g.hci_reg.callback = &hci_handler;
    hci_add_event_handler(&g.hci_reg);
}

void BluetoothClassic::add_passkey_listener(ClassicPasskeyCallback callback, void* user) {
    for (auto& l : g.passkey) {
        if (l.cb == callback && l.user == user) return;
        if (!l.cb) { l.cb = callback; l.user = user; return; }
    }
}

bool BluetoothClassic::start_inquiry(uint8_t duration_1280ms, uint8_t major_class,
                                     ClassicInquiryCallback callback, void* user) {
    if (!callback) return false;
    if (duration_1280ms < 1) duration_1280ms = 1;
    if (duration_1280ms > 48) duration_1280ms = 48;
    State::InquiryClient* slot = nullptr;
    for (auto& c : g.clients) {
        if (c.cb == callback) { slot = &c; break; }
        if (!c.cb && !slot) slot = &c;
    }
    if (!slot) return false;
    *slot = {callback, user, major_class};
    g.duration = duration_1280ms;
    start_radio_inquiry(); // a no-op while one is running: the new client just listens in
    return true;
}

void BluetoothClassic::stop_inquiry(ClassicInquiryCallback callback) {
    for (auto& c : g.clients)
        if (c.cb == callback) c = {};
    if (!any_client() && g.inquiry_running) gap_inquiry_stop();
}

bool BluetoothClassic::inquiry_active() { return any_client(); }

void BluetoothClassic::inhibit_background_paging(bool inhibit) {
    const bool started = inhibit && !g.paging_inhibited;
    g.paging_inhibited = inhibit;
    if (started && g.inhibit_cb) g.inhibit_cb(g.inhibit_user);
}
void BluetoothClassic::set_paging_inhibit_listener(void (*callback)(void*), void* user) {
    g.inhibit_cb = callback;
    g.inhibit_user = user;
}
bool BluetoothClassic::background_paging_inhibited() { return g.paging_inhibited; }

} // namespace pico_toolset

#endif // ENABLE_CLASSIC
