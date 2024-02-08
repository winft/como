# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(idletime_plugin_SRCS
    poller.cpp
)

add_library(KF6IdleTimeComoPlugin OBJECT ${idletime_plugin_SRCS})
target_compile_definitions(KF6IdleTimeComoPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF6IdleTimeComoPlugin
  KF6::IdleTime
  base
)
