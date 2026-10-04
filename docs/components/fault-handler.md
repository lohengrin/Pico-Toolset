# fault_handler (library)

Hard-fault handler that stashes the faulting PC, LR and CFSR in watchdog scratch
registers, reboots, and reports them on the **next** boot instead of the SDK's
silent halt. A library (`libs/fault_handler`), not a component: no board wiring is
involved. It has a source file (`src/fault_handler.cpp`), so it is not header-only.

| | |
|---|---|
| Target / option | `pico_toolset_fault_handler` / `PICO_TOOLSET_BUILD_FAULT_HANDLER` (ON) |
| Links | public `pico_stdlib`; private `hardware_watchdog` |
| Example | `libs/fault_handler/example/fault_handler_example.cpp` |
| Scratch registers | `[2]` magic, `[3]` PC, `[5]` LR, `[6]` CFSR |

## API

- `struct FaultInfo { pc, lr, cfsr }`
- `bool consume_pending_hard_fault(FaultInfo&)` -- programmatic access; clears the marker.
- `bool report_pending_hard_fault(const char* tag = nullptr)` -- prints a report.

No init call: linking the library overrides the SDK's weak `isr_hardfault`.
PicoDoom calls `report_pending_hard_fault("PicoDoom")` early in `main()`.

## Notes

- The report cannot be printed from fault context: pumping TinyUSB/stdio_usb there
  re-enters the USB stack and can itself hang.
- Written for the Cortex-M33; there are no `#if PICO_RP2350` guards, so an RP2040
  build compiles an M33-oriented handler -- *unverified* on RP2040.
- Moved off scratch `[0..3]` to `[2]/[3]/[5]/[6]` so it does not collide with
  `reset_buttons` (`[0]`/`[1]`); `[4]` belongs to the SDK. See the
  [scratch table](README.md#watchdog-scratch-registers).
