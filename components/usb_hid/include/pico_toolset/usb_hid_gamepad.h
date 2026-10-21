#pragma once

// GamepadState and the kBt* button bits now live in components/hid_input so
// other HID transports (e.g. Bluetooth) share them; this header keeps the
// old include path working.
#include "pico_toolset/hid_gamepad.h"
