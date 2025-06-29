# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(base SHARED)
target_link_libraries(base
  PUBLIC
    utils
    Qt::Core
    Qt::DBus
    Qt::Gui
    Qt::GuiPrivate
    Qt::Widgets
    KF6::ConfigCore
)

target_sources(base
  PUBLIC
    FILE_SET HEADERS
    FILES
      os/clock/skew_notifier_engine.h
      os/clock/skew_notifier.h
      seat/backend/logind/session.h
      seat/session.h
      app_singleton.h
      config.h
      logging.h
      options.h
      output.h
      output_helpers.h
      output_topology.h
      platform_helpers.h
      platform_qobject.h
      singleton_interface.h
      types.h
      utils.h
  PRIVATE
    os/clock/skew_notifier.cpp
    os/clock/skew_notifier_engine.cpp
    seat/session.cpp
    seat/backend/logind/session.cpp
    singleton_interface.cpp
    logging.cpp
    options.cpp
)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_sources(base
    PUBLIC
      FILE_SET HEADERS
      FILES
        os/clock/linux_skew_notifier_engine.h
    PRIVATE
      os/clock/linux_skew_notifier_engine.cpp
  )
endif()

configure_file(config-como.h.cmake config-como.h)
generate_export_header(base BASE_NAME como)

add_library(base-x11 SHARED)

target_link_libraries(base-x11
  PUBLIC
    base
    XCB::COMPOSITE
    XCB::DAMAGE
    XCB::GLX
    XCB::RANDR
    XCB::SHAPE
    XCB::SYNC
    XCB::XKB
)

target_sources(base-x11
  PUBLIC
    FILE_SET HEADERS
    # TODO(romangg): Should add BASE_DIRS but conflicts with xcb files shadowing system includes.
    FILES
      x11/atoms.h
      x11/data.h
      x11/event_filter.h
      x11/event_filter_container.h
      x11/event_filter_manager.h
      x11/fixx11h.h
      x11/grabs.h
      x11/selection_owner.h
      x11/user_interaction_filter.h
      x11/xcb/atom.h
      x11/xcb/extensions.h
      x11/xcb/geometry_hints.h
      x11/xcb/helpers.h
      x11/xcb/motif_hints.h
      x11/xcb/property.h
      x11/xcb/proto.h
      x11/xcb/qt_types.h
      x11/xcb/randr.h
      x11/xcb/window.h
      x11/xcb/wrapper.h
  PRIVATE
    x11/event_filter.cpp
    x11/event_filter_container.cpp
    x11/event_filter_manager.cpp
    x11/grabs.cpp
    x11/selection_owner.cpp
    x11/xcb/extensions.cpp
)

add_library(base-x11-backend SHARED)
target_link_libraries(base-x11-backend
  PUBLIC
    base-x11
)

target_sources(base-x11-backend
  PUBLIC
    FILE_SET HEADERS
    FILES
      backend/x11/wm_selection.h
      backend/x11/wm_selection_owner.h
      x11/app_singleton.h
      x11/output.h
      x11/output_helpers.h
      x11/platform.h
      x11/platform_helpers.h
      x11/randr_filter.h
  PRIVATE
    backend/x11/wm_selection_owner.cpp
)

add_library(base-wl SHARED)

target_link_libraries(base-wl
  PUBLIC
    KF6::Service
    WraplandServer
    base
)

target_sources(base-wl
  PUBLIC
    FILE_SET HEADERS
    FILES
      wayland/app_singleton.h
      wayland/filtered_display.h
      wayland/output.h
      wayland/output_helpers.h
      wayland/output_transform.h
      wayland/platform.h
      wayland/platform_helpers.h
      wayland/server.h
      wayland/server_helpers.h
      wayland/xwl_platform.h
  PRIVATE
    wayland/filtered_display.cpp
)

set_target_properties(base base-x11 base-wl PROPERTIES
   VERSION ${CMAKE_PROJECT_VERSION}
   SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}
   PREFIX libcomo-
)

install(TARGETS base base-x11 base-x11-backend base-wl
  EXPORT como-export
  LIBRARY NAMELINK_SKIP
  FILE_SET HEADERS DESTINATION ${KDE_INSTALL_INCLUDEDIR}/como/base
  INCLUDES DESTINATION ${KDE_INSTALL_INCLUDEDIR}
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/como_export.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/como/base
  COMPONENT Devel
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/config-como.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/como/base
  COMPONENT Devel
)
