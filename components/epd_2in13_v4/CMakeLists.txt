add_library(pico_toolset_epd_paint STATIC
    paint/GUI/GUI_Paint.c
    paint/Fonts/font8.c
    paint/Fonts/font12.c
    paint/Fonts/font16.c
    paint/Fonts/font20.c
    paint/Fonts/font24.c
)
target_include_directories(pico_toolset_epd_paint PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/paint
    ${CMAKE_CURRENT_LIST_DIR}/paint/Config
    ${CMAKE_CURRENT_LIST_DIR}/paint/GUI
    ${CMAKE_CURRENT_LIST_DIR}/paint/Fonts
)
target_link_libraries(pico_toolset_epd_paint PUBLIC pico_stdlib)

add_library(pico_toolset_epd_2in13_v4 STATIC
    src/epd_2in13_v4.cpp
)
target_include_directories(pico_toolset_epd_2in13_v4 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_epd_2in13_v4 PUBLIC
    pico_stdlib
    hardware_spi
    pico_toolset_epd_paint
)
target_compile_features(pico_toolset_epd_2in13_v4 PUBLIC cxx_std_20)

add_executable(epd_2in13_v4_example example/epd_2in13_v4_example.cpp)
target_link_libraries(epd_2in13_v4_example PRIVATE
    pico_toolset_epd_2in13_v4
)
pico_enable_stdio_usb(epd_2in13_v4_example ON)
pico_add_extra_outputs(epd_2in13_v4_example)
