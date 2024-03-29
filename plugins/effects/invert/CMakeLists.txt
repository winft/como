# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

#######################################
# Effect

set(invert_SOURCES
    invert.cpp
    invert.qrc
    main.cpp
)

kwin_add_builtin_effect(invert ${invert_SOURCES})
target_link_libraries(invert PRIVATE
  KF6::I18n
  Qt::Widgets
  render
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_invert_config_SRCS invert_config.cpp)

    kwin_add_effect_config(kwin_invert_config ${kwin_invert_config_SRCS})

    target_link_libraries(kwin_invert_config
      KF6::CoreAddons
      KF6::GlobalAccel
      KF6::XmlGui
    )
endif()
