#include "pico_toolset/bluetooth_page_text.h"

#include <cstdio>

namespace pico_toolset {

const char* bt_ui_kind_tag(BtUiKind k) {
    switch (k) {
    case BtUiKind::Keyboard: return "KBD";
    case BtUiKind::Mouse: return "MOUSE";
    case BtUiKind::KeyboardMouse: return "K+M";
    case BtUiKind::Gamepad: return "PAD";
    case BtUiKind::Headset: return "AUD";
    default: return "DEV";
    }
}

BtUiTone bt_ui_state_tone(BtUiState s) {
    switch (s) {
    case BtUiState::Connected:
    case BtUiState::Streaming: return BtUiTone::Ok;
    case BtUiState::Connecting: return BtUiTone::Warn;
    default: return BtUiTone::Dim;
    }
}

void bt_ui_display_name(const BtUiDevice& dev, char* out, size_t cap) {
    if (dev.name[0]) std::snprintf(out, cap, "%s", dev.name);
    else std::snprintf(out, cap, "%02X:%02X:%02X", dev.addr[3], dev.addr[4], dev.addr[5]);
}

const char* bt_ui_action_label(BtUiAction a, BtUiScreen screen, bool shortened) {
    switch (a) {
    case BtUiAction::StartScan: return screen == BtUiScreen::Scan ? "SCAN" : (shortened ? "PAIR" : "PAIR NEW");
    case BtUiAction::StopScan: return "STOP";
    case BtUiAction::Close: return "CLOSE";
    case BtUiAction::Back: return "BACK";
    case BtUiAction::Connect: return shortened ? "CONN" : "CONNECT";
    case BtUiAction::Disconnect: return shortened ? "DISC" : "DISCONNECT";
    case BtUiAction::Forget: return "FORGET";
    case BtUiAction::ConfirmForget: return "YES";
    case BtUiAction::Cancel: return "NO";
    default: return "";
    }
}

void bt_ui_page_title(const BluetoothPageController& page, const char* default_title, char* out, size_t cap) {
    switch (page.screen()) {
    case BtUiScreen::Scan: std::snprintf(out, cap, "Pair new"); break;
    case BtUiScreen::Detail: bt_ui_display_name(page.selected(), out, cap); break;
    case BtUiScreen::ConfirmForget: std::snprintf(out, cap, "Forget?"); break;
    default:
        if (page.bonded_count() > 0) std::snprintf(out, cap, "%s (%u)", default_title, static_cast<unsigned>(page.bonded_count()));
        else std::snprintf(out, cap, "%s", default_title);
    }
}

BtUiText bt_ui_status(const BluetoothPageController& page, const char* idle_hint) {
    BtUiText s{"", BtUiTone::Dim};
    uint32_t code = 0;
    if (page.passkey(&code)) { std::snprintf(s.text, sizeof(s.text), "Type %06lu on the device", static_cast<unsigned long>(code)); s.tone = BtUiTone::Warn; }
    else if (page.connecting()) { std::snprintf(s.text, sizeof(s.text), "Connecting..."); s.tone = BtUiTone::Warn; }
    else if (page.notice() == BtUiNotice::Paired) { std::snprintf(s.text, sizeof(s.text), "Paired"); s.tone = BtUiTone::Ok; }
    else if (page.notice() == BtUiNotice::PairFailed) { std::snprintf(s.text, sizeof(s.text), "Failed (0x%02X)", page.error()); s.tone = BtUiTone::Bad; }
    else if (page.notice() == BtUiNotice::Forgotten) { std::snprintf(s.text, sizeof(s.text), "Forgotten"); s.tone = BtUiTone::Ok; }
    else if (page.scanning()) std::snprintf(s.text, sizeof(s.text), "Scanning... %u found", static_cast<unsigned>(page.found_count()));
    else if (page.screen() == BtUiScreen::Scan) std::snprintf(s.text, sizeof(s.text), "Scan ended");
    else std::snprintf(s.text, sizeof(s.text), "%s", idle_hint);
    return s;
}

void bt_ui_row_state(const BtUiItem& item, const BtUiDevice& dev, char* out, size_t cap) {
    out[0] = '\0';
    if (item.action == BtUiAction::OpenDevice) std::snprintf(out, cap, "%s", bt_ui_state_name(dev.state));
    else if (item.action == BtUiAction::PickFound && dev.rssi != 0) std::snprintf(out, cap, "%d dBm", dev.rssi);
}

BtUiTone bt_ui_row_state_tone(const BtUiItem& item, const BtUiDevice& dev) {
    return item.action == BtUiAction::OpenDevice ? bt_ui_state_tone(dev.state) : BtUiTone::Normal;
}

size_t bt_ui_info_lines(const BluetoothPageController& page, BtUiText* out, size_t max) {
    size_t n = 0;
    auto add = [&](BtUiTone tone, const char* fmt, auto... args) {
        if (n >= max) return;
        std::snprintf(out[n].text, sizeof(out[n].text), fmt, args...);
        out[n].tone = tone;
        ++n;
    };
    const BtUiScreen screen = page.screen();
    if (screen == BtUiScreen::Detail) {
        const BtUiDevice& dev = page.selected();
        add(BtUiTone::Normal, "Kind: %s", bt_ui_kind_name(dev.kind));
        add(bt_ui_state_tone(dev.state), "State: %s", bt_ui_state_name(dev.state));
        add(BtUiTone::Dim, "%02X:%02X:%02X:%02X:%02X:%02X", dev.addr[0], dev.addr[1], dev.addr[2], dev.addr[3], dev.addr[4], dev.addr[5]);
    } else if (screen == BtUiScreen::ConfirmForget) {
        char name[32];
        bt_ui_display_name(page.selected(), name, sizeof(name));
        add(BtUiTone::Normal, "Forget %s?", name);
        add(BtUiTone::Dim, "%s", "Pair it again to use it.");
    } else {
        for (size_t i = 0; i < page.item_count(); ++i)
            if (page.item(i).action == BtUiAction::OpenDevice || page.item(i).action == BtUiAction::PickFound) return 0;
        if (screen == BtUiScreen::Scan) {
            add(BtUiTone::Normal, "%s", page.scanning() ? "Looking for devices..." : "Nothing found.");
            add(BtUiTone::Dim, "%s", "Put the device in pairing mode.");
        } else {
            add(BtUiTone::Dim, "%s", "No paired devices.");
        }
    }
    return n;
}

} // namespace pico_toolset
