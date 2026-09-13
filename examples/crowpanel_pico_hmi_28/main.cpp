// Elecrow CrowPanel PICO HMI 2.8" combination example (doubles as an
// integration test): ST7789 LCD (via the Screen/Widget composition) +
// XPT2046 touch + uSD card, all three sharing SPI1 with separate CS lines.
// See ../../boards/crowpanel_pico_hmi_28.md.
#include "pico_toolset/st7789.h"
#include "pico_toolset/st7789_configs.h"
#include "pico_toolset/buffered_display.h"
#include "pico_toolset/xpt2046.h"
#include "pico_toolset/xpt2046_configs.h"
#include "pico_toolset/sdcard.h"
#include "pico_toolset/sdcard_configs.h"
#include "pico_toolset/screen.h"
#include "pico_toolset/simple_font.h"
#include "pico/stdlib.h"

#include <cstdio>

using pico_toolset::BufferedDisplay;
using pico_toolset::Screen;
using pico_toolset::SdCard;
using pico_toolset::St7789;
using pico_toolset::TextWidget;
using pico_toolset::Xpt2046Touch;

// 320x240 RGB565 = 150KB -- most of the RP2040's 264KB SRAM. Fine for this
// smoke-test example; a real application should watch its total static +
// heap + stack budget carefully at this framebuffer size.
static uint16_t s_framebuffer[320 * 240];

int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("CrowPanel PICO HMI 2.8\" example: ST7789 + XPT2046 touch + SD\n");

    // -- Display (brings up SPI1) --
    St7789 lcd;
    bool lcd_ok = lcd.init(pico_toolset::configs::st7789::kElecrowCrowPanelPicoHmi28);
    printf("ST7789 init %s\n", lcd_ok ? "ok" : "FAILED");

    BufferedDisplay driver(lcd, s_framebuffer);
    Screen screen(driver);

    TextWidget status(4, 4, "CrowPanel PICO HMI 2.8", pico_toolset::kColorWhite,
                       pico_toolset::kColorBlack, pico_toolset::kGlyphFont5x8.glyphs,
                       pico_toolset::glyph_font_height);
    screen.set_widget(Screen::UL, &status);
    screen.update();

    // -- Touch (shares SPI1 -- take the bus from the display driver itself) --
    auto touch_cfg = pico_toolset::configs::xpt2046::kElecrowCrowPanelPicoHmi28;
    touch_cfg.spi_instance = lcd.spi();
    Xpt2046Touch touch;
    touch.init(touch_cfg);

    // -- SD card (also shares SPI1) --
    SdCard sd;
    if (!sd.init(pico_toolset::configs::sdcard::kElecrowCrowPanelPicoHmi28)) {
        printf("SD mount failed (FRESULT=%d)\n", sd.last_mount_result());
    } else {
        printf("SD mounted (native SPI: %s)\n", sd.used_native_spi() ? "yes" : "no");
        for (const auto& name : sd.list_files({"txt"})) printf("  %s\n", name.c_str());
    }

    // TextWidget stores a pointer, not a copy -- this buffer must outlive
    // every screen.update() that follows a set_text() call using it.
    char touch_buf[32];

    bool was_pressed = false;
    while (true) {
        Xpt2046Touch::RawSample sample = touch.read();
        if (sample.pressed && !was_pressed) {
            printf("touch pressed raw_x=%u raw_y=%u\n", sample.raw_x, sample.raw_y);
            snprintf(touch_buf, sizeof(touch_buf), "touch: %4u,%4u", sample.raw_x, sample.raw_y);
            status.set_text(touch_buf);
            screen.update();
        } else if (!sample.pressed && was_pressed) {
            printf("touch released\n");
        }
        was_pressed = sample.pressed;

        sleep_ms(20);
    }
}
