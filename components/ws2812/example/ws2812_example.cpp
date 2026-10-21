// WS2812 smoke test: rainbow chase along the chain (Presto preset).
#include "pico/stdlib.h"
#include "pico_toolset/ws2812.h"
#include "pico_toolset/ws2812_configs.h"

int main() {
    pico_toolset::Ws2812 leds;
    if (!leds.init(pico_toolset::configs::ws2812::kPimoroniPresto)) return 1;

    float phase = 0;
    while (true) {
        for (size_t i = 0; i < leds.count(); ++i)
            leds.set_hsv(i, phase + i * (360.0f / leds.count()), 1.0f, 0.2f);
        leds.show();
        phase += 4;
        sleep_ms(20);
    }
}
