# SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

add_subdirectory(manual)
add_subdirectory(plasma)

Include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.3
)
FetchContent_MakeAvailable(Catch2)

remove_definitions(-DQT_USE_QSTRINGBUILDER)
add_subdirectory(integration)
