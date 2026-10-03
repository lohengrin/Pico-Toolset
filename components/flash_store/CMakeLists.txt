add_library(pico_toolset_flash_store STATIC
    src/flash_store.cpp
    src/flash_store_rp.cpp
)
target_include_directories(pico_toolset_flash_store PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_flash_store PUBLIC
    pico_stdlib
    pico_flash
    pico_multicore
    hardware_flash
)
target_compile_features(pico_toolset_flash_store PUBLIC cxx_std_20)

add_executable(flash_store_example example/flash_store_example.cpp)
target_link_libraries(flash_store_example PRIVATE
    pico_toolset_flash_store
)
pico_enable_stdio_usb(flash_store_example ON)
pico_add_extra_outputs(flash_store_example)
