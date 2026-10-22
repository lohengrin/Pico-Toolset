# audio_synth

Header-only generated-audio sources for the audio sinks (today
[`bluetooth_a2dp`](bluetooth-a2dp.md)). No hardware, no dependencies.

| | |
|---|---|
| Target / option | `pico_toolset_audio_synth` (INTERFACE, header-only, pulled in by `bluetooth_a2dp`) |
| Headers | `pico_toolset/audio_source.h` (interface), `pico_toolset/audio_synth.h` (generators) |

## API

- `AudioSource`: `prepare(rate, channels)` is called whenever playback (re)starts, because the
  negotiated rate can differ between connections; `fill(int16_t* pcm, frames)` writes interleaved
  signed 16-bit PCM and is called from the sink's context (keep it short, no blocking calls).
- `ToneSource(Waveform, hz, amplitude)` -- sine (constexpr lookup table), square, triangle, saw.
- `SweepSource(f0, f1, seconds, amplitude)` -- repeating frequency sweep.
- `MelodySource(const AudioNote*, count, loop, waveform, amplitude)` -- `AudioNote{hz, ms}`, `hz == 0` is a rest.
- `SilenceSource`.

Implement `AudioSource` yourself to stream any other generated signal.
