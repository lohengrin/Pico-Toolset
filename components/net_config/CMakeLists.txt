# lwIP finds lwipopts.h through the include path of the final executable, so it
# has to ship as a target that both pico_toolset_wifi and pico_toolset_https_client
# (anything pulling in cyw43_arch/lwIP) link. One lwipopts.h per build: do not
# combine with a project-local one.
add_library(pico_toolset_net_config INTERFACE)
target_include_directories(pico_toolset_net_config INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)
