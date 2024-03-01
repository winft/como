# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

ecm_install_icons(
  ICONS
    icons/16-apps-kwin.png
    icons/32-apps-kwin.png
    icons/48-apps-kwin.png
    icons/sc-apps-kwin.svgz
  DESTINATION ${KDE_INSTALL_ICONDIR}
  THEME hicolor
)

########### next target ###############
add_executable(kwin5_update_default_rules update_default_rules.cpp)
target_link_libraries(kwin5_update_default_rules
  KF6::ConfigCore
  Qt::Core
  Qt::DBus
)
install(TARGETS kwin5_update_default_rules DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin/)

########### install files ###############

install(FILES org_kde_kwin.categories DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR})
