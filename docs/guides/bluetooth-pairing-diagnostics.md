# Bluetooth pairing diagnostics

How to find out why a Bluetooth keyboard, mouse or gamepad will not pair, connect or send input
with [`bluetooth_hid`](../components/bluetooth-hid.md). All of it was needed on the Presto while
bringing up a Microsoft LE keyboard and an old Classic "SFR Gamepad".

## 1. Read the `[hid]` log lines

`examples/presto` prints one line per event: `pairing-started`, `found`, `connecting`,
`passkey`, `paired`, `connected`, `disconnected`, `FAILED status 0x.. reason 0x..`, and for
Classic devices `CLASSIC cod 0x...`. The status/reason of `FAILED`:

| Log | Meaning |
|---|---|
| `status 0x05 reason 0x03` | the Security Manager refused the pairing parameters (BTstack default was "Secure Connections only"; `BluetoothHid::init()` turns that off) |
| `status 0x05 reason 0x05` | the device does not support the pairing method offered |
| `status 0x13` | the device hung up (often after receiving a pairing failure) |
| `status 0x56` | BTstack pool exhausted (`MAX_NR_HIDS_HOSTS` too small) |
| `status 0x11` after `paired` (Classic) | no usable HID service record over SDP (see below) |
| `status 0x04` / `0x08` | page / connection timeout: the device is not reachable (off, out of range, not connectable) |

## 2. Dump the HCI packets

```bash
cmake -S examples/presto -B build-presto -DPRESTO_BT_HCI_DUMP=ON
```

prints every HCI, L2CAP, SMP and SDP packet on the USB serial. The log is paused while scanning
and starts at the first `connecting` event, so the capture holds only the connection setup. Useful
decodes:

- SMP (L2CAP channel 6): `01` pairing request, `02` pairing response, `05` pairing failed (+ reason),
  `0B` security request. In a pairing request/response the bytes are I/O capability, OOB, auth
  requirements (bit 0 bonding, bit 2 MITM, bit 3 Secure Connections), key size, key distribution.
- ATT (channel 4): `1B` handle-value notification = one HID input report.
- SDP (PSM 1): `06` search-attribute request, `07` response. A response with only attribute
  `0x0001` means the device returns a stub record.

## 3. Compare with Linux

If the device pairs on a Linux machine, ask Linux what the device really is:

```bash
bluetoothctl info <MAC>        # name, class, UUIDs, legacy pairing, vendor/product (Modalias)
pip install hid-tools
sudo hid-recorder /dev/hidrawN # full HID report descriptor + decoded reports while you press controls
sudo btmon -w cap.btsnoop      # start BEFORE connecting; read with: btmon -r cap.btsnoop
```

`hid-recorder` gives the exact report layout; `btmon` shows how BlueZ sets the connection up
(a capture started after the device is connected only contains the report stream).

## Headsets (A2DP)

Use the `Capabilities` / `Configured` log lines from [bluetooth_a2dp](../components/bluetooth-a2dp.md):
the headset must be in pairing mode (or already bonded and idle, not connected to a phone). If
`found` never appears, check the headset's class of device (0x24xxxx audio). If `Failed` shows
an AVDTP/L2CAP status after `Connecting`, try `PRESTO_BT_HCI_DUMP` and compare with the headset's
capability bitmaps; `s` shows `dropped` frames when the link cannot keep up (lower the quality).

Failure codes seen on a Sony MDR-ZX770BN, in the order they appeared:

| Status | Meaning | Cause / fix |
|---|---|---|
| SDP `0x82` | service not found | stream opened before the link was authenticated: bond first |
| `0x63` | PIN or link key missing | stale link key kept from an earlier pairing: drop it before pairing |
| `0x1F` | unspecified error | our page right after bonding collides with the headset calling back: wait ~6 s, then retry |
| long `connecting`, then failure | radio busy | a bonded Classic gamepad is being paged in the background; paging is suspended while A2DP connects |
| `HCI reason 0x16` | closed by this device | look at the "closed by" text: an API call (e.g. serial input `x`/`F`) or the stack |

A headset that stops connecting right after a firmware update (new or changed Bluetooth services)
usually holds a stale record of the Pico: forget the headset (console `F`), clear the Pico from the
headset's list (pairing mode / reset), and pair again.

A set-top box in the room (class `0x28....`) can show up first in the inquiry; only
headset / headphone / speaker classes (`0x24....`) are connected automatically.

## Case file: the "SFR Gamepad" (Classic, broken SDP)

Symptoms: found by inquiry, pairs (legacy PIN `0000`), then `FAILED status 0x11`. The dump showed
the SDP search-attribute response containing only `ServiceClassIDList`, even on a paired link, so
BTstack's `hid_host` found no control/interrupt channel numbers. The Linux `hid-recorder` output
gave the real descriptor (Android-style gamepad, report id 3, 15 buttons + hat + 4 axes + 2
triggers). Fix in `bluetooth_hid`: after pairing, connect straight to the standard HID channels
(PSM 0x11 control, 0x13 interrupt), parse reports with that built-in descriptor, and page bonded
Classic pads in the background after a reset.

## Case file: Microsoft foldable keyboard (LE)

Symptoms: `FAILED status 0x13`, no passkey. The dump showed our pairing request answered by a
legacy (no Secure Connections) pairing response, then our own "pairing failed, reason 3": BTstack's
`sm_init()` enables Secure-Connections-only mode when SC is compiled in. Afterwards: report
notifications were delivered with a different packet type than connect events, and with a
report-id byte prepended (both handled in `bluetooth_hid.cpp`).
