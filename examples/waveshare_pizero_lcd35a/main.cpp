// Waveshare RP2350-PiZero + external Waveshare 3.5" RPi LCD (A) combination
// example (doubles as an integration test): ILI9486 LCD + XPT2046 touch
// (shared SPI bus) + PSRAM (works whether or not the chip is populated) +
// uSD card + USB HID host. See
// ../../boards/waveshare_rp2350_pizero_lcd35a.md.
#include "pico_toolset/ili9486.h"
#include "pico_toolset/ili9486_configs.h"
#include "pico_toolset/xpt2046.h"
#include "pico_toolset/xpt2046_configs.h"
#include "pico_toolset/psram.h"
#include "pico_toolset/psram_configs.h"
#include "pico_toolset/sdcard.h"
#include "pico_toolset/sdcard_configs.h"
#include "pico_toolset/usb_hid_host.h"
#include "pico_toolset/usb_hid_configs.h"
#include "pico/stdlib.h"

#include <cstdio>

using pico_toolset::Ili9486;
using pico_toolset::SdCard;
using pico_toolset::UsbHidHost;
using pico_toolset::Xpt2046Touch;

int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("Waveshare RP2350-PiZero + LCD(A) example: LCD+touch + PSRAM + SD + USB HID\n");

    // -- LCD + touch (shared SPI1 bus: bring the LCD up first, then hand the
    // display driver's own spi() to the touch config so they never disagree
    // about which bus they share -- see xpt2046_configs.h's own doc comment) --
    Ili9486 lcd;
    bool lcd_ok = lcd.init(pico_toolset::configs::ili9486::kWaveshareRp2350PiZero);
    printf("ILI9486 init %s\n", lcd_ok ? "ok" : "FAILED");

    auto touch_cfg = pico_toolset::configs::xpt2046::kWaveshareRp2350PiZero;
    touch_cfg.spi_instance = lcd.spi();
    Xpt2046Touch touch;
    touch.init(touch_cfg);

    if (lcd_ok) {
        constexpr uint16_t colours[] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000};
        for (uint16_t c : colours) {
            lcd.fill_solid(c);
            sleep_ms(300);
        }
    }

    // -- PSRAM: log status either way, don't fail if unpopulated --
    auto psram_status = pico_toolset::psram_init(pico_toolset::configs::psram::kWaveshareRp2350PiZero);
    printf("PSRAM present=%d test_ok=%d size=%zu bytes\n",
           psram_status.present, psram_status.test_ok, psram_status.size_bytes);

    // -- SD card --
    SdCard sd;
    if (!sd.init(pico_toolset::configs::sdcard::kWaveshareRp2350PiZero)) {
        printf("SD mount failed (FRESULT=%d)\n", sd.last_mount_result());
    } else {
        printf("SD mounted (native SPI: %s)\n", sd.used_native_spi() ? "yes" : "no");
        for (const auto& name : sd.list_files({"txt"})) printf("  %s\n", name.c_str());
    }

    // -- USB HID host (Lcd profile: core1 dedicated to the host stack, PIO0
    // free since no DVI is active in this combination) --
    UsbHidHost usb;
    usb.init(pico_toolset::configs::usb_hid::kWaveshareRp2350PiZeroLcd);

    bool was_pressed = false;
    while (true) {
        Xpt2046Touch::RawSample sample = touch.read();
        if (sample.pressed && !was_pressed) {
            printf("touch pressed raw_x=%u raw_y=%u\n", sample.raw_x, sample.raw_y);
        } else if (!sample.pressed && was_pressed) {
            printf("touch released\n");
        }
        was_pressed = sample.pressed;

        if (usb.connected_gamepad_count() > 0) {
            auto g = usb.gamepad_state(0);
            if (g.present) printf("pad0: btns=%04X\n", g.buttons);
        }

        sleep_ms(20);
    }
}
