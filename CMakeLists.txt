# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.23 FATAL_ERROR)

project("The Compositor Modules" VERSION 0.1.0)

set(QT_MIN_VERSION "6.6.0")
set(KF6_MIN_VERSION "6.0.0")
set(KDE_PLASMA_VERSION "6.0.0")

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(
  -DQT_NO_URL_CAST_FROM_STRING
  -DQT_USE_QSTRINGBUILDER
  -DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT
  -DQT_NO_KEYWORDS
  -DQT_NO_FOREACH
  -DQT_STRICT_ITERATORS
  -DQT_NO_CAST_TO_ASCII
)

# Prevent EGL headers from including platform headers, in particular Xlib.h.
add_definitions(-DMESA_EGL_NO_X11_HEADERS)
add_definitions(-DEGL_NO_X11)
add_definitions(-DEGL_NO_PLATFORM_SPECIFIC_TYPES)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${ECM_MODULE_PATH})

include(FeatureSummary)
include(WriteBasicConfigVersionFile)
include(GenerateExportHeader)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Concurrent
    Core
    Core5Compat
    DBus
    Quick
    UiTools
    Widgets
)

find_package(Qt6Test ${QT_MIN_VERSION} CONFIG QUIET)
set_package_properties(Qt6Test PROPERTIES
    PURPOSE "Required for tests"
    TYPE OPTIONAL
)
add_feature_info("Qt6Test" Qt6Test_FOUND "Required for building tests")

include(KDEInstallDirs)
include(CMakeSettings)
include(CTest)

include(ECMInstallIcons)
include(ECMOptionalAddSubdirectory)
include(ECMConfiguredInstall)
include(ECMQmlModule)
include(ECMGenerateQmlTypes)
include(ECMSetupVersion)
include(ECMEnableSanitizers)

ecm_setup_version(PROJECT
  VARIABLE_PREFIX COMO
  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/como_version.h"
  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/como-config-version.cmake"
  SOVERSION 0
)

# required frameworks by Core
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    Auth
    Config
    ConfigWidgets
    CoreAddons
    Crash
    GlobalAccel
    I18n
    IdleTime
    Notifications
    Package
    Svg
    WidgetsAddons
    WindowSystem
)
# required frameworks by config modules
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    Declarative
    KCMUtils
    NewStuff
    Service
    XmlGui
)

# Required frameworks by Wayland binary
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    DBusAddons
)

# optional frameworks
find_package(KF6DocTools ${KF6_MIN_VERSION} CONFIG)
set_package_properties(KF6DocTools PROPERTIES
    PURPOSE "Enable building documentation"
    TYPE OPTIONAL
)
add_feature_info("KF6DocTools" KF6DocTools_FOUND "Enable building documentation")

find_package(KF6Kirigami ${KF6_MIN_VERSION} CONFIG)
set_package_properties(KF6Kirigami PROPERTIES
    DESCRIPTION "A QtQuick based components set"
    PURPOSE "Required at runtime for several QML effects"
    TYPE RUNTIME
)

find_package(Plasma ${KDE_PLASMA_VERSION}
  CONFIG
)
set_package_properties(Plasma PROPERTIES
    DESCRIPTION "A QtQuick based components set"
    PURPOSE "Required at runtime for several QML effects"
    TYPE RUNTIME
)

find_package(KDecoration2 ${KDE_PLASMA_VERSION}
  CONFIG REQUIRED
)

find_package(KScreenLocker CONFIG REQUIRED)
set_package_properties(KScreenLocker PROPERTIES
    TYPE REQUIRED
    PURPOSE "For screenlocker integration in the Wayland session"
)

find_package(Breeze 6.0.0 CONFIG)
set_package_properties(Breeze PROPERTIES
    TYPE OPTIONAL
    PURPOSE "For setting the default window decoration plugin"
)
if (${Breeze_FOUND})
    if (${BREEZE_WITH_KDECORATION})
        set(HAVE_BREEZE_DECO true)
    else()
        set(HAVE_BREEZE_DECO FALSE)
    endif()
else()
    set(HAVE_BREEZE_DECO FALSE)
endif()
add_feature_info("Breeze-Decoration" HAVE_BREEZE_DECO "Default decoration plugin Breeze")

find_package(EGL)
set_package_properties(EGL PROPERTIES
    TYPE RUNTIME
    PURPOSE "Required to build with EGL support"
)

find_package(epoxy 1.3)
set_package_properties(epoxy PROPERTIES
    DESCRIPTION "libepoxy"
    URL "https://github.com/anholt/libepoxy"
    TYPE REQUIRED
    PURPOSE "OpenGL dispatch library"
)

set(HAVE_DL_LIBRARY FALSE)
if (epoxy_HAS_GLX)
    find_library(DL_LIBRARY dl)
    if (DL_LIBRARY)
        set(HAVE_DL_LIBRARY TRUE)
    endif()
endif()

find_package(Wayland 1.2 REQUIRED COMPONENTS Cursor Server OPTIONAL_COMPONENTS Egl)
set_package_properties(Wayland PROPERTIES
    TYPE REQUIRED
    PURPOSE "Required for building with Wayland support"
)

find_package(XKB 0.7.0)
set_package_properties(XKB PROPERTIES
    TYPE REQUIRED
    PURPOSE "Required for building with Wayland support"
)

# TODO(romangg): Only required for X11 session.
pkg_check_modules(XKBX11 IMPORTED_TARGET xkbcommon-x11 REQUIRED)
add_feature_info(XKBX11 XKBX11_FOUND "Required for handling keyboard events in X11")

find_package(Threads)
set_package_properties(Threads PROPERTIES
    TYPE REQUIRED
    PURPOSE "Required for building with Wayland support"
)

find_package(Libinput 1.9)
set_package_properties(Libinput PROPERTIES TYPE REQUIRED PURPOSE "Required for input handling on Wayland.")

find_package(Pixman)
set_package_properties(Pixman PROPERTIES
    TYPE REQUIRED
    PURPOSE "Required for building wlroots backend."
)

set(WLROOTS_MIN_VERSION "0.17.0")
find_package(wlroots ${WLROOTS_MIN_VERSION})
set_package_properties(wlroots PROPERTIES
    TYPE REQUIRED
    PURPOSE "Required for building wlroots backend."
)

if (${wlroots_VERSION} VERSION_GREATER_EQUAL 0.18)
  set(WLR_HAVE_SESSION_CREATE_LOOP 1)
  set(WLR_HAVE_UTIL_TRANSFORM_HEADER 1)
  set(WLR_HAVE_NEW_PIXEL_COPY_API 1)
  set(WLR_HAVE_BACKEND_CREATE_WITH_LOOP 1)
  set(WLR_HAVE_WL_POINTER_AXIS_SOURCE 1)
endif()

find_package(X11)
set_package_properties(X11 PROPERTIES
    DESCRIPTION "X11 libraries"
    URL "https://www.x.org"
    TYPE REQUIRED
)

# All the required XCB components
find_package(XCB 1.10 REQUIRED COMPONENTS
    COMPOSITE
    CURSOR
    DAMAGE
    EVENT
    GLX
    ICCCM
    IMAGE
    KEYSYMS
    RANDR
    RENDER
    SHAPE
    SHM
    SYNC
    XFIXES
    XKB
)
set_package_properties(XCB PROPERTIES TYPE REQUIRED)

find_package(Freetype REQUIRED)
set_package_properties(Freetype PROPERTIES
    DESCRIPTION "A font rendering engine"
    URL "https://www.freetype.org"
    TYPE REQUIRED
    PURPOSE "Needed for own QPA plugin."
)
find_package(Fontconfig REQUIRED)
set_package_properties(Fontconfig PROPERTIES
    TYPE REQUIRED
    PURPOSE "Needed for own QPA plugin."
)

find_package(Wrapland 0.600.0 REQUIRED)
set_package_properties(Wrapland PROPERTIES
    TYPE REQUIRED
    PURPOSE "Used as Wrapper library for Wayland protocol objects."
)

find_package(Xwayland)
set_package_properties(Xwayland PROPERTIES
    URL "https://x.org"
    DESCRIPTION "Xwayland X server"
    TYPE RUNTIME
    PURPOSE "Needed for running Wayland"
)

find_package(Libcap)
set_package_properties(Libcap PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Needed for running Wayland session with real-time scheduling policy"
)
set(HAVE_LIBCAP ${Libcap_FOUND})

find_package(QAccessibilityClient6 CONFIG)
set_package_properties(QAccessibilityClient6 PROPERTIES
    URL "https://www.kde.org"
    DESCRIPTION "KDE client-side accessibility library"
    TYPE OPTIONAL
    PURPOSE "Required to enable accessibility features"
)
set(HAVE_ACCESSIBILITY ${QAccessibilityClient6_FOUND})

include(ECMFindQmlModule)
ecm_find_qmlmodule(QtQuick 2.3)
ecm_find_qmlmodule(QtQuick.Controls 1.2)
ecm_find_qmlmodule(QtQuick.Layouts 1.3)
ecm_find_qmlmodule(QtQuick.Window 2.1)
ecm_find_qmlmodule(QtMultimedia 5.0)
ecm_find_qmlmodule(org.kde.kquickcontrolsaddons 2.0)
ecm_find_qmlmodule(org.kde.plasma.core 2.0)
ecm_find_qmlmodule(org.kde.plasma.components 2.0)

option(COMO_BUILD_DECORATIONS "Enable building of decorations." ON)
option(COMO_BUILD_KCMS "Enable building of configuration modules." ON)
option(COMO_BUILD_TABBOX "Enable building of Tabbox functionality" ON)
option(COMO_BUILD_PERF "Build internal tools for performance analysis at runtime." ON)

# Default to hidden visibility for symbols
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

set(COMO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(HAVE_PERF ${COMO_BUILD_PERF})

include_directories(${XKB_INCLUDE_DIR})

set(HAVE_EPOXY_GLX ${epoxy_HAS_GLX})

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckSymbolExists)

check_symbol_exists(SCHED_RESET_ON_FORK "sched.h" HAVE_SCHED_RESET_ON_FORK)
add_feature_info("SCHED_RESET_ON_FORK"
                 HAVE_SCHED_RESET_ON_FORK
                 "Required for running Wayland session with real-time scheduling")

########### global ###############
include_directories(BEFORE
    ${CMAKE_CURRENT_BINARY_DIR}/como
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(como)
add_subdirectory(plugins)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_subdirectory(examples)

if (KF6DocTools_FOUND)
    add_subdirectory(docs)
endif()

# create a Config.cmake and a ConfigVersion.cmake file and install them
set(CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/como")

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/como-config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/como-config.cmake" INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/como-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/como-config-version.cmake"
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  COMPONENT Devel
)

install(
  EXPORT como-export
  FILE como-targets.cmake
  NAMESPACE como::
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/como_version.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT Devel
)

install(
  FILES
    cmake/modules/FindPixman.cmake
    cmake/modules/Findwlroots.cmake
  DESTINATION ${CMAKECONFIG_INSTALL_DIR}/modules
)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

include(Packing)
