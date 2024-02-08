# SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_executable(kwin_x11 ${kwin_X11_SRCS} main_x11.cpp)
target_link_libraries(kwin_x11
  desktop-kde
  kwinft::script
  kwinft::x11
  KF6::Crash
)

kcoreaddons_target_static_plugins(kwin_x11 NAMESPACE "kwin/effects/plugins")

add_executable(kwin_wayland main_wayland.cpp)
target_link_libraries(kwin_wayland
  desktop-kde
  kwinft::script
  kwinft::wayland
  kwinft::xwayland
  KF6IdleTimeKWinPlugin
  KF6WindowSystemKWinPlugin
  KWinQpaPlugin
  KF6::DBusAddons
)

kcoreaddons_target_static_plugins(kwin_wayland NAMESPACE "kwin/effects/plugins")
