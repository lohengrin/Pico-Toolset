// Pimoroni Presto full-integration example (doubles as an integration
// test): ST7701 LCD + FT6236 touch + uSD + PSRAM + 7 WS2812 LEDs + piezo +
// Wi-Fi + BLE + QwSTPad + Multi-Sensor Stick (BME280, LTR-559, LSM6DS3),
// all together. See ../../docs/boards/pimoroni-presto.md.
//
// Screen: colour bars under a row of status squares (green = ok, red =
// failed, grey = not present/not configured): PSRAM, SD, touch bus, LCD,
// Wi-Fi, BLE, QwSTPad, sensor stick. Touch draws a crosshair, plays a tone
// whose pitch follows X and brightens the LEDs with Y; the user button turns
// the LEDs white while held and toggles automatic backlight when pressed
// (the LTR-559 light sensor drives it; manual = full brightness, QwSTPad +/-
// adjusts it). A QwSTPad moves a square with its D-pad (A beeps). Gauges
// along the bottom show temperature, humidity and light. The QW/ST bus is
// scanned once at start-up, and pairing / Wi-Fi events go to USB serial.
// Bluetooth LE keyboards and gamepads (e.g. an Xbox controller) pair and bond
// by touching the status band at the top of the screen, which opens a device manager
// (list of bonded devices with a green dot when connected; X forgets one; PAIR NEW
// starts pairing mode; CLOSE or the band again leaves). Pairing also starts
// automatically on the first boot with nothing bonded. Put the device in pairing
// mode; a keyboard shows a passkey to type; bonded devices reconnect after a
// reboot and steer the square with the arrow keys / left stick / D-pad.
//
// Wi-Fi is skipped unless a config.local.h defining WIFI_SSID/WIFI_PASSWORD
// is on the include path (see CMakeLists.txt; never commit one).

// Pins/wiring come from the Presto presets (validated on hardware with this
// example); the serial log reports each subsystem.
#if __has_include("config.local.h")
#include "config.local.h"
#endif

#ifndef PRESTO_NO_RADIO
#include "pico_toolset/bluetooth.h"
#include "pico_toolset/bluetooth_hid.h"
#endif
#include "pico_toolset/bme280.h"
#include "pico_toolset/bme280_configs.h"
#include "pico_toolset/ft6236.h"
#include "pico_toolset/ft6236_configs.h"
#include "pico_toolset/lsm6ds3.h"
#include "pico_toolset/lsm6ds3_configs.h"
#include "pico_toolset/ltr559.h"
#include "pico_toolset/ltr559_configs.h"
#include "pico_toolset/piezo.h"
#include "pico_toolset/piezo_configs.h"
#include "pico_toolset/psram.h"
#include "pico_toolset/psram_configs.h"
#include "pico_toolset/qwstpad.h"
#include "pico_toolset/qwstpad_configs.h"
#include "pico_toolset/sdcard.h"
#include "pico_toolset/sdcard_configs.h"
#include "pico_toolset/st7701.h"
#include "pico_toolset/st7701_configs.h"
#include "pico_toolset/simple_font.h"
#include "pico_toolset/st7701_driver.h"
#include "pico_toolset/ws2812.h"
#include "pico_toolset/widget.h"
#include "pico_toolset/ws2812_configs.h"
#ifndef PRESTO_NO_RADIO
#include "pico_toolset/wifi.h"
#endif

#ifdef WANT_HCI_DUMP
#include "hci_dump.h"
#endif
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#ifndef PRESTO_NO_RADIO
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "pico/cyw43_arch.h"
#endif
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace pico_toolset;

namespace {

#ifdef PRESTO_FULL_RES
constexpr int kSize = 480;
const St7701Config& lcd_preset() { return configs::st7701::kPimoroniPrestoFullRes; }
const Ft6236Config& touch_preset() { return configs::ft6236::kPimoroniPrestoFullRes; }
#else
constexpr int kSize = 240;
const St7701Config& lcd_preset() { return configs::st7701::kPimoroniPrestoHalfRes; }
const Ft6236Config& touch_preset() { return configs::ft6236::kPimoroniPrestoHalfRes; }
#endif

constexpr uint kUserButtonPin = 46;
constexpr uint kQwstSdaPin = 40;
constexpr uint kQwstSclPin = 41;

// Scanout framebuffer: SRAM only (PSRAM cannot feed the DMA at pixel rate).
// PRESTO_PALETTE: one byte per pixel (RGB332 colour table) instead of RGB565,
// which halves it (480x480: 225 KB) and leaves room for the radio stack.
#ifdef PRESTO_PALETTE
uint8_t g_fb[kSize * kSize] __attribute__((aligned(4)));
#else
uint16_t g_fb[kSize * kSize] __attribute__((aligned(4)));
#endif
St7701 g_lcd;

// Runs the LCD's PIO/DMA interrupts on core1, so core0 flash/PSRAM stalls
// can't glitch scanout. (No flash lockout is registered on core1 -- see
// AGENTS.md; everything flash-touching below runs before it is launched.)
// Core 1 must run from RAM once the radio is up: bonding writes flash from
// core 0, and PICO_FLASH_ASSUME_CORE1_SAFE (CMakeLists.txt) tells the SDK it
// does not need to stop core 1 for that. The scanout interrupt handlers are
// already RAM-resident.
void __not_in_flash_func(core1_idle)() {
    while (true) __wfe();
}

void core1_main() {
    auto cfg = lcd_preset();
    cfg.framebuffer = g_fb;
#ifdef PRESTO_PALETTE
    cfg.format = St7701Format::Palette8;
#endif
    multicore_fifo_push_blocking(g_lcd.init(cfg) ? 1 : 0);
    core1_idle();
}

// Ambient light (lux) -> backlight 10..255 on a log scale (about 1000 lux and
// up = full, darkness = dim but readable).
uint8_t lux_to_backlight(float lux) {
    const float f = std::clamp(std::log10(1.0f + std::max(lux, 0.0f)) / 3.0f, 0.0f, 1.0f);
    return static_cast<uint8_t>(10.0f + f * 245.0f);
}

// Draws `text` as large as fits the screen width (at most max_scale), left-aligned
// at x. Returns the height drawn.
int draw_text_fit(DisplayDriver& d, int x, int y, const char* text, Color fg, int max_scale,
                  Color bg = kColorBlack) {
    const int len = static_cast<int>(std::strlen(text));
    int scale = len > 0 ? (kSize - x - 4) / (6 * len) : max_scale; // 5px glyph + 1px gap
    scale = std::clamp(scale, 1, max_scale);
    TextWidget w(x, y, text, fg, bg, kGlyphFont5x8.glyphs, 8, static_cast<uint8_t>(scale));
    w.draw(d);
    return 8 * scale + 2;
}

#ifndef PRESTO_NO_RADIO
// US-layout HID usage -> ASCII for the few keys the demo echoes (0 = none).
char usage_to_ascii(uint8_t usage, bool shift) {
    if (usage >= 0x04 && usage <= 0x1D) return static_cast<char>((shift ? 'A' : 'a') + (usage - 0x04));
    if (usage >= 0x1E && usage <= 0x26) return static_cast<char>('1' + (usage - 0x1E));
    switch (usage) {
    case 0x27: return '0';
    case 0x28: return '\n';
    case 0x2A: return '\b';
    case 0x2C: return ' ';
    case 0x2D: return shift ? '_' : '-';
    case 0x2E: return shift ? '+' : '=';
    case 0x36: return shift ? '<' : ',';
    case 0x37: return shift ? '>' : '.';
    case 0x38: return shift ? '?' : '/';
    default: return 0;
    }
}
#endif

constexpr int kStatusH = kSize / 12;

#ifndef PRESTO_NO_RADIO
// ---- Bluetooth device manager (touch): list bonded devices, forget one, start pairing ----
struct DeviceMenu {
    bool open = false;
    BtHidDevice list[BluetoothHid::kMaxBonded];
    size_t count = 0;
};
enum class MenuAction { None, Forget, Pair, Close };

constexpr int kMenuScale = kSize / 120 > 0 ? kSize / 120 : 1;  // text scale (2 at 240, 4 at 480)
constexpr int kMenuRowH = 8 * kMenuScale + 10;                  // also the touch target height
constexpr int kMenuY0 = kStatusH + 6;
constexpr int kMenuButtonsY = kMenuY0 + kMenuRowH * (static_cast<int>(BluetoothHid::kMaxBonded) + 1);

MenuAction menu_hit(const DeviceMenu& m, int x, int y, size_t& index) {
    for (size_t i = 0; i < m.count; ++i) {
        const int ry = kMenuY0 + kMenuRowH * static_cast<int>(i + 1);
        if (y >= ry && y < ry + kMenuRowH && x >= kSize - kMenuRowH) {
            index = i;
            return MenuAction::Forget;
        }
    }
    if (y >= kMenuButtonsY && y < kMenuButtonsY + kMenuRowH) return x < kSize / 2 ? MenuAction::Pair : MenuAction::Close;
    return MenuAction::None;
}

void draw_menu(DisplayDriver& d, const DeviceMenu& m) {
    d.fill_rect(0, kStatusH + 2, kSize - 1, kSize - 1, kColorBlack);
    char line[40];
    std::snprintf(line, sizeof(line), "BT DEVICES (%u)", static_cast<unsigned>(m.count));
    draw_text_fit(d, 4, kMenuY0, line, kColorWhite, kMenuScale);
    if (m.count == 0) draw_text_fit(d, 4, kMenuY0 + kMenuRowH, "NONE BONDED", Color(0x7BEF), kMenuScale);
    for (size_t i = 0; i < m.count; ++i) {
        const BtHidDevice& dev = m.list[i];
        const int ry = kMenuY0 + kMenuRowH * static_cast<int>(i + 1);
        d.fill_rect(4, ry + 4, 4 + 8, ry + kMenuRowH - 5, dev.connected ? kColorGreen : Color(0x7BEF));
        const char* kind = (dev.kinds & bt_hid_kind::Gamepad) ? "PAD" : (dev.kinds & bt_hid_kind::Keyboard) ? "KBD"
                         : (dev.kinds & bt_hid_kind::Mouse) ? "MOUSE" : "DEV";
        if (dev.name[0]) std::snprintf(line, sizeof(line), "%s %.10s", kind, dev.name);
        else std::snprintf(line, sizeof(line), "%s %02X%02X%02X", kind, dev.addr[3], dev.addr[4], dev.addr[5]);
        draw_text_fit(d, 18, ry + 5, line, kColorWhite, kMenuScale);
        d.fill_rect(kSize - kMenuRowH, ry + 1, kSize - 3, ry + kMenuRowH - 2, kColorRed);
        draw_text_fit(d, kSize - kMenuRowH + (kMenuRowH - 6 * kMenuScale) / 2 - 1, ry + 5, "X", kColorWhite, kMenuScale, kColorRed);
    }
    const int by = kMenuButtonsY;
    d.fill_rect(2, by, kSize / 2 - 3, by + kMenuRowH - 2, Color(0x03E0));
    draw_text_fit(d, 8, by + 5, "PAIR NEW", kColorWhite, kMenuScale, Color(0x03E0));
    d.fill_rect(kSize / 2 + 2, by, kSize - 3, by + kMenuRowH - 2, Color(0x4208));
    draw_text_fit(d, kSize / 2 + 8, by + 5, "CLOSE", kColorWhite, kMenuScale, Color(0x4208));
}
#endif

constexpr Color kBars[8] = {
    Color(0xF800), Color(0xFFE0), Color(0x07E0), Color(0x07FF),
    Color(0x001F), Color(0xF81F), Color(0xFFFF), Color(0x0000),
};

enum class Status { Ok, Failed, Absent, Busy };

void draw_status(DisplayDriver& d, int slot, Status st) {
    const int s = kStatusH;
    const Color c = st == Status::Ok ? kColorGreen : st == Status::Failed ? kColorRed
                  : st == Status::Busy ? Color(0xFFE0) : Color(0x7BEF);
    d.fill_rect(slot * (s + 4) + 4, 2, slot * (s + 4) + 4 + s - 1, s - 1, c);
}

Status status(bool ok) { return ok ? Status::Ok : Status::Failed; }

// Horizontal gauge along the bottom of the bars, row 0 = lowest.
void draw_gauge(DisplayDriver& d, int row, float frac, Color c) {
    const int h = kSize / 30;
    const int y1 = kSize - 1 - row * (h + 2);
    frac = std::clamp(frac, 0.0f, 1.0f);
    d.fill_rect(0, y1 - h + 1, kSize - 1, y1, kColorBlack);
    d.fill_rect(0, y1 - h + 1, static_cast<int>(frac * (kSize - 1)), y1, c);
}

#ifndef PRESTO_NO_RADIO
// Bluetooth HID events arrive on the radio's context; the main loop reads these.
volatile long g_passkey = -1;       // >= 0 while a keyboard is waiting for its passkey
volatile bool g_hid_busy = false;   // pairing/connecting in progress
volatile bool g_hid_dirty = false;  // bonded/connected devices changed: refresh the device list
char g_hid_message[40] = "";

// Raw reports of Classic pads without a usable SDP record: print when they change, so the
// layout of an unknown pad can be worked out from the bytes (press one control at a time).
void on_raw_report(const uint8_t* data, uint16_t len, const BtHidDevice&, void*) {
    static uint8_t last[32];
    static uint16_t last_len = 0;
    const uint16_t n = len < sizeof(last) ? len : sizeof(last);
    if (n == last_len && std::memcmp(data, last, n) == 0) return;
    std::memcpy(last, data, n);
    last_len = n;
    std::printf("[raw %u]", static_cast<unsigned>(len));
    for (uint16_t i = 0; i < n; ++i) std::printf(" %02X", data[i]);
    std::printf("\n");
}

void on_hid_event(const BtHidEvent& e, void*) {
    if (e.type == BtHidEventType::Paired || e.type == BtHidEventType::Connected ||
        e.type == BtHidEventType::Disconnected || e.type == BtHidEventType::Forgotten)
        g_hid_dirty = true;
#ifdef WANT_HCI_DUMP
    // The packet log is switched on only while a link is being set up or is up:
    // scanning produces hundreds of advertising reports per second.
    if (e.type == BtHidEventType::Connecting) hci_dump_enable_packet_log(true);
    else if (e.type == BtHidEventType::Disconnected || e.type == BtHidEventType::PairingFailed)
        hci_dump_enable_packet_log(false);
#endif
    switch (e.type) {
    case BtHidEventType::PairingStarted:
        g_hid_busy = true;
        std::snprintf(g_hid_message, sizeof(g_hid_message), "PAIRING: set device to pair");
        break;
    case BtHidEventType::Connecting:
        g_hid_busy = true;
        std::snprintf(g_hid_message, sizeof(g_hid_message), "CONNECTING %.20s", e.device.name);
        break;
    case BtHidEventType::PasskeyDisplay:
        g_passkey = static_cast<long>(e.passkey);
        break;
    case BtHidEventType::Connected:
        g_passkey = -1;
        g_hid_busy = false;
        std::snprintf(g_hid_message, sizeof(g_hid_message), "CONNECTED %.20s", e.device.name);
        break;
    case BtHidEventType::PairingFailed:
        g_passkey = -1;
        g_hid_busy = false;
        std::snprintf(g_hid_message, sizeof(g_hid_message), "FAILED 0x%02X/%02X", e.status, e.reason);
        break;
    case BtHidEventType::PairingStopped:
    case BtHidEventType::Disconnected:
        g_passkey = -1;
        g_hid_busy = false;
        break;
    default:
        break;
    }
    static const char* const kNames[] = {"pairing-started", "pairing-stopped", "found", "connecting",
                                         "passkey", "paired", "connected", "disconnected", "FAILED",
                                         "forgotten"};
    const auto& d = e.device;
    std::printf("[%lu ms] [hid] %s %02X:%02X:%02X:%02X:%02X:%02X '%s'", static_cast<unsigned long>(to_ms_since_boot(get_absolute_time())),
                kNames[static_cast<int>(e.type)], d.addr[0],
                d.addr[1], d.addr[2], d.addr[3], d.addr[4], d.addr[5], d.name);
    if (e.type == BtHidEventType::PairingFailed)
        std::printf(" status 0x%02X reason 0x%02X", e.status, e.reason);
    if (e.type == BtHidEventType::DeviceFound) {
        std::printf(" rssi %d", d.rssi);
        if (e.class_of_device)
            std::printf(" CLASSIC cod 0x%06lX vid 0x%04X pid 0x%04X", static_cast<unsigned long>(e.class_of_device),
                        e.vendor_id, e.product_id);
    }
    if (e.type == BtHidEventType::PasskeyDisplay)
        std::printf("\n*** PASSKEY: %06lu -- type it on the keyboard, then Enter ***", static_cast<unsigned long>(e.passkey));
    std::printf("\n");
}

#endif

void draw_bars(DisplayDriver& d) {
    const int w = kSize / 8;
    for (int i = 0; i < 8; ++i)
        d.fill_rect(i * w, kStatusH + 2, (i + 1) * w - 1, kSize - 1, kBars[i]);
}

void scan_qwst() {
    std::printf("QW/ST I2C0 scan:");
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
        uint8_t dummy;
        if (i2c_read_timeout_us(i2c0, addr, &dummy, 1, false, 5000) >= 0) {
            std::printf(" 0x%02X", addr);
            ++found;
        }
    }
    std::printf(found ? "\n" : " (none)\n");
}

} // namespace

int main() {
    stdio_init_all();
    sleep_ms(2000);
    std::printf("Pimoroni Presto example: LCD + touch + PSRAM + SD + LEDs + piezo + QW/ST (%dx%d)\n",
                kSize, kSize);

    // PSRAM and SD first: PSRAM bring-up reconfigures QMI and must finish
    // before core1 starts (see AGENTS.md "Flash/PSRAM (re)initialization").
    auto psram = psram_init(configs::psram::kPimoroniPresto);
    std::printf("PSRAM present=%d test_ok=%d size=%zu bytes clk=%lu Hz\n", psram.present,
                psram.test_ok, psram.size_bytes, static_cast<unsigned long>(psram.clock_hz));

    SdCard sd;
    const bool sd_ok = sd.init(configs::sdcard::kPimoroniPresto);
    if (sd_ok) {
        std::printf("SD mounted (native SPI: %s)\n", sd.used_native_spi() ? "yes" : "no");
        for (const auto& name : sd.list_files({"txt", "jpg", "png"})) std::printf("  %s\n", name.c_str());
    } else {
        std::printf("SD mount failed (FRESULT=%d)\n", sd.last_mount_result());
    }

    multicore_launch_core1(core1_main);
    const bool lcd_ok = multicore_fifo_pop_blocking() == 1;
    std::printf("ST7701 init %s\n", lcd_ok ? "ok" : "FAILED");

    Ft6236 touch;
    touch.init(touch_preset());

    Ws2812 leds;
    const bool leds_ok = leds.init(configs::ws2812::kPimoroniPresto);
    std::printf("WS2812 init %s\n", leds_ok ? "ok" : "FAILED");

    Piezo piezo;
    piezo.init(configs::piezo::kPimoroniPresto);
    constexpr PiezoNote kBoot[] = {{784, 80}, {1047, 120}};
    piezo.play(kBoot, 2);

    gpio_init(kUserButtonPin);
    gpio_set_dir(kUserButtonPin, GPIO_IN);
    gpio_pull_up(kUserButtonPin);

    St7701Driver display(g_lcd);
    display.set_backlight(255);

    Status radio_wifi = Status::Absent, radio_ble = Status::Absent;
    Status pad_status = Status::Absent, stick_status = Status::Absent;
    Status hid_status = Status::Absent;

    bool menu_open = false; // Bluetooth device manager covers the bars
    auto draw_ui = [&](const TouchSample& t) {
        if (!menu_open) draw_bars(display);
        draw_status(display, 0, status(psram.present && psram.test_ok));
        draw_status(display, 1, status(sd_ok));
        draw_status(display, 2, status(touch.bus_ok()));
        draw_status(display, 3, status(lcd_ok));
        draw_status(display, 4, radio_wifi);
        draw_status(display, 5, radio_ble);
        draw_status(display, 6, pad_status);
        draw_status(display, 7, stick_status);
        draw_status(display, 8, hid_status);
        if (t.pressed && !menu_open) {
            display.draw_line(t.raw_x, 0, t.raw_x, kSize - 1, kColorWhite);
            display.draw_line(0, t.raw_y, kSize - 1, t.raw_y, kColorWhite);
        }
    };
    if (lcd_ok) draw_ui({});

    // -- QW/ST bus (I2C0): QwSTPad + Multi-Sensor Stick --
    i2c_init(i2c0, 400'000);
    gpio_set_function(kQwstSdaPin, GPIO_FUNC_I2C);
    gpio_set_function(kQwstSclPin, GPIO_FUNC_I2C);
    gpio_pull_up(kQwstSdaPin);
    gpio_pull_up(kQwstSclPin);
    scan_qwst();

    QwstPad pad;
    const bool pad_ok = pad.init(configs::qwstpad::kPimoroniPresto);
    pad_status = pad_ok ? Status::Ok : Status::Absent;
    std::printf("QwSTPad %s\n", pad_ok ? "found" : "not found");

    Bme280 bme;
    Ltr559 ltr;
    Lsm6ds3 imu;
    const bool bme_ok = bme.init(configs::bme280::kPimoroniPrestoMultiSensorStick);
    const bool ltr_ok = ltr.init(configs::ltr559::kPimoroniPrestoMultiSensorStick);
    const bool imu_ok = imu.init(configs::lsm6ds3::kPimoroniPrestoMultiSensorStick);
    std::printf("Multi-Sensor Stick: BME280 %d, LTR-559 %d, LSM6DS3 %d (who_am_i 0x%02X)\n", bme_ok,
                ltr_ok, imu_ok, imu.who_am_i());
    const int sensors = bme_ok + ltr_ok + imu_ok;
    stick_status = sensors == 3 ? Status::Ok : sensors == 0 ? Status::Absent : Status::Failed;

    // -- Radio: Wi-Fi (if credentials are configured) + BLE. The WS2812 PIO
    // setup above already ran: the CYW43 PIO SPI then picks a PIO that still
    // fits (see docs/boards/pimoroni-presto.md, "Resource map"). --
    bool radio_ok = false;
#ifdef PRESTO_NO_RADIO
    std::printf("Wi-Fi/BLE not built (full-res leaves no SRAM for the radio stack)\n");
#elif defined(WIFI_SSID)
    {
        WifiConfig wc;
        wc.ssid = WIFI_SSID;
        wc.password = WIFI_PASSWORD;
        wc.power_save = false;
        if (lcd_ok) draw_ui({});
        radio_ok = wifi_connect(wc); // initialises cyw43_arch
        radio_wifi = radio_ok ? Status::Ok : Status::Failed;
        // wifi_connect() failing after a successful cyw43 init still leaves the radio usable.
        radio_ok = radio_ok || cyw43_is_initialized(&cyw43_state);
        if (radio_wifi == Status::Ok) {
            cyw43_arch_lwip_begin();
            std::printf("Wi-Fi up, IP %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
            cyw43_arch_lwip_end();
        }
    }
#else
    radio_ok = cyw43_arch_init() == 0;
    std::printf("Wi-Fi skipped (no config.local.h)\n");
#endif
#ifndef PRESTO_NO_RADIO
    Bluetooth bt;
    if (radio_ok) {
        const bool bt_ok = bt.init(BluetoothConfig{.name = "presto"});
        radio_ble = status(bt_ok);
        std::printf("BLE init %s\n", bt_ok ? "ok" : "FAILED");
        if (bt_ok) {
            bt.start_advertising();
            // (BLE scanning would collide with HID pairing's scan: HID owns it.)
        }
    }
    BluetoothHid hid;
    bool hid_ok = false;
    if (radio_ble == Status::Ok) {
        BluetoothHidConfig hc;
        hc.callback = on_hid_event;
        hc.raw_report_callback = on_raw_report;
        hid_ok = hid.init(hc);
#ifdef WANT_HCI_DUMP
        hci_dump_enable_packet_log(false); // see on_hid_event()
#endif
        hid_status = hid_ok ? Status::Absent : Status::Failed;
        BtHidDevice bonded[BluetoothHid::kMaxBonded];
        const size_t nb = hid_ok ? hid.bonded_devices(bonded, BluetoothHid::kMaxBonded) : 0;
        std::printf("Bluetooth HID %s, %u bonded device(s)\n", hid_ok ? "ok" : "FAILED",
                    static_cast<unsigned>(nb));
        if (hid_ok && nb == 0) hid.start_pairing();
    }
#endif

    uint32_t frame = 0;
    absolute_time_t next_sensors = get_absolute_time();
    int pad_x = kSize / 2, pad_y = kSize / 2;
    uint16_t pad_prev = 0;
    Bme280Reading env;
    Ltr559Reading light;
    Lsm6ds3Sample motion{};
    bool auto_backlight = ltr_ok;
    float backlight = 255.0f;
    int ltr_updates = 0;
    bool button_prev = false;
    bool pad_a_prev = false;
    uint32_t pad_raw_prev = 0;
#ifndef PRESTO_NO_RADIO
    char typed[28] = "";
    size_t typed_len = 0;
    DeviceMenu menu;
#endif
    TouchSample prev{};
    while (true) {
        auto t = touch.read();

        if (time_reached(next_sensors)) {
            next_sensors = make_timeout_time_ms(250);
            if (bme_ok) env = bme.read();
            if (ltr_ok && ltr.poll(light)) ++ltr_updates;
            if (auto_backlight && ltr_updates >= 2) { // skip the first readings: still warming up
                // Low-pass so passing shadows do not make the screen flicker.
                backlight += (lux_to_backlight(light.lux) - backlight) * 0.2f;
                display.set_backlight(static_cast<uint8_t>(backlight));
            }
            if (imu_ok) imu.read(motion);
        }

#ifndef PRESTO_NO_RADIO
        if (hid_ok) {
            // Touching the status band starts pairing mode.
            // Tap the status band: open/close the device manager. Inside it: X forgets a device,
            // PAIR NEW starts pairing mode, CLOSE (or the band again) leaves.
            const bool tap = t.pressed && !prev.pressed;
            auto refresh_menu = [&] { menu.count = hid.bonded_devices(menu.list, BluetoothHid::kMaxBonded); };
            if (g_hid_dirty) {
                g_hid_dirty = false;
                if (menu.open) refresh_menu();
            }
            if (tap) {
                if (t.raw_y < kStatusH + 2) {
                    if (menu.open) menu.open = false;
                    else if (!hid.pairing() && !g_hid_busy) { refresh_menu(); menu.open = true; }
                } else if (menu.open) {
                    size_t idx = 0;
                    switch (menu_hit(menu, t.raw_x, t.raw_y, idx)) {
                    case MenuAction::Forget:
                        hid.forget(menu.list[idx].addr);
                        refresh_menu();
                        break;
                    case MenuAction::Pair:
                        menu.open = false;
                        hid.start_pairing();
                        break;
                    case MenuAction::Close:
                        menu.open = false;
                        break;
                    default:
                        break;
                    }
                }
            }
            menu_open = menu.open;
            hid_status = (hid.connected_keyboard_count() + hid.connected_gamepad_count() +
                          hid.connected_mouse_count()) > 0 ? Status::Ok
                         : (hid.pairing() || g_hid_busy) ? Status::Busy
                                                         : Status::Absent;
            // Echo typed keys on the console and the screen.
            for (int u = 0x04; u < 0x68; ++u) {
                if (!hid.consume_key_press(static_cast<uint8_t>(u))) continue;
                const bool shift = hid.is_modifier_down(0x22);
                const char c = usage_to_ascii(static_cast<uint8_t>(u), shift);
                const char lit[2] = {c, '\0'};
                std::printf("[key] usage 0x%02X%s%s\n", u, c ? " = " : "",
                            c == '\n' ? "<enter>" : c == '\b' ? "<bksp>" : (c ? lit : ""));
                if (c == '\b') { if (typed_len) typed[--typed_len] = '\0'; }
                else if (c == '\n') { typed_len = 0; typed[0] = '\0'; }
                else if (c && typed_len < sizeof(typed) - 1) { typed[typed_len++] = c; typed[typed_len] = '\0'; }
            }
            // Arrow keys and the first gamepad's stick/D-pad move the square.
            const int step = kSize / 80 + 1;
            if (hid.is_key_down(0x50)) pad_x -= step;
            if (hid.is_key_down(0x4F)) pad_x += step;
            if (hid.is_key_down(0x52)) pad_y -= step;
            if (hid.is_key_down(0x51)) pad_y += step;
            const GamepadState g = hid.gamepad_state(0);
            if (g.present) {
                const uint32_t raw = hid.gamepad_raw_buttons(0);
                if (raw != pad_raw_prev) { // lets you verify the button mapping of a new controller
                    std::printf("pad0 raw buttons 0x%08lX -> 0x%04X L(%u,%u) R(%u,%u) LT %u RT %u\n",
                                static_cast<unsigned long>(raw), g.buttons, g.lx, g.ly, g.rx, g.ry, g.lt, g.rt);
                    pad_raw_prev = raw;
                }
                if (g.down(kBtLeft) || g.lx < 64) pad_x -= step;
                if (g.down(kBtRight) || g.lx > 192) pad_x += step;
                if (g.down(kBtUp) || g.ly < 64) pad_y -= step;
                if (g.down(kBtDown) || g.ly > 192) pad_y += step;
                if (g.down(kBtA) && !pad_a_prev) piezo.set_tone(660);
                if (!g.down(kBtA) && pad_a_prev) piezo.stop();
                pad_a_prev = g.down(kBtA);
            }
            pad_x = std::clamp(pad_x, 0, kSize - 1);
            pad_y = std::clamp(pad_y, 0, kSize - 1);
        }
#endif

        if (pad_ok) {
            using namespace qwstpad_button;
            const uint16_t b = pad.read_buttons();
            const int step = kSize / 80 + 1;
            if (b & Left) pad_x -= step;
            if (b & Right) pad_x += step;
            if (b & Up) pad_y -= step;
            if (b & Down) pad_y += step;
            pad_x = std::clamp(pad_x, 0, kSize - 1);
            pad_y = std::clamp(pad_y, 0, kSize - 1);
            if ((b & A) && !(pad_prev & A)) piezo.set_tone(880);
            if (!(b & A) && (pad_prev & A)) piezo.stop();
            pad.set_leds((b & A ? 1 : 0) | (b & B ? 2 : 0) | (b & X ? 4 : 0) | (b & Y ? 8 : 0));
            const uint16_t pressed_now = b & ~pad_prev;
            if (pressed_now & (Plus | Minus)) {
                auto_backlight = false;
                backlight = std::clamp(backlight + ((pressed_now & Plus) ? 32.0f : -32.0f), 10.0f, 255.0f);
                display.set_backlight(static_cast<uint8_t>(backlight));
                std::printf("backlight manual %d\n", static_cast<int>(backlight));
            }
            if (b != pad_prev) std::printf("pad 0x%03X\n", b);
            pad_prev = b;
        }


        if (lcd_ok) {
            display.flush(); // wait for vblank so the redraw trails the beam
            draw_status(display, 2, status(touch.bus_ok()));
            draw_ui(t);
            if (!menu_open && bme_ok) {
                draw_gauge(display, 2, (env.temperature_c - 0.0f) / 40.0f, kColorRed);
                draw_gauge(display, 1, env.humidity_pct / 100.0f, kColorBlue);
            }
            if (!menu_open && ltr_ok) draw_gauge(display, 0, light.lux / 1000.0f, kColorWhite);
#ifndef PRESTO_NO_RADIO
            if (menu_open) draw_menu(display, menu);
            if (g_hid_busy || g_passkey >= 0) {
                const int scale = kSize / 120 + 1;
                int y = kStatusH + 6;
                y += draw_text_fit(display, 4, y, g_hid_message, kColorWhite, scale);
                if (g_passkey >= 0) {
                    char digits[12];
                    y += draw_text_fit(display, 4, y, "PASSKEY:", Color(0xFFE0), scale);
                    std::snprintf(digits, sizeof(digits), "%06ld", static_cast<long>(g_passkey));
                    draw_text_fit(display, 4, y, digits, Color(0xFFE0), 8);
                }
            }
            if (typed_len && !menu_open) {
                const int gauge_h = (kSize / 30 + 2) * 3;
                draw_text_fit(display, 4, kSize - gauge_h - 8 * (kSize / 120 + 1) - 4, typed, kColorWhite,
                              kSize / 120 + 1);
            }
#endif
            if (pad_ok && !menu_open) {
                const int r = kSize / 40;
                display.fill_rect(pad_x - r, pad_y - r, pad_x + r, pad_y + r, kColorBlack);
                display.fill_rect(pad_x - r + 2, pad_y - r + 2, pad_x + r - 2, pad_y + r - 2, kColorWhite);
            }
        }

        if (t.pressed) {
            piezo.set_tone(200 + (t.raw_x * 1800u) / kSize);
        } else if (prev.pressed) {
            piezo.stop();
        }
        if (t.pressed != prev.pressed || (t.pressed && frame % 30 == 0))
            std::printf("touch %d (%u,%u)\n", t.pressed, t.raw_x, t.raw_y);
        prev = t;

        if (leds_ok) {
            const bool button = !gpio_get(kUserButtonPin);
            if (button && !button_prev && ltr_ok) {
                auto_backlight = !auto_backlight;
                if (!auto_backlight) {
                    backlight = 255.0f;
                    display.set_backlight(255);
                }
                std::printf("backlight %s\n", auto_backlight ? "auto" : "manual");
            }
            button_prev = button;
            const float v = t.pressed ? 0.1f + 0.5f * (1.0f - static_cast<float>(t.raw_y) / kSize) : 0.15f;
            for (size_t i = 0; i < leds.count(); ++i) {
                if (button) leds.set_rgb(i, 80, 80, 80);
                else leds.set_hsv(i, frame * 3 + i * (360.0f / leds.count()), 1.0f, v);
            }
            leds.show();
        }

        ++frame;
    }
}
