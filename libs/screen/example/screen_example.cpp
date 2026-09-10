// Screen abstraction example (doubles as an integration test). Composes a
// two-slot HUD (status text + a progress bar) on the SSD1306 via the
// pluggable DisplayDriver interface. See screen.h for the slot model and
// display_driver.h / ssd1306_driver.h / ili9486_driver.h for backends.
#include "pico_toolset/screen.h"
#include "pico_toolset/simple_font.h"
#include "pico_toolset/ssd1306.h"
#include "pico_toolset/ssd1306_driver.h"
#include "pico/stdlib.h"

#include <cstdio>

using namespace pico_toolset;

int main() {
    stdio_init_all();
    sleep_ms(1500);

    Ssd1306Config oled_cfg;
    oled_cfg.i2c_instance = i2c1;
    oled_cfg.sda_pin = 19;
    oled_cfg.scl_pin = 18;
    oled_cfg.i2c_freq_hz = 400000;
    oled_cfg.address = 0x3C;
    oled_cfg.width = 128;
    oled_cfg.height = 64;

    Ssd1306 oled;
    if (!oled.init(oled_cfg)) {
        printf("SSD1306 init failed\n");
        return 1;
    }

    Ssd1306Driver driver(oled);
    Screen screen(driver);

    // Status line in the upper-left slot and a full-width bar in the lower left.
    TextWidget status(4, 4, "CPU: 000%", kColorWhite, kColorBlack,
                      kGlyphFont5x8.glyphs, glyph_font_height);
    RectWidget bar(4, 40, 0, 12, kColorWhite);

    // Swap the full-screen slot between two widgets to show FS composition.
    RectWidget frame(2, 2, 124, 60, kColorWhite);
    RectWidget hole(4, 4, 120, 56, kColorBlack);

    screen.set_widget(Screen::UL, &status);
    screen.set_widget(Screen::BL, &bar);

    uint8_t percent = 0;
    while (true) {
        // Animate the widget state, then re-compose + flush one frame.
        char text[16];
        snprintf(text, sizeof(text), "CPU: %03u%%", percent);
        status.set_text(text);
        bar = RectWidget(4, 40, static_cast<int>(4 + (124 - 8) * percent / 100), 12, kColorWhite);
        screen.set_widget(Screen::BL, &bar);

        screen.set_widget(Screen::FS, (percent % 20 == 0) ? static_cast<Widget*>(&frame)
                                                          : static_cast<Widget*>(&hole));
        screen.update();

        percent = static_cast<uint8_t>((percent + 1) % 100);
        sleep_ms(200);
    }
}