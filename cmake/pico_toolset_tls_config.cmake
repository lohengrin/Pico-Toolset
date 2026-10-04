# Points pico-sdk's mbedtls at the config shipped with pico_toolset_https_client.
# pico-sdk reads PICO_MBEDTLS_CONFIG_FILE inside pico_sdk_init(), so include this
# file BEFORE pico_sdk_init() (the toolset's own top-level does it when
# PICO_TOOLSET_BUILD_HTTPS_CLIENT is ON). Skip it if your project supplies its own
# mbedtls config -- it then has to keep the settings of the shipped one.
if(NOT PICO_MBEDTLS_CONFIG_FILE)
    set(PICO_MBEDTLS_CONFIG_FILE
        ${CMAKE_CURRENT_LIST_DIR}/../components/https_client/config/mbedtls_config_wrapper.h
        CACHE FILEPATH "mbedtls config used by pico_toolset_https_client")
endif()
