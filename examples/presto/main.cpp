// Pimoroni Presto full-integration example (doubles as an integration
// test): ST7701 LCD + FT6236 touch + uSD + PSRAM + 7 WS2812 LEDs + piezo +
// Wi-Fi + BLE + QwSTPad + Multi-Sensor Stick (BME280, LTR-559, LSM6DS3),
// all together. See ../../docs/boards/pimoroni-presto.md.
//
// Screen: colour bars under a row of status squares (green = ok, red =
// failed, grey = not present/not configured): PSRAM, SD, touch bus, LCD,
// Wi-Fi, BLE, QwSTPad, sensor stick. Touch draws a crosshair, plays a tone
// whose pitch follows X and brightens the LEDs with Y; the user button turns
// the LEDs white. A QwSTPad moves a square with its D-pad (A beeps). Gauges
// along the bottom show temperature, humidity and light. The QW/ST bus is
// scanned every few seconds, and sensor / BLE / Wi-Fi data goes to USB serial.
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
#include "pico_toolset/st7701_driver.h"
#include "pico_toolset/ws2812.h"
#include "pico_toolset/ws2812_configs.h"
#ifndef PRESTO_NO_RADIO
#include "pico_toolset/wifi.h"
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
uint16_t g_fb[kSize * kSize] __attribute__((aligned(4)));
St7701 g_lcd;

// Runs the LCD's PIO/DMA interrupts on core1, so core0 flash/PSRAM stalls
// can't glitch scanout. (No flash lockout is registered on core1 -- see
// AGENTS.md; everything flash-touching below runs before it is launched.)
void core1_main() {
    auto cfg = lcd_preset();
    cfg.framebuffer = g_fb;
    multicore_fifo_push_blocking(g_lcd.init(cfg) ? 1 : 0);
    while (true) tight_loop_contents();
}

constexpr Color kBars[8] = {
    Color(0xF800), Color(0xFFE0), Color(0x07E0), Color(0x07FF),
    Color(0x001F), Color(0xF81F), Color(0xFFFF), Color(0x0000),
};
constexpr int kStatusH = kSize / 10;

enum class Status { Ok, Failed, Absent };

void draw_status(DisplayDriver& d, int slot, Status st) {
    const int s = kStatusH;
    const Color c = st == Status::Ok ? kColorGreen : st == Status::Failed ? kColorRed : Color(0x7BEF);
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
// BLE scan results arrive on the radio's context: keep it to bookkeeping and
// let the main loop print.
struct BleSeen {
    uint8_t addr[6];
    int8_t rssi;
    char name[32];
};
BleSeen g_ble_seen[16];
volatile int g_ble_count = 0;
int g_ble_printed = 0; // main loop only

void on_ble_adv(const BleAdvertisement& a, void*) {
    const int n = g_ble_count;
    for (int i = 0; i < n; ++i)
        if (std::memcmp(g_ble_seen[i].addr, a.addr, 6) == 0) return;
    if (n >= 16) return;
    std::memcpy(g_ble_seen[n].addr, a.addr, 6);
    g_ble_seen[n].rssi = a.rssi;
    std::memcpy(g_ble_seen[n].name, a.name, sizeof(a.name));
    g_ble_count = n + 1;
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

    auto draw_ui = [&](const TouchSample& t) {
        draw_bars(display);
        draw_status(display, 0, status(psram.present && psram.test_ok));
        draw_status(display, 1, status(sd_ok));
        draw_status(display, 2, status(touch.bus_ok()));
        draw_status(display, 3, status(lcd_ok));
        draw_status(display, 4, radio_wifi);
        draw_status(display, 5, radio_ble);
        draw_status(display, 6, pad_status);
        draw_status(display, 7, stick_status);
        if (t.pressed) {
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
            bt.start_scan(on_ble_adv);
        }
    }
#endif

    uint32_t frame = 0;
    absolute_time_t next_scan = make_timeout_time_ms(5000);
    absolute_time_t next_sensors = get_absolute_time();
    int pad_x = kSize / 2, pad_y = kSize / 2;
    uint16_t pad_prev = 0;
    Bme280Reading env;
    Ltr559Reading light;
    Lsm6ds3Sample motion{};
    int print_div = 0;
    TouchSample prev{};
    while (true) {
        auto t = touch.read();

        if (time_reached(next_sensors)) {
            next_sensors = make_timeout_time_ms(250);
            if (bme_ok) env = bme.read();
            if (ltr_ok) ltr.poll(light);
            if (imu_ok) imu.read(motion);
            if (++print_div >= 4) {
                print_div = 0;
                if (bme_ok)
                    std::printf("BME280 %.1f C  %.1f hPa  %.0f %%RH\n", env.temperature_c,
                                env.pressure_pa / 100.0f, env.humidity_pct);
                if (ltr_ok) std::printf("LTR559 prox %u  lux %.0f\n", light.proximity, light.lux);
                if (imu_ok)
                    std::printf("LSM6DS3 a %.2f %.2f %.2f g  steps %d\n", motion.ax, motion.ay,
                                motion.az, imu.step_count());
            }
        }

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
            if (b != pad_prev) std::printf("pad 0x%03X\n", b);
            pad_prev = b;
        }

#ifndef PRESTO_NO_RADIO
        if (g_ble_printed < g_ble_count) {
            const auto& s = g_ble_seen[g_ble_printed++];
            std::printf("BLE %02X:%02X:%02X:%02X:%02X:%02X rssi %d %s\n", s.addr[0], s.addr[1],
                        s.addr[2], s.addr[3], s.addr[4], s.addr[5], s.rssi, s.name);
        }
#endif

        if (lcd_ok) {
            display.flush(); // wait for vblank so the redraw trails the beam
            draw_status(display, 2, status(touch.bus_ok()));
            draw_ui(t);
            if (bme_ok) {
                draw_gauge(display, 2, (env.temperature_c - 0.0f) / 40.0f, kColorRed);
                draw_gauge(display, 1, env.humidity_pct / 100.0f, kColorBlue);
            }
            if (ltr_ok) draw_gauge(display, 0, light.lux / 1000.0f, kColorWhite);
            if (pad_ok) {
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
            const float v = t.pressed ? 0.1f + 0.5f * (1.0f - static_cast<float>(t.raw_y) / kSize) : 0.15f;
            for (size_t i = 0; i < leds.count(); ++i) {
                if (button) leds.set_rgb(i, 80, 80, 80);
                else leds.set_hsv(i, frame * 3 + i * (360.0f / leds.count()), 1.0f, v);
            }
            leds.show();
        }

        if (time_reached(next_scan)) {
            scan_qwst();
            next_scan = make_timeout_time_ms(5000);
        }
        ++frame;
    }
}
