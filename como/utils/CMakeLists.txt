# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(utils INTERFACE)

target_sources(utils
  PUBLIC
    FILE_SET HEADERS
    FILES
      algorithm.h
      blocker.h
      file_descriptor.h
      flags.h
      gamma_ramp.h
      geo.h
      memory.h
)

set_target_properties(utils PROPERTIES
  VERSION ${CMAKE_PROJECT_VERSION}
  SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}
  PREFIX libcomo-
)

install(TARGETS utils
  EXPORT como-export
  LIBRARY NAMELINK_SKIP
  FILE_SET HEADERS DESTINATION ${KDE_INSTALL_INCLUDEDIR}/como/utils
  INCLUDES DESTINATION ${KDE_INSTALL_INCLUDEDIR}
)
