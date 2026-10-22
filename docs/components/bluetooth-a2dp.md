# bluetooth_a2dp

Bluetooth **A2DP source**: streams generated audio (an [`AudioSource`](audio-synth.md)) to a
Bluetooth headset or speaker, with SBC bit-rate presets, volume control, pairing and
reconnect to the last headset. Playback only: no microphone / hands-free, no file playback.

> **Status: validated on the Presto with a Sony MDR-ZX770BN** (pairing, SBC negotiation at 44.1 kHz
> joint stereo / bitpool 25 = 173 kbps, continuous generated tone, no dropped frames, ~0.33 ms
> encode time per SBC frame at the default 150 MHz clock). Not yet tested: the HUAWEI FreeBuds 6i,
> volume control on a headset (the Sony's remote volume was not exercised), the other quality
> presets, and a reconnect after a reset. At the first connect the example logs the headset's SBC
> capabilities; paste that log if something is off.

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

## Pairing flow and lessons learned (Sony MDR-ZX770BN)

`start_pairing()` runs an inquiry for audio devices and connects to the first one whose class is a
headset / headphones / speaker (set-top boxes and TVs share the Audio/Video major class and are
reported but skipped). Then:

1. Any stored link key for that device is dropped first. A headset in pairing mode has forgotten the
   old bond and answers a stale key with status `0x63` ("PIN or key missing").
2. The device is bonded on its own (`gap_dedicated_bonding`, no MITM -- we are display-only, the
   headset display-yes/no, so an authenticated link is impossible and BTstack rejected the
   confirmation otherwise). Older Sonys only show their A2DP record on an authenticated link: opening
   the stream first fails with SDP `0x82`.
3. BTstack drops the bonding link, and many headsets then call back by themselves. We wait 6 s for
   that (an incoming connection is adopted and the stream opened), then page the headset ourselves,
   retrying 3 times 2.5 s apart. Paging at once made this Sony answer `0x1F` every time.
4. While pairing/connecting/streaming, `BluetoothClassic::inhibit_background_paging()` is held: the
   background page of a bonded Classic gamepad (see below) otherwise blocks the radio for up to 15 s
   and the headset leaves pairing mode before our attempt gets through. A page already in flight is
   cancelled.

Build requirements worth knowing: `HCI_ACL_PAYLOAD_SIZE` is raised to 1 KB under A2DP (at the
default 255 bytes an SBC packet holds only 3 frames and the stream starves), `MAX_NR_AVRCP_CONNECTIONS`
is 2 (the headset opens its own remote-control channel while we open ours) and our AVRCP connect is
made only after the stream is open. The SBC encoder runs in the BTstack context on core 0's stack, so
give it room (`PICO_STACK_SIZE=0x1000` in the Presto example).

Debugging aids: `A2dpEventType::Disconnected` carries the HCI reason (`status`; `0x16` only means *we*
closed the link, whatever the underlying reason) and `cause` (which API call closed it, if any --
another process writing keys to the serial console once triggered `forget`). `stats()` includes encode
time per SBC frame. With the Presto's `PRESTO_BT_HCI_DUMP=ON` the HCI packets of each connection
attempt are logged.

## First test (Presto)

1. `cmake -S examples/presto -B build -DPRESTO_BT_A2DP=ON`, flash, open the serial console.
2. Put the headset in pairing mode (keep it close, and unpair it from phones first; FreeBuds 6i: hold the case button until the LED blinks white;
   MDR-ZX770BN: hold the power button ~7 s until it announces pairing).
3. Touch the status band, `AUDIO (HEADSET)`, `PAIR` (or press `a` on the console). Expect
   `found` -> `connecting` -> (a few seconds) -> `connected` -> `Capabilities` -> `Configured` ->
   `streaming`; a tone should play. Bonding is followed by a deliberate wait of up to ~6 s.
4. `+`/`-`, `q` (quality), `w` (sound), `p` (pause), `s` (stats; `dropped` should stay ~0).
5. Reset the board: `connect_last()` / auto-reconnect should bring the headset back (it must be on
   and not connected to a phone).

Pairing problems: [Bluetooth pairing diagnostics](../guides/bluetooth-pairing-diagnostics.md).
Credit: flow after BTstack's `a2dp_source_demo` (Bluedroid SBC encoder from BTstack).
