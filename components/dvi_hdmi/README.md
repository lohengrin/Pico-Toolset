# DVI/HDMI (`pico_toolset_dvi_hdmi`)

PIO-based DVI/TMDS serialiser + encoder, with optional HDMI data-island
digital audio. Drives the RP2040/RP2350's PIO blocks to bit-bang a DVI/HDMI
signal (three TMDS data lanes + clock) -- no HSTX peripheral needed, so it
works on any GPIO set a board wires to its connector, not just the RP2350's
fixed HSTX pad range.

This is the low-level signal-generation layer (PIO serialiser, TMDS encode,
scanline timing, the data-island packet/audio-ring machinery) -- pixel
content comes from whatever the consumer's own `dvi_inst` scanout callback
produces. TOM6809 (github.com/lohengrin/TOM6809)'s `PicoDviVideoOutput`/
`PicoHdmiAudioOutput` classes are one real, real-hardware-validated example
of that consumer layer, for reference.

## Provenance

Base library vendored from Waveshare's RP2350-PiZero C example repository
(`RP2350-PiZero/C/01-DVI/libdvi`), itself derived from
[Wren6991/PicoDVI](https://github.com/Wren6991/PicoDVI) (Luke Wren). That
source tree has no `.git`/remote (an extracted vendor resource download), so
this arrived as a plain file copy rather than a `FetchContent`/submodule
reference -- there is no stable upstream URL to pin to. `common_dvi_pin_configs.h`
is copied from the sibling `01-DVI/include/` directory (not `libdvi/` itself
in the upstream tree) -- it's a reference table of `dvi_serialiser_cfg`
pinouts for a handful of boards (including the Waveshare RP2350-PiZero's
`pico_sock_cfg`, matching that board's onboard TMDS connector), the same
"named preset" idea this toolset's own `<component>_configs.h` files apply
elsewhere, just from upstream rather than this project.

License: BSD 3-Clause (Copyright (c) 2021, Luke Wren) -- see `LICENSE` in
this directory. Applies to every vendored file (all but `audio_ring.*` and
`data_packet.*`, see below).

Originally extracted into TOM6809 (github.com/lohengrin/TOM6809) on
2026-09-06, moved here (its first consumer's own consolidated toolset) on
2026-09-12 once TOM6809's HDMI/audio work had stabilized -- see the git
history of both repos for the intermediate real-hardware debugging that
happened in between.

## HDMI digital audio (`PICO_TOOLSET_DVI_HDMI_AUDIO`)

`dvi.c`, `dvi.h`, `dvi_timing.c` and `dvi_timing.h` carry HDMI digital-audio
support (CEA-861 data islands: AVI/audio InfoFrames, audio clock
regeneration, and audio-sample packets riding the horizontal blanking
interval). Every changed or added block is tagged `// PATCH (hdmi-audio):`.
With `PICO_TOOLSET_DVI_HDMI_AUDIO` off (the default), these files compile to
byte-identical output to the vendored (pure-DVI, video-only) originals --
that's the regression firewall for consumers who only want video.

`audio_ring.{h,cpp}` and `data_packet.{h,cpp}` are new files in this
directory, not vendored from PicoDVI -- ported for TOM6809's HDMI-audio
addition from two further third-party sources, each credited in full in the
file's own header comment:

- **`audio_ring.{h,cpp}`** -- the lock-free SPSC sample ring feeding the
  data-island packetiser. Design follows
  [rh1tech/frank-hdmi-audio](https://github.com/rh1tech/frank-hdmi-audio)'s
  `frank_audio_ring.{c,h}` (BSD-3-Clause, itself layered on Wren6991/PicoDVI),
  with one deliberate behavior change: frank's own `get_write_size()`/
  `get_read_size()` take a `full` bool whose `false` branch has a documented
  sign-error that over-reports free space and corrupts in-flight samples
  (see frank-hdmi-audio's `docs/LLM_GUIDE.md`, "Audio plays but sounds
  broken"). This port only ever implements the correct (`full=true`)
  arithmetic -- there is no unsafe mode to accidentally call.
- **`data_packet.{h,cpp}`** -- CEA-861 InfoFrame/ACR/audio-sample packet
  encoders (TERC4-encoded with BCH parity, as an HDMI receiver expects). The
  encode algorithm and packet layouts are ported from Shuichi Takano's
  `dvi::DataPacket` ([github.com/shuichitakano/pico_lib](https://github.com/shuichitakano/pico_lib),
  `dvi/data_packet.{h,cpp}`, MIT License, Copyright (c) 2021 Shuichi Takano),
  translated from its original C++ class into the C-linkage free-function
  shape rh1tech/frank-hdmi-audio's `frank_data_packet.{c,h}` already uses
  (BSD-3-Clause, layered on Wren6991/PicoDVI) so it fits this library's
  plain-C `dvi.c`/`dvi.h` the same way frank's does.

Half-pre-filling the audio ring at init (rather than starting empty) follows
rh1tech/frank-hdmi-audio's `docs/LLM_GUIDE.md`, "Half-pre-fill the audio ring
at init" -- see `PicoHdmiAudioOutput`'s own doc comment in TOM6809 for the
consumer-side half of that.

## IRQ headroom stats (`PICO_TOOLSET_DVI_HDMI_IRQ_STATS`)

`dvi.c` and `dvi.h` also carry a second, independent patch: a cumulative-
microseconds counter around `dvi_dma_irq_handler()`'s body, tagged
`// PATCH (irq-stats):`. Lets a caller on the other core (`dvi_irq_us_accum()`)
see how much of core1's hard-real-time per-scanline deadline that handler
actually uses -- independent of `PICO_TOOLSET_DVI_HDMI_AUDIO`: build with
audio off for a baseline reading, on for the after reading, both with this
flag on, to measure exactly what the data-island work above costs. A
bring-up/measurement tool, not meant to ship on by default.

No other files here have been modified from the vendored source.

**If you patch anything on the per-scanline DMA IRQ path, read
`data_packet.h`'s "RAM residency" note first.** Upstream keeps that entire
path out of flash for a reason that is not obvious and fails in a way that
looks like anything but its actual cause.
