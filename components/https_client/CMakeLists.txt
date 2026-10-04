# INTERFACE library with sources (see components/wifi/CMakeLists.txt): the
# SDK's lwIP and mbedtls targets add their sources as INTERFACE sources, and
# they must be compiled exactly once, in the final executable. The platform
# time hook and psa_crypto_random.c have to sit next to those sources anyway.
add_library(pico_toolset_https_client INTERFACE)
target_sources(pico_toolset_https_client INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/https_client.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/mbedtls_platform_time.c
)
target_include_directories(pico_toolset_https_client INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_https_client INTERFACE
    pico_stdlib
    pico_toolset_net_config
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mbedtls
    pico_mbedtls
)
target_compile_features(pico_toolset_https_client INTERFACE cxx_std_20)

# mbedtls 3.6.6 added psa_crypto_random.c, which pico-sdk 2.3.x's pico_mbedtls
# source list predates. Compile it here instead of patching the SDK -- but only
# while the SDK still lacks it, or the symbols would be defined twice.
set(_psa_random ${PICO_MBEDTLS_PATH}/library/psa_crypto_random.c)
file(READ ${PICO_SDK_PATH}/src/rp2_common/pico_mbedtls/CMakeLists.txt _sdk_mbedtls_cmake)
string(FIND "${_sdk_mbedtls_cmake}" "psa_crypto_random.c" _sdk_has_psa_random)
if(EXISTS ${_psa_random} AND _sdk_has_psa_random EQUAL -1)
    target_sources(pico_toolset_https_client INTERFACE ${_psa_random})
endif()

# The example needs Wi-Fi credentials at configure time, e.g.
#   -DPICO_TOOLSET_EXAMPLE_WIFI_SSID=... -DPICO_TOOLSET_EXAMPLE_WIFI_PASSWORD=...
if(PICO_TOOLSET_EXAMPLE_WIFI_SSID AND TARGET pico_toolset_wifi)
    add_executable(https_client_example example/https_client_example.cpp)
    target_link_libraries(https_client_example PRIVATE
        pico_toolset_https_client
        pico_toolset_wifi
    )
    target_compile_definitions(https_client_example PRIVATE
        EXAMPLE_WIFI_SSID="${PICO_TOOLSET_EXAMPLE_WIFI_SSID}"
        EXAMPLE_WIFI_PASSWORD="${PICO_TOOLSET_EXAMPLE_WIFI_PASSWORD}"
    )
    pico_enable_stdio_usb(https_client_example ON)
    pico_add_extra_outputs(https_client_example)
endif()
