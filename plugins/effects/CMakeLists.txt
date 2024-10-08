# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

include(ECMSetupQtPluginMacroNames)
ecm_setup_qtplugin_macro_names(
    JSON_ARG2
        "KWIN_EFFECT_FACTORY"
    JSON_ARG3
        "KWIN_EFFECT_FACTORY_ENABLED"
        "KWIN_EFFECT_FACTORY_SUPPORTED"
    JSON_ARG4
        "KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED"
    CONFIG_CODE_VARIABLE
        PACKAGE_SETUP_KWINEFFECTS_AUTOMOC_VARIABLES
)

add_subdirectory(private)

function(kwin_add_effect_config name)
    list(REMOVE_ITEM ARGV ${name})
    kcoreaddons_add_plugin(${name} INSTALL_NAMESPACE "kwin/effects/configs" SOURCES ${ARGV})
    target_compile_definitions(${name} PRIVATE -DTRANSLATION_DOMAIN=\"kwin\")
    target_link_libraries(${name}
      effects-interface
      KF6::I18n
      KF6::KCMUtils
      Qt::DBus
    )
endfunction()

# Add a CMake-time check for python3 to avoid failures during build.
find_package (Python3 COMPONENTS Interpreter)
add_feature_info("Python3" Python3_Interpreter_FOUND "Required to strip effects metadata")
set(KSEM_EXE "${CMAKE_CURRENT_SOURCE_DIR}/strip-effect-metadata.py")

function (kwin_strip_builtin_effect_metadata target metadata)
    set(stripped_metadata "${CMAKE_CURRENT_BINARY_DIR}/${metadata}.stripped")

    set(command ${KSEM_EXE} --source=${metadata} --output=${stripped_metadata})
    add_custom_command(
        OUTPUT ${stripped_metadata}
        COMMAND ${command}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${metadata}
        COMMENT "Stripping ${metadata}..."
    )
    set_property(TARGET ${target} APPEND PROPERTY AUTOGEN_TARGET_DEPENDS ${stripped_metadata})
endfunction()

macro(kwin_add_builtin_effect name)
    kcoreaddons_add_plugin(${name} SOURCES ${ARGN} INSTALL_NAMESPACE "kwin/effects/plugins")
    target_compile_definitions(${name} PRIVATE -DTRANSLATION_DOMAIN=\"kwin\")
    kwin_strip_builtin_effect_metadata(${name} metadata.json)
    install(FILES metadata.json DESTINATION ${KDE_INSTALL_DATADIR}/kwin/builtin-effects/ RENAME ${name}.json)
endmacro()

function(kwin_add_scripted_effect name source)
    kpackage_install_package(${source} ${name} effects kwin)

    # necessary so tests are found without installing
    file(COPY ${source}/contents ${source}/metadata.json DESTINATION ${CMAKE_BINARY_DIR}/bin/kwin/effects/${name})
endfunction()

add_subdirectory(backgroundcontrast)
add_subdirectory(blendchanges)
add_subdirectory(blur)
add_subdirectory(colorblindnesscorrection)
add_subdirectory(colorpicker)
add_subdirectory(cube)
add_subdirectory(cubeslide)
add_subdirectory(dialogparent)
add_subdirectory(diminactive)
add_subdirectory(dimscreen)
add_subdirectory(eyeonscreen)
add_subdirectory(fade)
add_subdirectory(fadedesktop)
add_subdirectory(fadingpopups)
add_subdirectory(fallapart)
add_subdirectory(frozenapp)
add_subdirectory(fullscreen)
add_subdirectory(glide)
add_subdirectory(highlightwindow)
add_subdirectory(invert)
add_subdirectory(kscreen)
add_subdirectory(lookingglass)
add_subdirectory(login)
add_subdirectory(logout)
add_subdirectory(magiclamp)
add_subdirectory(magnifier)
add_subdirectory(maximize)
add_subdirectory(mouseclick)
add_subdirectory(mousemark)
add_subdirectory(overview)
add_subdirectory(resize)
add_subdirectory(scale)
add_subdirectory(screenedge)
add_subdirectory(screenshot)
add_subdirectory(sessionquit)
add_subdirectory(sheet)
add_subdirectory(showfps)
add_subdirectory(showpaint)
add_subdirectory(slide)
add_subdirectory(slideback)
add_subdirectory(slidingpopups)
add_subdirectory(snaphelper)
add_subdirectory(squash)
add_subdirectory(startupfeedback)
add_subdirectory(thumbnailaside)
add_subdirectory(touchpoints)
add_subdirectory(trackmouse)
add_subdirectory(translucency)
add_subdirectory(windowaperture)
add_subdirectory(windowview)
add_subdirectory(wobblywindows)
add_subdirectory(zoom)
