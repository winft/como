# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(input SHARED)

target_link_libraries(input
  PUBLIC
    win
    # TODO(romangg): for lock_screen.h, remove.
    PW::KScreenLocker
)

target_sources(input
  PUBLIC
    FILE_SET HEADERS
    FILES
      control/device.h
      control/keyboard.h
      filters/decoration_event.h
      filters/dpms.h
      filters/drag_and_drop.h
      filters/effects.h
      filters/fake_tablet.h
      filters/forward.h
      filters/global_shortcut.h
      filters/helpers.h
      filters/internal_window.h
      filters/keyboard_grab.h
      filters/lock_screen.h
      filters/move_resize.h
      filters/popup.h
      filters/screen_edge.h
      filters/tabbox.h
      filters/virtual_terminal.h
      filters/window_action.h
      filters/window_selector.h
      spies/activity.h
      spies/modifier_only_shortcuts.h
      spies/tablet_mode_switch.h
      spies/touch_hide_cursor.h
      xkb/keyboard.h
      xkb/keymap.h
      config.h
      cursor.h
      device_redirect.h
      event.h
      event_filter.h
      event_spy.h
      idle.h
      keyboard.h
      keyboard_redirect.h
      logging.h
      platform.h
      platform_qobject.h
      pointer.h
      pointer_redirect.h
      qt_event.h
      redirect_qobject.h
      singleton_interface.h
      switch.h
      touch.h
      types.h
      window_find.h
  PRIVATE
    control/device.cpp
    control/keyboard.cpp
    keyboard.cpp
    keyboard_redirect.cpp
    platform_qobject.cpp
    redirect_qobject.cpp
    singleton_interface.cpp
    switch.cpp
    cursor.cpp
    logging.cpp
    pointer.cpp
    xkb/keyboard.cpp
    xkb/keymap.cpp
)

add_library(input-x11-backend SHARED)

target_link_libraries(input-x11-backend
  PUBLIC
    base-x11
    input
    win-x11
    KF6::GlobalAccel
    X11::Xi
    XCB::CURSOR
    PkgConfig::XKBX11
)

target_sources(input-x11-backend
  PUBLIC
    FILE_SET HEADERS
    FILES
      x11/cursor.h
      x11/ge_event_mem_mover.h
      x11/global_shortcuts_manager.h
      x11/keyboard.h
      x11/keyboard_redirect.h
      x11/platform.h
      x11/pointer_redirect.h
      x11/redirect.h
      x11/window_selector.h
      x11/xfixes_cursor_event_filter.h
      x11/xinput_helpers.h
      x11/xinput_integration.h
      x11/xkb.h
  PRIVATE
    x11/cursor.cpp
    x11/global_shortcuts_manager.cpp
    x11/xfixes_cursor_event_filter.cpp
)

add_library(input-wl SHARED)

target_link_libraries(input-wl
  PUBLIC
    base-wl
    input
    KF6::GlobalAccel
    Wayland::Cursor
)

target_sources(input-wl
  PUBLIC
    FILE_SET HEADERS
    FILES
      control/config.h
      control/pointer.h
      control/pointer_config.h
      control/pointer_types.h
      control/switch.h
      control/touch.h
      dbus/dbus.h
      dbus/device.h
      dbus/device_helpers.h
      dbus/device_manager.h
      dbus/keyboard_layout.h
      dbus/keyboard_layouts_v2.h
      dbus/tablet_mode_manager.h
      extern/xcursor.h
      spies/keyboard_repeat.h
      wayland/cursor.h
      wayland/cursor_image.h
      wayland/device_redirect.h
      wayland/fake/devices.h
      wayland/fake/keyboard.h
      wayland/fake/pointer.h
      wayland/fake/touch.h
      wayland/global_shortcuts_manager.h
      wayland/idle.h
      wayland/input_method.h
      wayland/keyboard_redirect.h
      wayland/motion_scheduler.h
      wayland/platform.h
      wayland/pointer_redirect.h
      wayland/redirect.h
      wayland/tablet_redirect.h
      wayland/touch_redirect.h
      wayland/xcursor_theme.h
      xkb/helpers.h
      xkb/layout_manager.h
      xkb/layout_policies.h
      xkb/manager.h
      xkb/numlock.h
  PRIVATE
    control/pointer.cpp
    control/switch.cpp
    control/touch.cpp
    dbus/device.cpp
    dbus/device_manager.cpp
    dbus/keyboard_layout.cpp
    dbus/keyboard_layouts_v2.cpp
    dbus/tablet_mode_manager.cpp
    extern/xcursor.c
    logging.cpp
    wayland/global_shortcuts_manager.cpp
    wayland/kglobalaccel/runtime/component.cpp
    wayland/kglobalaccel/runtime/global_accel_d.cpp
    wayland/kglobalaccel/runtime/global_shortcut.cpp
    wayland/kglobalaccel/runtime/global_shortcut_context.cpp
    wayland/kglobalaccel/runtime/global_shortcuts_registry.cpp
    wayland/kglobalaccel/runtime/service_action_component.cpp
    wayland/kglobalaccel/runtime/sequence_helpers.cpp
    wayland/xcursor_theme.cpp
    xkb/layout_manager.cpp
    xkb/layout_policies.cpp
)

set_target_properties(input input-x11-backend input-wl PROPERTIES
  VERSION ${CMAKE_PROJECT_VERSION}
  SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}
  PREFIX libcomo-
)

qt6_generate_dbus_interface(dbus/device.h org.kde.kwin.InputDevice.xml OPTIONS -A)

add_custom_target(KWinInputDBusInterfaces ALL
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.InputDevice.xml
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.InputDevice.xml
  DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR}
)

install(TARGETS input input-x11-backend input-wl
  EXPORT como-export
  LIBRARY NAMELINK_SKIP
  FILE_SET HEADERS DESTINATION ${KDE_INSTALL_INCLUDEDIR}/como/input
  INCLUDES DESTINATION ${KDE_INSTALL_INCLUDEDIR}
)
