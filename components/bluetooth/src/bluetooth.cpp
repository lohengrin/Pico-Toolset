#include "pico_toolset/bluetooth.h"
#include "pico_toolset/bluetooth_detail.h"

#include <cstring>

#include "btstack.h"
#include "pico/async_context.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

namespace pico_toolset {

namespace {

Bluetooth* g_instance = nullptr;
btstack_packet_callback_registration_t g_hci_cb;

using bt_detail::ContextLock;

void packet_handler(uint8_t packet_type, uint16_t, uint8_t* packet, uint16_t) {
    if (g_instance) g_instance->on_event(packet_type, packet);
}

uint16_t ms_to_units(uint16_t ms) { return static_cast<uint16_t>(ms * 1000u / 625u); } // 0.625 ms units

} // namespace

void Bluetooth::on_event(uint8_t packet_type, const uint8_t* packet) {
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(const_cast<uint8_t*>(packet))) {
    case BTSTACK_EVENT_STATE:
        m_ready = btstack_event_state_get_state(const_cast<uint8_t*>(packet)) == HCI_STATE_WORKING;
        break;
    case GAP_EVENT_ADVERTISING_REPORT: {
        if (!m_scan_cb) break;
        auto* p = const_cast<uint8_t*>(packet);
        BleAdvertisement adv{};
        bd_addr_t addr;
        gap_event_advertising_report_get_address(p, addr);
        std::memcpy(adv.addr, addr, 6);
        adv.addr_type = gap_event_advertising_report_get_address_type(p);
        adv.rssi = static_cast<int8_t>(gap_event_advertising_report_get_rssi(p));
        bt_detail::extract_local_name(gap_event_advertising_report_get_data(p),
                                      gap_event_advertising_report_get_data_length(p), adv.name, sizeof(adv.name));
        m_scan_cb(adv, m_scan_user);
        break;
    }
    default:
        break;
    }
}

bool Bluetooth::init(const BluetoothConfig& config) {
    m_cfg = config;
    g_instance = this;
    m_ready = false;

    {
        ContextLock lock;
        if (!m_initialised) {
            l2cap_init();
            sm_init();
            g_hci_cb.callback = &packet_handler;
            hci_add_event_handler(&g_hci_cb);
            m_initialised = true;
        }
        hci_power_control(HCI_POWER_ON);
    }

    for (int i = 0; i < 100 && !m_ready; ++i) sleep_ms(50);
    return m_ready;
}

void Bluetooth::start_advertising() {
    uint8_t data[31];
    uint8_t len = 0;
    data[len++] = 2; data[len++] = BLUETOOTH_DATA_TYPE_FLAGS; data[len++] = 0x06; // LE general discoverable, no BR/EDR
    const size_t name_len = std::strlen(m_cfg.name) < 27 ? std::strlen(m_cfg.name) : 27;
    data[len++] = static_cast<uint8_t>(name_len + 1);
    data[len++] = BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME;
    std::memcpy(&data[len], m_cfg.name, name_len);
    len += static_cast<uint8_t>(name_len);

    bd_addr_t null_addr{};
    ContextLock lock;
    const uint16_t interval = ms_to_units(m_cfg.adv_interval_ms);
    // 0x03 = ADV_NONCONN_IND: broadcast only, so no ATT/GATT database needed.
    gap_advertisements_set_params(interval, interval, 0x03, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(len, data);
    gap_advertisements_enable(1);
}

void Bluetooth::stop_advertising() {
    ContextLock lock;
    gap_advertisements_enable(0);
}

void Bluetooth::start_scan(BleScanCallback callback, void* user) {
    ContextLock lock;
    m_scan_cb = callback;
    m_scan_user = user;
    const uint16_t interval = ms_to_units(m_cfg.scan_interval_ms);
    gap_set_scan_params(m_cfg.active_scan ? 1 : 0, interval, interval, 0);
    gap_start_scan();
}

void Bluetooth::stop_scan() {
    ContextLock lock;
    gap_stop_scan();
    m_scan_cb = nullptr;
}

} // namespace pico_toolset
