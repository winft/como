# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_subdirectory(utils)
add_subdirectory(base)
add_subdirectory(win)
add_subdirectory(input)
add_subdirectory(render)
add_subdirectory(data)
add_subdirectory(script)
add_subdirectory(xwl)
add_subdirectory(debug)
add_subdirectory(desktop)

add_library(x11 SHARED)
add_library(como::x11 ALIAS x11)

target_link_libraries(x11
  PUBLIC
    base-x11-backend
    debug-common
    input-x11-backend
    render-x11-backend
    win-x11-backend
)

target_sources(x11
  PUBLIC
    FILE_SET HEADERS
    FILES
      base/x11/platform.h
)

add_library(wayland SHARED)
add_library(como::wayland ALIAS wayland)

target_link_libraries(wayland
  PUBLIC
    debug-wl
    input-wl
    render-wl
    win-wl
    Libinput::Libinput
    Pixman::Pixman
    wlroots::wlroots
    Wayland::Server
)

target_sources(wayland
  PUBLIC
    FILE_SET HEADERS
    FILES
      base/backend/wlroots/backend.h
      base/backend/wlroots/drm_lease.h
      base/backend/wlroots/events.h
      base/backend/wlroots/helpers.h
      base/backend/wlroots/non_desktop_output.h
      base/backend/wlroots/output.h
      base/seat/backend/wlroots/session.h
      input/backend/wlroots/backend.h
      input/backend/wlroots/control/control.h
      input/backend/wlroots/control/headless/keyboard.h
      input/backend/wlroots/control/keyboard.h
      input/backend/wlroots/control/pointer.h
      input/backend/wlroots/control/switch.h
      input/backend/wlroots/control/touch.h
      input/backend/wlroots/device_helpers.h
      input/backend/wlroots/keyboard.h
      input/backend/wlroots/pointer.h
      input/backend/wlroots/switch.h
      input/backend/wlroots/touch.h
      render/backend/wlroots/backend.h
      render/backend/wlroots/egl_backend.h
      render/backend/wlroots/egl_helpers.h
      render/backend/wlroots/egl_output.h
      render/backend/wlroots/egl_texture.h
      render/backend/wlroots/output.h
      render/backend/wlroots/output_event.h
      render/backend/wlroots/qpainter_backend.h
      render/backend/wlroots/qpainter_output.h
      render/backend/wlroots/texture_update.h
      render/backend/wlroots/wlr_helpers.h
      render/backend/wlroots/wlr_includes.h
      render/backend/wlroots/wlr_non_owning_data_buffer.h
  PRIVATE
   base/backend/wlroots/drm_lease.cpp
   base/seat/backend/wlroots/session.cpp
   input/backend/wlroots/control/headless/keyboard.cpp
   input/backend/wlroots/control/keyboard.cpp
   input/backend/wlroots/control/pointer.cpp
   input/backend/wlroots/control/switch.cpp
   input/backend/wlroots/control/touch.cpp
)

set_target_properties(x11 wayland PROPERTIES
   VERSION ${CMAKE_PROJECT_VERSION}
   SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}
   PREFIX libcomo-
)

target_compile_definitions(wayland PUBLIC WLR_USE_UNSTABLE)

kcoreaddons_target_static_plugins(wayland
  NAMESPACE "como/effects/plugins"
)

install(TARGETS x11 wayland
  EXPORT como-export
  LIBRARY NAMELINK_SKIP
  FILE_SET HEADERS DESTINATION ${KDE_INSTALL_INCLUDEDIR}/como
  INCLUDES DESTINATION ${KDE_INSTALL_INCLUDEDIR}
)
