# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(QPA_SOURCES
    abstractplatformcontext.cpp
    backingstore.cpp
    eglhelpers.cpp
    integration.cpp
    main.cpp
    offscreensurface.cpp
    platformcursor.cpp
    screen.cpp
    sharingplatformcontext.cpp
    window.cpp
)

include(ECMQtDeclareLoggingCategory)
ecm_qt_declare_logging_category(QPA_SOURCES HEADER logging.h IDENTIFIER KWIN_QPA CATEGORY_NAME kwin_qpa_plugin DEFAULT_SEVERITY Critical)

add_library(ComoQpaPlugin MODULE ${QPA_SOURCES})
set_target_properties(ComoQpaPlugin PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/platforms"
)

target_link_libraries(ComoQpaPlugin PRIVATE
    epoxy::epoxy
    Qt::CorePrivate
    Qt::GuiPrivate
    Freetype::Freetype # Must be after Qt platform support libs
    Fontconfig::Fontconfig
    WraplandServer
    input
    render
    win
)

install(
  TARGETS ComoQpaPlugin
  DESTINATION ${KDE_INSTALL_PLUGINDIR}/platforms
)
