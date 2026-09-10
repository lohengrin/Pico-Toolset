# Pico-Toolset consumer integration helper
# Include this file or use FetchContent directly:
#
#   FetchContent_Declare(
#       pico_toolset
#       GIT_REPOSITORY https://github.com/<org>/Pico-Toolset.git
#       GIT_TAG        main
#   )
#   FetchContent_MakeAvailable(pico_toolset)
#
# Then link only the components you need:
#
#   target_link_libraries(my_project PRIVATE
#       pico_toolset_ssd1306
#       pico_toolset_ili9486
#       pico_toolset_psram        # RP2350 only
#       pico_toolset_usb_hid
#       pico_toolset_screen
#   )
