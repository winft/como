# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(xwayland SHARED)
add_library(como::xwayland ALIAS xwayland)

target_link_libraries(xwayland
  PUBLIC
    base-x11
    render-x11
    win-x11
    WraplandServer
    XCB::CURSOR
)

target_sources(xwayland
  PUBLIC
    FILE_SET HEADERS
    FILES
      clipboard.h
      data_bridge.h
      dnd.h
      drag.h
      drag_wl.h
      drag_x.h
      event_x11.h
      mime.h
      primary_selection.h
      selection_data.h
      selection_wl.h
      selection_x11.h
      socket.h
      sources.h
      sources_ext.h
      surface.h
      transfer.h
      transfer_timeout.h
      types.h
      wl_visit.h
      x11_visit.h
      xwayland.h
      xauthority.h
  PRIVATE
    socket.cpp
    sources_ext.cpp
    transfer.cpp
    xauthority.cpp
)

set_target_properties(xwayland PROPERTIES
  VERSION ${CMAKE_PROJECT_VERSION}
  SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}
  PREFIX libcomo-
)

install(TARGETS xwayland
  EXPORT como-export
  LIBRARY NAMELINK_SKIP
  FILE_SET HEADERS DESTINATION ${KDE_INSTALL_INCLUDEDIR}/como/xwl
  INCLUDES DESTINATION ${KDE_INSTALL_INCLUDEDIR}
)
