# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

#######################################
# Effect

set(magiclamp_SOURCES
    magiclamp.cpp
    main.cpp
)

kconfig_add_kcfg_files(magiclamp_SOURCES
    magiclampconfig.kcfgc
)

kwin_add_builtin_effect(magiclamp ${magiclamp_SOURCES})
target_link_libraries(magiclamp PRIVATE
  KF6::ConfigGui
  render
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_magiclamp_config_SRCS magiclamp_config.cpp)
    ki18n_wrap_ui(kwin_magiclamp_config_SRCS magiclamp_config.ui)
    kconfig_add_kcfg_files(kwin_magiclamp_config_SRCS magiclampconfig.kcfgc)

    kwin_add_effect_config(kwin_magiclamp_config ${kwin_magiclamp_config_SRCS})
endif()
