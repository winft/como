# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

#######################################
# Effect

# Source files
set(sheet_SOURCES
    main.cpp
    sheet.cpp
)
kconfig_add_kcfg_files(sheet_SOURCES sheetconfig.kcfgc)

kwin_add_builtin_effect(sheet ${sheet_SOURCES})
target_link_libraries(sheet PRIVATE
  KF6::ConfigGui
  render
)
