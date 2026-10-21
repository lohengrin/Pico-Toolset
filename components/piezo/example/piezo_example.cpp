// Piezo smoke test: plays a rising arpeggio, then repeats (Presto preset).
#include "pico/stdlib.h"
#include "pico_toolset/piezo.h"
#include "pico_toolset/piezo_configs.h"

int main() {
    pico_toolset::Piezo piezo;
    piezo.init(pico_toolset::configs::piezo::kPimoroniPresto);

    constexpr pico_toolset::PiezoNote melody[] = {
        {523, 200}, {659, 200}, {784, 200}, {1047, 400}, {0, 200},
    };
    while (true) {
        piezo.play(melody, sizeof(melody) / sizeof(melody[0]));
        sleep_ms(1000);
    }
}
