# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_executable(minico main.cpp)

target_link_libraries(minico
  como::wayland
  KF6IdleTimeComoPlugin
  KF6WindowSystemComoPlugin
  ComoQpaPlugin
)

install(TARGETS minico)
