// RGB LED example (doubles as an integration test): cycles red, green, blue,
// white, off.
#include "pico_toolset/rgb_led.h"
#include "pico_toolset/rgb_led_configs.h"
#include "pico/stdlib.h"

int main() {
    // On a different board, copy this preset and change only the fields that
    // differ (pins, polarity).
    pico_toolset::RgbLed led;
    led.init(pico_toolset::configs::rgb_led::kPimoroniPicoDisplayPack);

    while (true) {
        led.set_rgb(255, 0, 0);     sleep_ms(500);
        led.set_rgb(0, 255, 0);     sleep_ms(500);
        led.set_rgb(0, 0, 255);     sleep_ms(500);
        led.set_rgb(255, 255, 255); sleep_ms(500);
        led.set_rgb(0, 0, 0);       sleep_ms(500);
    }
}
