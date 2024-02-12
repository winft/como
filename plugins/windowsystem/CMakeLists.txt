# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(kwindowsystem_plugin_SRCS
    plugin.cpp
    windoweffects.cpp
    windowsystem.cpp
)

add_library(KF6WindowSystemComoPlugin MODULE ${kwindowsystem_plugin_SRCS})
set_target_properties(KF6WindowSystemComoPlugin PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf6/kwindowsystem/"
)

target_link_libraries(KF6WindowSystemComoPlugin
PRIVATE
  render
  win
  KF6::WindowSystem
)

install(
  TARGETS KF6WindowSystemComoPlugin
  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/kwindowsystem
)
