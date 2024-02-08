# SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_executable(kwin_x11 ${kwin_X11_SRCS} main_x11.cpp)
target_link_libraries(kwin_x11
  desktop-kde
  como::script
  como::x11
  KF6::Crash
)

kcoreaddons_target_static_plugins(kwin_x11 NAMESPACE "kwin/effects/plugins")

add_executable(kwin_wayland main_wayland.cpp)
target_link_libraries(kwin_wayland
  desktop-kde
  como::script
  como::wayland
  como::xwayland
  KF6IdleTimeComoPlugin
  KF6WindowSystemComoPlugin
  ComoQpaPlugin
  KF6::DBusAddons
)

kcoreaddons_target_static_plugins(kwin_wayland NAMESPACE "kwin/effects/plugins")
