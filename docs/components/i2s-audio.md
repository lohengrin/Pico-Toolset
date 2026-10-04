# i2s_audio

Float-sample I2S DAC output (for example the Pico DV's PCM5100A) over pico-extras'
`pico_audio_i2s`, with a non-blocking sample queue.

| | |
|---|---|
| Target / option | `pico_toolset_i2s_audio` / `PICO_TOOLSET_BUILD_I2S_AUDIO` (**OFF** by default) |
| Dependency | pico-extras. The *consumer* must run `pico_extras_import.cmake` **before** `project()`; the component fails with a clear error if the `pico_audio_i2s` target does not exist |
| Links | public `pico_stdlib`, `pico_audio_i2s` |
| Example | `components/i2s_audio/example/i2s_audio_example.cpp` (continuous sine) |
| Presets | `configs::i2s_audio::kPicoDvCarrier` |
| Board | [Pimoroni Pico DV Demo Base](../boards/pimoroni-pico-dv-demo-base.md) |

## Config -- `I2sAudioConfig`

| Field | Default | Notes |
|---|---|---|
| `sample_rate_hz` | 44100 | |
| `channel_count` | 1 | 1 = mono duplicated to L/R, 2 = stereo |
| `pin_data` | -- | **must set** |
| `pin_clock_base` | -- | **must set**; BCK here, LRCK = +1 (consecutive pins required) |
| `dma_channel` | -- | **must set**; `pico_audio_i2s` claims *this exact channel* (no auto-claim) |
| `pio_sm` | 0 | state machine inside the PIO block `pico_audio_i2s` selects (PIO0 by default) |
| `buffer_count` | 4 | |
| `samples_per_buffer` | 882 | 20 ms at 44.1 kHz |

Preset `kPicoDvCarrier`: 44.1 kHz, mono, DATA 26, BCK 27 (LRCK 28), DMA 0, SM 0,
4 buffers of 882.

## API -- `I2sAudioOutput`

- `bool init(cfg)`.
- `void queue_samples(std::span<const float>)` -- non-blocking; if no buffer is
  free the call's audio is dropped; a short remainder is zero-filled.

## Gotchas

- `audio_i2s_setup()` panics with "DMA channel N is already claimed" if another
  driver took the channel. Pick one nothing else claims (Pico-PIO-USB hard-codes
  a default; `Ili9486`/`St7789` auto-claim and avoid it).
- `pico_audio_i2s` uses PIO0 by default, which collides with DVI on RP2040 (see
  the [Pico DV board doc](../boards/pimoroni-pico-dv-demo-base.md)).
- HDMI digital audio is a separate path: [dvi_hdmi](dvi-hdmi.md).

Lineage: I2S DAC output for a PCM5100A over `pico_audio_i2s`.
