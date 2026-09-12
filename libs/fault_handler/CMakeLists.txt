add_library(pico_toolset_fault_handler STATIC
    src/fault_handler.cpp
)
target_include_directories(pico_toolset_fault_handler PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_fault_handler PUBLIC
    pico_stdlib
)
target_link_libraries(pico_toolset_fault_handler PRIVATE
    hardware_watchdog
)
target_compile_features(pico_toolset_fault_handler PUBLIC cxx_std_20)

add_executable(fault_handler_example example/fault_handler_example.cpp)
target_link_libraries(fault_handler_example PRIVATE
    pico_toolset_fault_handler
)
pico_enable_stdio_usb(fault_handler_example ON)
pico_add_extra_outputs(fault_handler_example)
