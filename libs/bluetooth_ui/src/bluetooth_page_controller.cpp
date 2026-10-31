#include "pico_toolset/bluetooth_page_controller.h"

namespace pico_toolset {

void BluetoothPageController::open() {
    closed_ = false;
    notice_ = BtUiNotice::None;
    pending_ = false;
    go(BtUiScreen::List);
}

bool BluetoothPageController::passkey(uint32_t* code) const {
    if (has_passkey_ && code) *code = passkey_;
    return has_passkey_;
}

void BluetoothPageController::go(BtUiScreen screen) {
    if (screen_ == BtUiScreen::Scan && screen != BtUiScreen::Scan && devices_.scanning()) devices_.stop_scan();
    const bool entering_scan = screen == BtUiScreen::Scan && screen_ != BtUiScreen::Scan;
    screen_ = screen;
    if (entering_scan) {
        notice_ = BtUiNotice::None;
        devices_.start_scan();
    }
    have_counter_ = false; // force a refresh
    refresh();
    rebuild_items(false);
    focus_ = screen == BtUiScreen::ConfirmForget && item_count_ > 1 ? 1 : 0;
    dirty_ = true;
}

void BluetoothPageController::refresh() {
    bonded_count_ = devices_.bonded(bonded_, BluetoothDevices::kMaxDevices);
    found_count_ = screen_ == BtUiScreen::Scan ? devices_.found(found_, BluetoothDevices::kMaxFound) : 0;
    scanning_ = devices_.scanning();
    connecting_ = devices_.connecting();
    has_passkey_ = devices_.passkey(&passkey_);
    last_counter_ = devices_.change_counter();
    have_counter_ = true;

    if (screen_ == BtUiScreen::Detail || screen_ == BtUiScreen::ConfirmForget) {
        bool present = false;
        for (size_t i = 0; i < bonded_count_; ++i) {
            if (bt_ui_same_device(bonded_[i], detail_)) {
                detail_ = bonded_[i];
                present = true;
            }
        }
        if (!present) { // forgotten by the page itself, or from outside
            if (notice_ == BtUiNotice::None) notice_ = BtUiNotice::Forgotten;
            screen_ = BtUiScreen::List;
        }
    }

    if (pending_) {
        bool paired = false;
        for (size_t i = 0; i < bonded_count_; ++i) paired |= bt_ui_same_device(bonded_[i], pending_device_);
        if (paired) {
            pending_ = false;
            notice_ = BtUiNotice::Paired;
            if (screen_ == BtUiScreen::Scan) {
                if (devices_.scanning()) devices_.stop_scan();
                screen_ = BtUiScreen::List;
            }
        } else if (!connecting_ && devices_.last_error() != 0) {
            pending_ = false;
            notice_ = BtUiNotice::PairFailed;
            error_ = devices_.last_error();
        }
    }
}

void BluetoothPageController::rebuild_items(bool keep_focus) {
    const BtUiItem before = item_count_ ? items_[focus_] : BtUiItem{BtUiAction::Close, -1};
    size_t n = 0;
    auto add = [&](BtUiAction a, int idx = -1) { items_[n++] = BtUiItem{a, static_cast<int8_t>(idx)}; };
    switch (screen_) {
    case BtUiScreen::List:
        for (size_t i = 0; i < bonded_count_; ++i) add(BtUiAction::OpenDevice, static_cast<int>(i));
        add(BtUiAction::StartScan);
        add(BtUiAction::Close);
        break;
    case BtUiScreen::Scan:
        for (size_t i = 0; i < found_count_; ++i) add(BtUiAction::PickFound, static_cast<int>(i));
        add(scanning_ ? BtUiAction::StopScan : BtUiAction::StartScan);
        add(BtUiAction::Back);
        break;
    case BtUiScreen::Detail:
        if (detail_.state == BtUiState::Offline) add(BtUiAction::Connect);
        else if (devices_.can_disconnect(detail_)) add(BtUiAction::Disconnect);
        add(BtUiAction::Forget);
        add(BtUiAction::Back);
        break;
    case BtUiScreen::ConfirmForget:
        add(BtUiAction::ConfirmForget);
        add(BtUiAction::Cancel);
        break;
    }
    item_count_ = n;
    if (keep_focus) {
        for (size_t i = 0; i < n; ++i) {
            if (items_[i].action == before.action && items_[i].index == before.index) { focus_ = i; return; }
        }
    }
    if (focus_ >= n) focus_ = n ? n - 1 : 0;
}

bool BluetoothPageController::update() {
    const BtUiScreen screen_before = screen_;
    if (!have_counter_ || devices_.change_counter() != last_counter_) {
        refresh();
        rebuild_items(true);
        dirty_ = true;
        if (screen_ != screen_before) { // the screen changed by itself (paired, forgotten)
            have_counter_ = false;
            refresh();
            rebuild_items(false);
            focus_ = 0;
        }
    }
    const bool changed = dirty_;
    dirty_ = false;
    return changed;
}

void BluetoothPageController::set_focus(size_t item) {
    if (item < item_count_ && item != focus_) { focus_ = item; dirty_ = true; }
}

void BluetoothPageController::back() {
    switch (screen_) {
    case BtUiScreen::List:
        closed_ = true;
        dirty_ = true;
        break;
    case BtUiScreen::Scan:
    case BtUiScreen::Detail: go(BtUiScreen::List); break;
    case BtUiScreen::ConfirmForget: go(BtUiScreen::Detail); break;
    }
}

void BluetoothPageController::handle(BtUiInput input) {
    switch (input) {
    case BtUiInput::Prev:
        if (focus_ > 0) { --focus_; dirty_ = true; }
        break;
    case BtUiInput::Next:
        if (focus_ + 1 < item_count_) { ++focus_; dirty_ = true; }
        break;
    case BtUiInput::Select:
        if (focus_ < item_count_) run(items_[focus_]);
        break;
    case BtUiInput::Back: back(); break;
    }
}

void BluetoothPageController::activate(size_t item) {
    if (item >= item_count_) return;
    focus_ = item;
    run(items_[item]);
}

void BluetoothPageController::run(BtUiItem item) {
    switch (item.action) {
    case BtUiAction::OpenDevice:
        if (static_cast<size_t>(item.index) < bonded_count_) {
            detail_ = bonded_[item.index];
            notice_ = BtUiNotice::None;
            go(BtUiScreen::Detail);
        }
        break;
    case BtUiAction::PickFound:
        if (static_cast<size_t>(item.index) < found_count_) {
            pending_device_ = found_[item.index];
            pending_ = true;
            notice_ = BtUiNotice::None;
            error_ = 0;
            devices_.connect(pending_device_);
            have_counter_ = false;
            update();
            dirty_ = true;
        }
        break;
    case BtUiAction::StartScan:
        if (screen_ == BtUiScreen::Scan) {
            notice_ = BtUiNotice::None;
            devices_.start_scan();
            have_counter_ = false;
            update();
            dirty_ = true;
        } else {
            go(BtUiScreen::Scan);
        }
        break;
    case BtUiAction::StopScan:
        devices_.stop_scan();
        have_counter_ = false;
        update();
        dirty_ = true;
        break;
    case BtUiAction::Close:
        if (devices_.scanning()) devices_.stop_scan();
        closed_ = true;
        dirty_ = true;
        break;
    case BtUiAction::Back: back(); break;
    case BtUiAction::Connect:
        devices_.connect(detail_);
        have_counter_ = false;
        update();
        dirty_ = true;
        break;
    case BtUiAction::Disconnect:
        devices_.disconnect(detail_);
        have_counter_ = false;
        update();
        dirty_ = true;
        break;
    case BtUiAction::Forget: go(BtUiScreen::ConfirmForget); break;
    case BtUiAction::ConfirmForget:
        devices_.forget(detail_);
        notice_ = BtUiNotice::Forgotten;
        go(BtUiScreen::List);
        break;
    case BtUiAction::Cancel: go(BtUiScreen::Detail); break;
    }
}

} // namespace pico_toolset
