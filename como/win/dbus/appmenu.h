/*
SPDX-FileCopyrightText: 2011 Lionel Chauvin <megabigbug@yahoo.fr>
SPDX-FileCopyrightText: 2011, 2012 Cédric Bellegarde <gnumdk@gmail.com>
SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "como_export.h"

#include <como/win/appmenu.h>

#include <KDecoration2/DecorationSettings>
#include <QObject>
#include <functional>
#include <memory>

class QPoint;
class OrgKdeKappmenuInterface;
class QDBusObjectPath;
class QDBusServiceWatcher;

namespace como::win::dbus
{

struct appmenu_callbacks {
    std::function<void(appmenu_address const&, int action_id)> show_request;
    std::function<void(appmenu_address const&, bool)> visibility;
};

/**
 * Request showing the application menu bar.
 * @param actionId The DBus menu ID of the action that should be highlighted, 0 for the root menu.
 */
template<typename Win>
void show_appmenu(Win& win, int actionId)
{
    if (auto decoration = win.control->deco.decoration) {
        decoration->showApplicationMenu(actionId);
    } else if (win.control->has_application_menu()) {
        // No info where application menu button is, show it in the top left corner by default.
        win.space.appmenu->showApplicationMenu(win.geo.pos(), win.control->appmenu, actionId);
    }
}

template<typename Space>
appmenu_callbacks create_appmenu_callbacks(Space const& space)
{
    appmenu_callbacks callbacks;

    callbacks.show_request = [&space](appmenu_address const& addr, int action_id) {
        if (auto deco_settings = space.deco->settings()) {
            // Ignore request when user has not configured appmenu title bar button.
            auto menu_enum = KDecoration2::DecorationButtonType::ApplicationMenu;
            auto const& lbtn = deco_settings->decorationButtonsLeft();
            auto const& rbtn = deco_settings->decorationButtonsRight();
            if (!lbtn.contains(menu_enum) && !rbtn.contains(menu_enum)) {
                return;
            }
        }
        if (auto win = find_window_with_appmenu(space, addr)) {
            std::visit(overload{[&](auto&& win) { show_appmenu(*win, action_id); }}, *win);
        }
    };
    callbacks.visibility = [&space](appmenu_address const& addr, bool active) {
        if (auto win = find_window_with_appmenu(space, addr)) {
            std::visit(
                overload{[&](auto&& win) { win->control->set_application_menu_active(active); }},
                *win);
        }
    };

    return callbacks;
}

class COMO_EXPORT appmenu : public QObject
{
    Q_OBJECT

public:
    explicit appmenu(appmenu_callbacks callbacks);
    ~appmenu();

    void showApplicationMenu(const QPoint& pos, win::appmenu const& data, int actionId);
    bool applicationMenuEnabled() const;
    void setViewEnabled(bool enabled);

Q_SIGNALS:
    void applicationMenuEnabledChanged(bool enabled);

private Q_SLOTS:
    void slotShowRequest(const QString& serviceName,
                         const QDBusObjectPath& menuObjectPath,
                         int actionId);
    void slotMenuShown(const QString& serviceName, const QDBusObjectPath& menuObjectPath);
    void slotMenuHidden(const QString& serviceName, const QDBusObjectPath& menuObjectPath);

private:
    std::unique_ptr<OrgKdeKappmenuInterface> dbus_iface;
    std::unique_ptr<QDBusServiceWatcher> dbus_watcher;

    bool m_applicationMenuEnabled = false;
    appmenu_callbacks callbacks;
};

}
