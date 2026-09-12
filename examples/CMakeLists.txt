# Superbuild: builds every full board/combination example in one pass, with
# no board/component flags to pass -- each combination needs its own
# PICO_BOARD (a single global cache variable per CMake configure), so this
# drives several independent nested pico-sdk configures via
# ExternalProject_Add rather than one shared configure. See
# ../boards/README.md for what each combination is.
#
# Usage (two commands, nothing else):
#   cmake -S examples -B examples-build
#   cmake --build examples-build
#
# Produces, under examples-build/uf2/:
#   pico_dv_pico1.uf2  pico_dv_pico1w.uf2  pico_dv_pico2.uf2
#   waveshare_pizero.uf2  waveshare_pizero_lcd35a.uf2
#
# Prerequisites (env vars, same ones the rest of this toolset needs):
#   PICO_SDK_PATH     -- required by every leg.
#   PICO_EXTRAS_PATH  -- required by the pico_dv legs (i2s_audio).
# Both are inherited by the nested `cmake` invocations automatically.
cmake_minimum_required(VERSION 3.20)
project(pico_toolset_examples NONE)

include(ExternalProject)

set(UF2_OUT ${CMAKE_BINARY_DIR}/uf2)
file(MAKE_DIRECTORY ${UF2_OUT})

# name, source subdir, extra -D args (semicolon-separated), output basename
function(pico_toolset_example_leg name source_dir extra_args output_name)
    set(bdir ${CMAKE_BINARY_DIR}/${name})
    set(uf2 ${bdir}/${output_name}.uf2)
    ExternalProject_Add(${name}
        SOURCE_DIR      ${CMAKE_CURRENT_LIST_DIR}/${source_dir}
        BINARY_DIR      ${bdir}
        CMAKE_ARGS      -DEXAMPLE_OUTPUT_NAME=${output_name} ${extra_args}
        BUILD_ALWAYS    TRUE
        BUILD_BYPRODUCTS ${uf2}
        INSTALL_COMMAND ""
    )
    add_custom_command(
        OUTPUT  ${UF2_OUT}/${output_name}.uf2
        COMMAND ${CMAKE_COMMAND} -E copy ${uf2} ${UF2_OUT}/${output_name}.uf2
        DEPENDS ${name}
        VERBATIM
    )
    list(APPEND PICO_TOOLSET_UF2_OUTPUTS ${UF2_OUT}/${output_name}.uf2)
    set(PICO_TOOLSET_UF2_OUTPUTS ${PICO_TOOLSET_UF2_OUTPUTS} PARENT_SCOPE)
endfunction()

pico_toolset_example_leg(pico_dv_pico1   pico_dv "-DPICO_BOARD=pico"    pico_dv_pico1)
pico_toolset_example_leg(pico_dv_pico1w  pico_dv "-DPICO_BOARD=pico_w"  pico_dv_pico1w)
pico_toolset_example_leg(pico_dv_pico2   pico_dv "-DPICO_BOARD=pico2"   pico_dv_pico2)
pico_toolset_example_leg(waveshare_pizero            waveshare_pizero            "" waveshare_pizero)
pico_toolset_example_leg(waveshare_pizero_lcd35a     waveshare_pizero_lcd35a     "" waveshare_pizero_lcd35a)

add_custom_target(uf2s ALL DEPENDS ${PICO_TOOLSET_UF2_OUTPUTS})
