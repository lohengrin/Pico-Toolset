add_library(pico_toolset_json STATIC
    src/json_reader.cpp
)
target_include_directories(pico_toolset_json PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_compile_features(pico_toolset_json PUBLIC cxx_std_20)

add_executable(json_example example/json_example.cpp)
target_link_libraries(json_example PRIVATE
    pico_toolset_json
    pico_stdlib
)
pico_enable_stdio_usb(json_example ON)
pico_add_extra_outputs(json_example)
