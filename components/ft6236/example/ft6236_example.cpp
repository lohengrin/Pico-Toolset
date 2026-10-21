// FT6236 smoke test: prints both touch points over USB serial. Uses the
// Presto preset; edit the config for another board.
#include <cstdio>

#include "pico/stdlib.h"
#include "pico_toolset/ft6236.h"
#include "pico_toolset/ft6236_configs.h"

int main() {
    stdio_init_all();
    sleep_ms(2000);

    pico_toolset::Ft6236 touch;
    touch.init(pico_toolset::configs::ft6236::kPimoroniPrestoHalfRes);

    while (true) {
        auto t1 = touch.read();
        auto t2 = touch.touch2();
        if (t1.pressed || t2.pressed) {
            std::printf("t1 %d (%u,%u)  t2 %d (%u,%u)\n", t1.pressed, t1.raw_x, t1.raw_y,
                        t2.pressed, t2.raw_x, t2.raw_y);
        }
        if (!touch.bus_ok()) std::printf("touch bus error, backing off\n");
        sleep_ms(10);
    }
}
