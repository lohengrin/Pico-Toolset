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

## `SampleMixer<N>` (`sample_mixer.h`)

N-voice (default 8) player for **8-bit unsigned PCM** (DOOM DMX lumps, Sound Blaster bodies, retro sample
banks): per-voice source rate, volume (0..127) and playback `speed`, linear interpolation, mixed into a
caller-supplied `int32_t` accumulator (`mix(acc, count)` adds; full scale per voice is +-127 x 127). Voices are
identified by caller-chosen handles; a new `start()` steals the voice with the lowest handle when all are busy.
`set_rate()`, `start()`, `stop()`, `stop_all()`, `set_volume()`, `playing()`, `active()`. No hardware, no
allocation, not thread-safe by itself (guard `start/stop` against a concurrent `mix` with an interrupt-off
section or by calling from one context); the samples must stay readable from the mixing context.
Used by PicoDoom to play sound effects on the Presto piezo (`PiezoPcm`) and to a Bluetooth headset (A2DP).

