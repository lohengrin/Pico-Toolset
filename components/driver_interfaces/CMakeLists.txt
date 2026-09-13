# Header-only, dependency-free interface component: the low-level
# DisplayPanel/TouchPanel contracts that concrete display/touch drivers
# implement so callers can swap chips without rewriting call sites. See
# display_panel.h/touch_panel.h for the interfaces themselves and
# AGENTS.md's "Kind interfaces" section for when to extend this pattern.
#
# Deliberately no PICO_TOOLSET_BUILD_* option -- this is a zero-cost header
# every relevant component depends on, not an optional feature to toggle.
add_library(pico_toolset_driver_interfaces INTERFACE)
target_include_directories(pico_toolset_driver_interfaces INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_driver_interfaces INTERFACE
    hardware_spi
)
target_compile_features(pico_toolset_driver_interfaces INTERFACE cxx_std_20)
