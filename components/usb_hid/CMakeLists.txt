# USB HID host component: TinyUSB host stack (rhport 1, Pico-PIO-USB HCD)
# plus the Pico SDK's vendored TinyUSB (rhport 0 stays a CDC device). See
# cmake/pico_pio_usb.cmake for how `pio_usb` and `tinyusb_host` are provided.

if(NOT TARGET pico_pio_usb OR NOT TARGET tinyusb_host)
    message(FATAL_ERROR
        "pico_toolset_usb_hid requires pico_pio_usb and tinyusb_host; "
        "enable PICO_TOOLSET_BUILD_USB_HID via the top-level project "
        "and make sure cmake/pico_pio_usb.cmake ran."
    )
endif()

# Keyboard layouts implemented in usb_hid_keymap.cpp. Adding a new layout:
# implement it under a '#if PICO_TOOLSET_USB_HID_KEYMAP_<NAME>' block in
# usb_hid_keymap.cpp AND register the name here so its entry appears in the
# runtime registry (kKeymaps) and can be picked via the CMake lists below.
set(PICO_TOOLSET_USB_HID_KEYMAP_IMPL us fr)

# Semicolon-separated list of the keymaps to compile in (a subset of
# PICO_TOOLSET_USB_HID_KEYMAP_IMPL). Default "us;fr" ships both layouts; a
# lean build can pick just one (e.g. -DPICO_TOOLSET_USB_HID_KEYMAPS=us).
set(PICO_TOOLSET_USB_HID_KEYMAPS "us;fr" CACHE STRING
    "Semicolon-separated keyboard layouts to compile in (one of: ${PICO_TOOLSET_USB_HID_KEYMAP_IMPL})")

set(PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP "us" CACHE STRING
    "Layout used by default (must be in PICO_TOOLSET_USB_HID_KEYMAPS)")

foreach(k IN LISTS PICO_TOOLSET_USB_HID_KEYMAPS)
    if(NOT k IN_LIST PICO_TOOLSET_USB_HID_KEYMAP_IMPL)
        message(FATAL_ERROR
            "Unknown USB HID keymap '${k}' in PICO_TOOLSET_USB_HID_KEYMAPS. "
            "Supported: ${PICO_TOOLSET_USB_HID_KEYMAP_IMPL}.")
    endif()
endforeach()
if(NOT PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP IN_LIST PICO_TOOLSET_USB_HID_KEYMAPS)
    message(FATAL_ERROR
        "PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP (${PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP}) "
        "must be listed in PICO_TOOLSET_USB_HID_KEYMAPS (${PICO_TOOLSET_USB_HID_KEYMAPS}).")
endif()

list(LENGTH PICO_TOOLSET_USB_HID_KEYMAPS usb_hid_keymap_count)
list(FIND PICO_TOOLSET_USB_HID_KEYMAPS ${PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP} usb_hid_default_keymap_index)

add_library(pico_toolset_usb_hid STATIC
    src/usb_hid_host.cpp
    src/usb_hid_keymap.cpp
)
target_include_directories(pico_toolset_usb_hid PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    # Our tusb_config.h is found here; it must precede pico_stdio_usb's
    # (which is #if'd empty once tinyusb_host is linked) on the include path.
    ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(pico_toolset_usb_hid PUBLIC
    pico_pio_usb
    tinyusb_host
)
target_link_libraries(pico_toolset_usb_hid PRIVATE
    pico_stdlib
    hardware_sync
    hardware_dma
    pico_multicore
)
target_compile_features(pico_toolset_usb_hid PUBLIC cxx_std_20)

# Compile in the selected keymaps and their runtime registry metadata. PUBLIC
# so consumers (and the example) can read kKeymaps/kDefaultKeymapIndex from
# usb_hid_keymap.h.
foreach(k IN LISTS PICO_TOOLSET_USB_HID_KEYMAPS)
    string(TOUPPER ${k} k_upper)
    target_compile_definitions(pico_toolset_usb_hid PUBLIC
        PICO_TOOLSET_USB_HID_KEYMAP_${k_upper}=1)
endforeach()
target_compile_definitions(pico_toolset_usb_hid PUBLIC
    PICO_TOOLSET_USB_HID_KEYMAP_COUNT=${usb_hid_keymap_count}
    PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP_INDEX=${usb_hid_default_keymap_index}
)

# Linking tinyusb_host defines LIB_TINYUSB_HOST, which the SDK headers use to
# silently turn OFF stdio_usb defaults that every consumer still needs (see
# TOM6809's pico/CMakeLists.txt for the full derivation):
#   - own device descriptors instead of none,
#   - stdio_usb_init() still calls tusb_init() for rhport 0,
#   - the background IRQ still pumps tud_task(), and
#   - picotool reset via the RPI vendor interface is preserved.
# PUBLIC (not just set on usb_hid_example below) so every consumer of this
# target gets a working USB CDC console for free instead of having to
# re-derive and copy-paste this exact block themselves.
target_compile_definitions(pico_toolset_usb_hid PUBLIC
    PICO_STDIO_USB_USE_DEFAULT_DESCRIPTORS=1
    PICO_STDIO_USB_ENABLE_TINYUSB_INIT=1
    PICO_STDIO_USB_ENABLE_IRQ_BACKGROUND_TASK=1
    PICO_ENABLE_USB_RESET_VIA_VENDOR_INTERFACE=1
    PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF=2
    PICO_USB_RESET_SUPPORT_MS_OS_20_DESCRIPTOR=1
    PICO_ENABLE_USB_RESET_VIA_BAUD_RATE=1
)

add_executable(usb_hid_example example/usb_hid_example.cpp)
target_include_directories(usb_hid_example PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(usb_hid_example PRIVATE
    pico_toolset_usb_hid
)
pico_enable_stdio_usb(usb_hid_example ON)
pico_add_extra_outputs(usb_hid_example)