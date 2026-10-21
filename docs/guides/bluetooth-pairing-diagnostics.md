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
