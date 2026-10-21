// Pimoroni Presto full-integration example (doubles as an integration
// test): ST7701 LCD + FT6236 touch + uSD + PSRAM + 7 WS2812 LEDs + piezo +
// QW/ST I2C scan, all together. See ../../docs/boards/pimoroni-presto.md.
//
// Screen: colour bars under a row of status squares (green = ok, red =
// failed: PSRAM, SD, touch bus, LCD). Touch draws a crosshair, plays a tone
// whose pitch follows X and brightens the LEDs with Y; the user button turns
// the LEDs white. The QW/ST bus is scanned every few seconds over USB serial.
//
// Pins/wiring come from the Presto presets; none have been run from this
// repo on hardware yet (derived from Pimoroni's firmware) -- check the
// serial log first.
#include "pico_toolset/ft6236.h"
#include "pico_toolset/ft6236_configs.h"
#include "pico_toolset/piezo.h"
#include "pico_toolset/piezo_configs.h"
#include "pico_toolset/psram.h"
#include "pico_toolset/psram_configs.h"
#include "pico_toolset/sdcard.h"
#include "pico_toolset/sdcard_configs.h"
#include "pico_toolset/st7701.h"
#include "pico_toolset/st7701_configs.h"
#include "pico_toolset/st7701_driver.h"
#include "pico_toolset/ws2812.h"
#include "pico_toolset/ws2812_configs.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include <cstdio>

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

void draw_status(DisplayDriver& d, int slot, bool ok) {
    const int s = kStatusH;
    d.fill_rect(slot * (s + 4) + 4, 2, slot * (s + 4) + 4 + s - 1, s - 1,
                ok ? kColorGreen : kColorRed);
}

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

    i2c_init(i2c0, 400'000);
    gpio_set_function(kQwstSdaPin, GPIO_FUNC_I2C);
    gpio_set_function(kQwstSclPin, GPIO_FUNC_I2C);
    gpio_pull_up(kQwstSdaPin);
    gpio_pull_up(kQwstSclPin);
    gpio_init(kUserButtonPin);
    gpio_set_dir(kUserButtonPin, GPIO_IN);
    gpio_pull_up(kUserButtonPin);

    St7701Driver display(g_lcd);
    display.set_backlight(255);

    uint32_t frame = 0;
    absolute_time_t next_scan = get_absolute_time();
    TouchSample prev{};
    while (true) {
        auto t = touch.read();

        if (lcd_ok) {
            display.flush(); // wait for vblank so the redraw trails the beam
            draw_bars(display);
            draw_status(display, 0, psram.present && psram.test_ok);
            draw_status(display, 1, sd_ok);
            draw_status(display, 2, touch.bus_ok());
            draw_status(display, 3, lcd_ok);
            if (t.pressed) {
                display.draw_line(t.raw_x, 0, t.raw_x, kSize - 1, kColorWhite);
                display.draw_line(0, t.raw_y, kSize - 1, t.raw_y, kColorWhite);
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
