# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_subdirectory(tabbox/switchers)
add_library(win SHARED)

target_link_libraries(win
  PUBLIC
    KDecoration2::KDecoration
    KDecoration2::KDecoration2Private
    KF6::ColorScheme
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::CoreAddons
    KF6::I18n
    KF6::Package
    Qt::Concurrent
    Qt::OpenGL
    Qt::Quick
    base
)

target_sources(win
  PUBLIC
    FILE_SET HEADERS
    FILES
      dbus/appmenu.h
      dbus/session_manager.h
      dbus/virtual_desktop_manager.h
      dbus/virtual_desktop_types.h
      deco/bridge.h
      deco/bridge_qobject.h
      deco/client_impl.h
      deco/client_impl_qobject.h
      deco/decorations_logging.h
      deco/palette.h
      deco/renderer.h
      deco/settings.h
      deco/window.h
      input/gestures.h
      input/global_shortcut.h
      rules/book.h
      rules/book_edit.h
      rules/book_settings.h
      rules/find.h
      rules/ruling.h
      rules/types.h
      rules/update.h
      rules/window.h
      actions.h
      activation.h
      active_window.h
      appmenu.h
      control.h
      controlling.h
      cursor_shape.h
      damage.h
      deco.h
      deco_input.h
      desktop_get.h
      desktop_set.h
      desktop_space.h
      focus_blocker.h
      focus_chain.h
      focus_chain_edit.h
      focus_chain_find.h
      fullscreen.h
      geo.h
      geo_block.h
      geo_change.h
      geo_electric.h
      geo_move.h
      geo_restrict.h
      input.h
      kill_window.h
      layers.h
      maximize.h
      meta.h
      move.h
      net.h
      options.h
      osd.h
      osd_notification.h
      output_space.h
      placement.h
      property_window.h
      quicktile.h
      remnant.h
      rules.h
      scene.h
      screen.h
      screen_edges.h
      setup.h
      shortcuts_init.h
      shortcut_dialog.h
      shortcut_set.h
      singleton_interface.h
      space_areas.h
      space_areas_helpers.h
      space_qobject.h
      space_reconfigure.h
      space_setup.h
      space_window_release.h
      stacking.h
      stacking_order.h
      stacking_state.h
      structs.h
      strut_rect.h
      tabbox.h
      transient.h
      transient_stacking.h
      types.h
      user_actions_menu.h
      util.h
      subspace.h
      subspace_grid.h
      subspace_manager.h
      subspace_manager_qobject.h
      subspaces_get.h
      subspaces_set.h
      window_area.h
      window_find.h
      window_geometry.h
      window_metadata.h
      window_operation.h
      window_qobject.h
      window_release.h
      window_render_data.h
      window_setup_base.h
      window_topology.h
  PRIVATE
    cursor_shape.cpp
    dbus/appmenu.cpp
    dbus/session_manager.cpp
    dbus/virtual_desktop_manager.cpp
    dbus/virtual_desktop_types.cpp
    deco/palette.cpp
    deco/decorations_logging.cpp
    input/gestures.cpp
    input/global_shortcut.cpp
    options.cpp
    osd_notification.cpp
    property_window.cpp
    rules/book.cpp
    rules/book_settings.cpp
    rules/ruling.cpp
    rules/window.cpp
    screen_edges.cpp
    shortcut_dialog.cpp
    singleton_interface.cpp
    space_qobject.cpp
    subspace.cpp
    subspace_grid.cpp
)

if (COMO_BUILD_TABBOX)
  target_sources(win
    PUBLIC
      FILE_SET HEADERS
      FILES
        tabbox/tabbox.h
        tabbox/tabbox_client.h
        tabbox/tabbox_client_impl.h
        tabbox/tabbox_client_model.h
        tabbox/tabbox_config.h
        tabbox/tabbox_handler.h
        tabbox/tabbox_handler_impl.h
        tabbox/tabbox_logging.h
        tabbox/tabbox_switcher_item.h
    PRIVATE
      tabbox/tabbox_client_model.cpp
      tabbox/tabbox_config.cpp
      tabbox/tabbox_handler.cpp
      tabbox/tabbox_logging.cpp
      tabbox/tabbox_switcher_item.cpp
  )
  target_link_libraries(win PRIVATE Qt::GuiPrivate)
endif()

qt6_add_dbus_adaptor(win_dbus_src
  dbus/org.kde.KWin.VirtualDesktopManager.xml
  dbus/virtual_desktop_manager.h
  como::win::dbus::subspace_manager_wrap
)
qt6_add_dbus_interface(win_dbus_src dbus/org.kde.kappmenu.xml appmenu_interface)
qt6_add_dbus_interface(win_dbus_src
  ${KSCREENLOCKER_DBUS_INTERFACES_DIR}/kf6_org.freedesktop.ScreenSaver.xml
  screenlocker_interface
)
qt6_add_dbus_adaptor(win_dbus_src
  dbus/org.kde.KWin.Session.xml
  dbus/session_manager.h
  como::win::dbus::session_manager
)

target_sources(win PRIVATE ${win_dbus_src})

kconfig_add_kcfg_files(win config/win_settings.kcfgc)
kconfig_add_kcfg_files(win rules/kconfig/rules_settings.kcfgc)
kconfig_add_kcfg_files(win rules/kconfig/rules_book_settings_base.kcfgc)

ki18n_wrap_ui(win shortcut_dialog.ui)

# Target killer helper
add_executable(como_killer_helper x11/helpers/killer.cpp)

target_link_libraries(como_killer_helper
  KF6::AuthCore
  KF6::I18n
  KF6::WidgetsAddons
  Qt::GuiPrivate
  Qt::Widgets
)

add_library(win-x11 SHARED)

target_link_libraries(win-x11
  PUBLIC
    win
    XCB::ICCCM
    XCB::KEYSYMS
    ${X11_LIBRARIES}
)

target_sources(win-x11
  PUBLIC
    FILE_SET HEADERS
    FILES
      x11/client_machine.h
      x11/extras.h
      x11/key_server.h
      x11/net/root_info.h
      x11/net/win_info.h
      x11/net/atoms_p.h
      x11/net/geo.h
      x11/net/info_p.h
      x11/net/net.h
      x11/net/rarray.h
      x11/actions.h
      x11/activation.h
      x11/appmenu.h
      x11/client.h
      x11/command.h
      x11/control.h
      x11/control_create.h
      x11/damage.h
      x11/debug.h
      x11/deco.h
      x11/desktop_space.h
      x11/event.h
      x11/focus.h
      x11/focus_stealing.h
      x11/fullscreen.h
      x11/geo.h
      x11/group.h
      x11/hidden_preview.h
      x11/hide.h
      x11/input.h
      x11/maximize.h
      x11/meta.h
      x11/move.h
      x11/moving_window_filter.h
      x11/netinfo.h
      x11/netinfo_helpers.h
      x11/placement.h
      x11/root_info_filter.h
      x11/scene.h
      x11/screen_edge.h
      x11/screen_edges.h
      x11/screen_edges_filter.h
      x11/session.h
      x11/session_manager.h
      x11/shortcut.h
      x11/space_areas.h
      x11/space_event.h
      x11/space_setup.h
      x11/stacking.h
      x11/subspace_manager.h
      x11/sync.h
      x11/sync_alarm_filter.h
      x11/tabbox_filter.h
      x11/tool_windows.h
      x11/transient.h
      x11/types.h
      x11/unmanaged.h
      x11/user_time.h
      x11/window.h
      x11/window_create.h
      x11/window_find.h
      x11/window_release.h
      x11/win_info.h
      x11/xcb.h
      x11/xcb_cursor.h
      x11/xcb_event_filter.h
      x11/xcb_windows.h
  PRIVATE
    x11/client_machine.cpp
    x11/extras.cpp
    x11/key_server.cpp
    x11/session_manager.cpp
    x11/net/atoms.cpp
    x11/net/root_info.cpp
    x11/net/win_info.cpp
)

add_library(win-x11-backend SHARED)

target_link_libraries(win-x11-backend
  PUBLIC
    win-x11
)

target_sources(win-x11-backend
  PUBLIC
    FILE_SET HEADERS
    FILES
      x11/space.h
)

add_library(win-wl SHARED)

target_link_libraries(win-wl
  PUBLIC
    win
    WraplandServer
)

target_sources(win-wl
  PUBLIC
    FILE_SET HEADERS
    FILES
      wayland/appmenu.h
      wayland/control.h
      wayland/control_destroy.h
      wayland/deco.h
      wayland/desktop_set.h
      wayland/idle.h
      wayland/input.h
      wayland/internal_window.h
      wayland/layer_shell.h
      wayland/plasma_shell.h
      wayland/plasma_window.h
      wayland/popup_placement.h
      wayland/scene.h
      wayland/screen_lock.h
      wayland/session_manager.h
      wayland/setup.h
      wayland/space.h
      wayland/space_areas.h
      wayland/space_setup.h
      wayland/space_windows.h
      wayland/subspace_manager.h
      wayland/subsurface.h
      wayland/surface.h
      wayland/transient.h
      wayland/window.h
      wayland/window_release.h
      wayland/xdg_activation.h
      wayland/xdg_shell.h
      wayland/xdg_shell_control.h
      wayland/xwl_control.h
      wayland/xwl_space.h
      wayland/xwl_window.h
)

set_target_properties(win win-x11 win-x11-backend win-wl PROPERTIES
   VERSION ${CMAKE_PROJECT_VERSION}
   SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}
   PREFIX libcomo-
)

install(
  FILES
    rules/kconfig/rules_settings.kcfg
    rules/kconfig/rules_book_settings_base.kcfg
  DESTINATION ${KDE_INSTALL_KCFGDIR}
)
install(
  FILES dbus/org.kde.KWin.VirtualDesktopManager.xml
  DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR}
)
install(
  DIRECTORY qml/onscreennotification/plasma
  DESTINATION ${KDE_INSTALL_DATADIR}/como/onscreennotification
)

install(TARGETS win win-x11 win-x11-backend win-wl
  EXPORT como-export
  LIBRARY NAMELINK_SKIP
  FILE_SET HEADERS DESTINATION ${KDE_INSTALL_INCLUDEDIR}/como/win
  INCLUDES DESTINATION ${KDE_INSTALL_INCLUDEDIR}
)

install(TARGETS como_killer_helper DESTINATION ${KDE_INSTALL_LIBEXECDIR})

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/rules_book_settings_base.h
    ${CMAKE_CURRENT_BINARY_DIR}/rules_settings.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/como/win/rules
  COMPONENT Devel
)
