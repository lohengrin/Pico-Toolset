# pico_toolset -- helper to make LVGL available as the `lvgl` target.
#
# Consumer contract: set LV_CONF_PATH (the consumer's own lv_conf.h -- memory
# pool size, fonts and widgets are per-application tuning, not toolset
# policy) BEFORE including this file, or set PICO_LVGL_DIR to a checkout.
if(TARGET lvgl)
    return()
endif()
if(NOT LV_CONF_PATH)
    message(FATAL_ERROR "pico_toolset: set LV_CONF_PATH to your lv_conf.h before including pico_lvgl.cmake")
endif()
set(LV_CONF_PATH "${LV_CONF_PATH}" CACHE STRING "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
if(DEFINED PICO_LVGL_DIR)
    add_subdirectory(${PICO_LVGL_DIR} ${CMAKE_BINARY_DIR}/lvgl)
else()
    include(FetchContent)
    FetchContent_Declare(lvgl GIT_REPOSITORY https://github.com/lvgl/lvgl.git GIT_TAG v9.2.2)
    FetchContent_MakeAvailable(lvgl)
endif()
