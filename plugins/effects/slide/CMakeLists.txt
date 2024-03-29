# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

#######################################
# Effect

set(slide_SOURCES
    main.cpp
    slide.cpp
    springmotion.cpp
)

kconfig_add_kcfg_files(slide_SOURCES
    slideconfig.kcfgc
)

kwin_add_builtin_effect(slide ${slide_SOURCES})
target_link_libraries(slide PRIVATE
  KF6::ConfigGui
  render
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_slide_config_SRCS slide_config.cpp)
    ki18n_wrap_ui(kwin_slide_config_SRCS slide_config.ui)
    kconfig_add_kcfg_files(kwin_slide_config_SRCS slideconfig.kcfgc)

    kwin_add_effect_config(kwin_slide_config ${kwin_slide_config_SRCS})

    target_link_libraries(kwin_slide_config
      KF6::CoreAddons
    )
endif()
