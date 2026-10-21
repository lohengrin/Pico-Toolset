# lsm6ds3

LSM6DS3 6-axis IMU (the motion sensor of the Multi-Sensor Stick).

| | |
|---|---|
| Target / option | `pico_toolset_lsm6ds3` / `PICO_TOOLSET_BUILD_LSM6DS3` (ON) |
| Links | public `pico_stdlib`, `hardware_i2c` |
| Example | `components/lsm6ds3/example/lsm6ds3_example.cpp` |
| Presets | `kPimoroniPrestoMultiSensorStick` (I2C0, 0x6A; validated) |
| Boards | [Pimoroni Presto](../boards/pimoroni-presto.md) |

## API

- `Lsm6ds3Config { i2c; addr (0x6A/0x6B); odr_mode = 0x40 }` -- `odr_mode` is written to CTRL1_XL/CTRL2_G (0x40 = 104 Hz, 0x50 = 208 Hz, 0x60 = 416 Hz, 0x02 = 26 Hz low power).
- `bool init(cfg)` -- enables accelerometer, gyro, pedometer, tap detection; false only when the device does not answer. `who_am_i()` is recorded, not enforced.
- `read_raw(Lsm6ds3Raw&)` (LSB) and `read(Lsm6ds3Sample&)` (g and degrees per second). Fixed at +-2 g and +-250 dps.
- `step_count()`, `reset_step_count()`, `tilt_detected()`, `significant_motion_detected()`, `single_tap_detected()`, `double_tap_detected()`, `free_fall_detected()`.

Lineage: Pimoroni's Presto `lsm6ds3.py` (MIT).
