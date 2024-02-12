# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(idletime_plugin_SRCS
    poller.cpp
)

add_library(KF6IdleTimeComoPlugin MODULE ${idletime_plugin_SRCS})
set_target_properties(KF6IdleTimeComoPlugin PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf6/org.kde.kidletime.platforms/"
)

target_link_libraries(KF6IdleTimeComoPlugin
  KF6::IdleTime
  input
)

install(
  TARGETS KF6IdleTimeComoPlugin
  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/org.kde.kidletime.platforms/
)
