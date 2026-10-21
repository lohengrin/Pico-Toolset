# piezo

Square-wave tone on a PWM pin (piezo or buzzer, usually through a transistor).

| | |
|---|---|
| Target / option | `pico_toolset_piezo` / `PICO_TOOLSET_BUILD_PIEZO` (ON) |
| Links | public `pico_stdlib`; private `hardware_gpio/pwm/clocks` |
| Example | `components/piezo/example/piezo_example.cpp` |
| Presets | `kPimoroniPresto` (GPIO43) |
| Boards | [Pimoroni Presto](../boards/pimoroni-presto.md) |

## API -- `Piezo`

- `PiezoConfig { pin; default_duty = 0.5 }`.
- `init(cfg)`; `set_tone(freq_hz, duty = default)` -- below 50 Hz it silences the
  output and returns false (the PWM divider cannot go lower); `stop()`.
- `play(const PiezoNote*, count)` -- blocking melody of `{freq_hz, duration_ms}`
  notes (`freq_hz == 0` is a rest), 10 ms gap after each note.

The PWM divider is the smallest that keeps the 16-bit wrap in range, so pitch
resolution is best at high frequencies. PWM only: no PCM or I2S.
