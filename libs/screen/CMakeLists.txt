if(PICO_TOOLSET_SCREEN_PIMORONI)
    set(SCREEN_EXTRA_LIBS "")
    set(SCREEN_EXTRA_FLAGS "")
    # pimoroni_pico needs to be available via PIMORONI_PICO_PATH; its
    # pico_graphics + display drivers are linked by the consumer as needed.
    if(NOT DEFINED PIMORONI_PICO_PATH)
        message(WARNING "PICO_TOOLSET_SCREEN_PIMORONI=ON but PIMORONI_PICO_PATH is not set")
    endif()
else()
    set(SCREEN_EXTRA_LIBS "")
    set(SCREEN_EXTRA_FLAGS "")
endif()

# Header-only component: the abstraction + adaptor drivers.
add_library(pico_toolset_screen INTERFACE)
target_include_directories(pico_toolset_screen INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_compile_features(pico_toolset_screen INTERFACE cxx_std_20)

# The example drives an SSD1306; only build it when that driver is enabled.
if(TARGET pico_toolset_ssd1306)
    add_executable(screen_example example/screen_example.cpp)
    target_link_libraries(screen_example PRIVATE
        pico_toolset_screen
        pico_toolset_ssd1306
    )
    pico_enable_stdio_usb(screen_example OFF)
    pico_add_extra_outputs(screen_example)
endif()