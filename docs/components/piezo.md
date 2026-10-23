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
resolution is best at high frequencies.

## PCM -- `PiezoPcm` (RP2350 only)

Streams mono PCM through the same pin: the 8-bit duty cycle of a fast carrier
(`clk_sys / 256`, 586 kHz at 150 MHz) follows the samples, written by a
free-running DMA channel paced by a DMA timer -- no CPU time per sample, the sample rate
is `clk_sys * num / den` with the closest 16-bit pair (error < 0.01 %). Quality is a piezo's:
loud midrange, no bass; good for beeps, music and sound effects.

- `PiezoPcmConfig { pin; sample_rate_hz = 44100 }`; `init(cfg)` claims one DMA channel and one
  DMA timer and starts silent (no carrier). One instance only.
- `write(const int16_t*|const float* mono, count)` -- queues samples behind what is queued
  (full scale = +-32767 / +-1.0), starts the output if paused, returns how many were accepted;
  the rest is dropped when the 2048-sample (46 ms) ring is full. Never blocks.
- `queued()` / `free_space()`, `pause()` (stops the DMA, pin low: no carrier, no idle current)
  / `resume()` / `paused()`.
- Underrun guard: a 10 ms repeating timer notices when the DMA has overtaken the producer (e.g.
  a paused emulator) and covers the ring with silence instead of replaying old audio; the
  producer then restarts ~17 ms ahead. `underruns()` counts them.

Used by TOM6809's Presto port to play the emulated machine's sound when no Bluetooth
headset is connected.
