# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

#######################################
# Effect

# Source files
set(fallapart_SOURCES
    fallapart.cpp
    main.cpp
)

kconfig_add_kcfg_files(fallapart_SOURCES
    fallapartconfig.kcfgc
)

kwin_add_builtin_effect(fallapart ${fallapart_SOURCES})
target_link_libraries(fallapart PRIVATE
  KF6::ConfigGui
  render
)
