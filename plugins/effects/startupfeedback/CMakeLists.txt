# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

#######################################
# Effect

# Source files
set(startupfeedback_SOURCES
    main.cpp
    startupfeedback.cpp
    startupfeedback.qrc
)

kwin_add_builtin_effect(startupfeedback ${startupfeedback_SOURCES})
target_link_libraries(startupfeedback PRIVATE
  KF6::WindowSystem
  Qt::DBus
  Qt::Widgets
  render
)
