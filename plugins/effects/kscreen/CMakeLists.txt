# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

#######################################
# Effect

# Source files
set(kscreen_SOURCES
    kscreen.cpp
    main.cpp
)

kconfig_add_kcfg_files(kscreen_SOURCES kscreenconfig.kcfgc)
kwin_add_builtin_effect(kscreen ${kscreen_SOURCES})
target_link_libraries(kscreen PRIVATE
  KF6::ConfigGui
  render
)
