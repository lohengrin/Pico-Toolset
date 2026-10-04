# reset_buttons

Up to eight debounced momentary buttons, plus a generic "reboot into mode X"
mechanism that carries a tag across a watchdog reset.

| | |
|---|---|
| Target / option | `pico_toolset_reset_buttons` / `PICO_TOOLSET_BUILD_RESET_BUTTONS` (ON) |
| Links | public `pico_stdlib`; private `hardware_gpio`, `hardware_watchdog` |
| Example | `components/reset_buttons/example/reset_buttons_example.cpp` (three buttons {14,15,16}, each rebooting with its own tag) |
| Presets | `configs::buttons::kPimoroniPicoDisplayPack` only (Pico DV buttons 14/15/16 active-high have no preset) |
| Scratch registers | uses `scratch[0]` (magic) and `scratch[1]` (tag) |

## Config -- `ButtonsConfig`

`pins` (`std::array<uint8_t, 8>`), `count`, `active_low`, `debounce_frames`; all
required, no defaults.

| Preset | Pins | Behaviour |
|---|---|---|
| `kPimoroniPicoDisplayPack` | {12, 13, 14, 15} = A top-left, B bottom-left, X top-right, Y bottom-right | active-low, 3-frame debounce |

## API -- `DebouncedButtons`

- `kMaxButtons` = 8.
- `bool init(std::span<const uint8_t> pins, int debounce_frames = 3, bool active_low = false)`
  (false if too many pins); `bool init(const ButtonsConfig&)`.
- `int poll()` -- edge-triggered: the index of a button whose debounce threshold was
  *just* reached, else -1. Call once per frame.
- `uint8_t held_mask()`; `std::string raw_state()` (instantaneous, for wiring
  diagnosis).

## Tagged reboot

- `[[noreturn]] void watchdog_reboot_with_tag(uint32_t tag)`
- `bool consume_pending_watchdog_tag(uint32_t& out_tag)` -- returns false on a normal
  power-on, and clears the marker.

The tag survives a `watchdog_reboot()` but reads back 0 on power-on, so the pair
distinguishes "rebooted with a tag" from a cold start. Check it early in `main()`.

## Gotchas

- Only one pending tag fits; anything else writing `scratch[0]`/`[1]` collides.
- Pico DV buttons are active-**high** (pull-down inputs); the legacy span `init`
  defaults to that. Display Pack buttons are active-low.
