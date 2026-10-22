# bluetooth_a2dp

Bluetooth **A2DP source**: streams generated audio (an [`AudioSource`](audio-synth.md)) to a
Bluetooth headset or speaker, with SBC bit-rate presets, volume control, pairing and
reconnect to the last headset. Playback only: no microphone / hands-free, no file playback.

> **Status: written and builds, not yet tested on hardware.** The intended test devices are a
> HUAWEI FreeBuds 6i and a Sony MDR-ZX770BN. At the first connect the example logs the headset's
> SBC capabilities (see "First test" below); please paste that log if something is off.

| | |
|---|---|
| Target / option | `pico_toolset_bluetooth_a2dp` / `PICO_TOOLSET_BUILD_BLUETOOTH_A2DP` (**OFF**; forces `PICO_TOOLSET_BLUETOOTH_CLASSIC` and needs `PICO_TOOLSET_BUILD_BLUETOOTH`) |
| Links | INTERFACE library with sources: `pico_toolset_bluetooth`, `pico_toolset_audio_synth`, `pico_btstack_sbc_encoder` |
| Example | `components/bluetooth_a2dp/example/bluetooth_a2dp_example.cpp` (~480 KB flash, ~20 KB RAM); integrated in [`examples/presto`](../boards/pimoroni-presto.md) (`-DPRESTO_BT_A2DP=ON`) |
| Optional | not compiled unless linked; defines `PICO_TOOLSET_BLUETOOTH_A2DP=1`, which is what enlarges the BTstack AVDTP/AVRCP pools in `btstack_config.h`. HID-only builds keep their smaller pools |

## Using it

```cpp
cyw43_arch_init();
Bluetooth bt;  bt.init({});
BluetoothA2dp a2dp;
A2dpConfig cfg;               // quality = Low by default
cfg.source = &my_source;      // an AudioSource (audio_synth has tones, sweeps, melodies)
cfg.callback = on_event;      // runs in the BTstack context: only record state
a2dp.init(cfg);               // reconnects the last headset once (auto_reconnect)
a2dp.start_pairing();         // headset in pairing mode: inquiry, connects to the first audio device
a2dp.set_volume(80);          // 0..127
```

Other calls: `connect(addr)`, `connect_last()`, `disconnect()`, `forget_last()`, `play()`,
`pause()`, `set_source()`, `set_quality()`, `set_custom()`, `state()`, `stats()`.
The last headset (address + name) is stored in BTstack's flash TLV store next to the link keys,
so bonding writes flash exactly as described in [bluetooth_hid](bluetooth-hid.md).

## Quality presets (SBC bit rate)

A2DP audio is SBC. Sinks must support 44.1 and 48 kHz; 16 and 32 kHz are optional, so the
presets keep 44.1 kHz and cut bandwidth with mono and lower bitpools. Blocks 16, subbands 8 and
loudness allocation are used when the headset offers them. Rate = 8 x frame bytes x fs / (blocks x subbands).

| Preset | Mode / bitpool | ~kbps |
|---|---|---|
| `High` | joint stereo, 53 | 328 |
| `Medium` | joint stereo, 35 | 229 |
| **`Low`** (default) | joint stereo, 25 | 174 |
| `Mono` | mono, 24 | 154 |
| `MonoLow` | mono, 16 | 110 |
| `Custom` | `A2dpConfig::custom` (16/32/44.1/48 kHz, any mode, bitpool 2..53) | |

The bitpool is clamped into the range the headset accepts. A bitpool-only change applies
immediately; a change of rate or channel mode re-establishes the connection.
Lower bit rates mean smaller RTP packets and a smaller backlog buffer; the encoder state itself
is a few KB whatever the preset.

## Volume

`A2dpVolumeMode::Auto` (default) uses the headset's own volume through AVRCP absolute volume when
the headset supports it (`remote_volume_active()`), otherwise it scales the PCM before encoding
(software gain). The headset's buttons are reported as `VolumeChanged`.

## Events

`PairingStarted/Stopped`, `DeviceFound`, `Connecting`, `Capabilities` (the headset's SBC
capability bitmaps, for diagnostics), `Connected`, `Configured` (negotiated rate / mode / blocks /
subbands / bitpool range / resulting bit rate), `StreamStarted`, `StreamSuspended`,
`VolumeChanged`, `Disconnected`, `Failed`.

## First test (Presto)

1. `cmake -S examples/presto -B build -DPRESTO_BT_A2DP=ON`, flash, open the serial console.
2. Put the headset in pairing mode (FreeBuds 6i: hold the case button until the LED blinks white;
   MDR-ZX770BN: hold the power button ~7 s until it announces pairing).
3. Touch the status band, `AUDIO (HEADSET)`, `PAIR` (or press `a` on the console). Expect
   `found` -> `Capabilities` -> `Configured` -> `streaming`; a tone should play.
4. `+`/`-`, `q` (quality), `w` (sound), `p` (pause), `s` (stats; `dropped` should stay ~0).
5. Reset the board: `connect_last()` / auto-reconnect should bring the headset back (it must be on
   and not connected to a phone).

Pairing problems: [Bluetooth pairing diagnostics](../guides/bluetooth-pairing-diagnostics.md).
Credit: flow after BTstack's `a2dp_source_demo` (Bluedroid SBC encoder from BTstack).
