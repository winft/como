
# SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

################# RPATH handling ##################################

# Set the default RPATH to point to useful locations, namely where the
# libraries will be installed and the linker search path.

# KDE_INSTALL_LIBDIR comes from previous KDEInstallDirs include.
if(NOT KDE_INSTALL_LIBDIR)
   message(FATAL_ERROR "KDE_INSTALL_LIBDIR is not set. Setting one is necessary for using the RPATH settings.")
endif()

set(_abs_LIB_INSTALL_DIR "${KDE_INSTALL_LIBDIR}")

if (NOT IS_ABSOLUTE "${_abs_LIB_INSTALL_DIR}")
   set(_abs_LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${_abs_LIB_INSTALL_DIR}")
endif()

# add our LIB_INSTALL_DIR to the RPATH (but only when it is not one of
# the standard system link directories - such as /usr/lib)
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${_abs_LIB_INSTALL_DIR}" _isSystemLibDir)
list(FIND CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES      "${_abs_LIB_INSTALL_DIR}" _isSystemCxxLibDir)
list(FIND CMAKE_C_IMPLICIT_LINK_DIRECTORIES        "${_abs_LIB_INSTALL_DIR}" _isSystemCLibDir)
if("${_isSystemLibDir}" STREQUAL "-1"  AND  "${_isSystemCxxLibDir}" STREQUAL "-1"  AND  "${_isSystemCLibDir}" STREQUAL "-1")
   set(CMAKE_INSTALL_RPATH "${_abs_LIB_INSTALL_DIR}")
endif()

# Append directories in the linker search path (but outside the project)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

################ Build settings ###########################

# Always include srcdir and builddir in include path
# This saves typing ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} in about every subdir
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# put the include dirs which are in the source or build tree before all other include dirs, so the
# headers in the sources are preferred over the already installed ones
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

# Add the src and build dir to the BUILD_INTERFACE include directories of all targets. Similar to
# CMAKE_INCLUDE_CURRENT_DIR, but transitive.
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

# When a shared library changes, but its includes do not, don't relink all dependencies. It is not
# needed.
set(CMAKE_LINK_DEPENDS_NO_SHARED ON)

# Default to shared libs, if no type is explicitly given to add_library().
set(BUILD_SHARED_LIBS TRUE CACHE BOOL "If enabled, shared libs will be built by default, otherwise static libs")

set(CMAKE_AUTOMOC ON)

# Enable autorcc and in cmake so qrc files get generated..
set(CMAKE_AUTORCC ON)

# By default, don't put a prefix on MODULE targets. add_library(MODULE) is basically for plugin
# targets, and in KDE plugins don't have a prefix.
set(CMAKE_SHARED_MODULE_PREFIX "")

# All executables are built into a toplevel "bin" dir, making it possible to find helper binaries,
# and to find uninstalled plugins (provided that you use kcoreaddons_add_plugin() or set
# LIBRARY_OUTPUT_DIRECTORY as documented on [1].
# [1] https://community.kde.org/Guidelines_and_HOWTOs/Making_apps_run_uninstalled)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
