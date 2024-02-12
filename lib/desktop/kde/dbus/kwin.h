/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "como_export.h"
#include "debug/support_info.h"
#include "win/kill_window.h"
#include "win/placement.h"
#include <win/activation.h>
#include <win/space_qobject.h>
#include <win/subspaces_set.h>

#include <QObject>
#include <QtDBus>

namespace KWin
{

namespace win
{
class space_qobject;
}

namespace desktop::kde
{

/**
 * @brief This class is a wrapper for the org.kde.KWin D-Bus interface.
 *
 * The main purpose of this class is to be exported on the D-Bus as object /KWin.
 * It is a pure wrapper to provide the deprecated D-Bus methods which have been
 * removed from Workspace which used to implement the complete D-Bus interface.
 *
 * Nowadays the D-Bus interfaces are distributed, parts of it are exported on
 * /Compositor, parts on /Effects and parts on /KWin. The implementation in this
 * class just delegates the method calls to the actual implementation in one of the
 * three singletons.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 */
class COMO_EXPORT kwin : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin")
    Q_PROPERTY(bool showingDesktop READ showingDesktop NOTIFY showingDesktopChanged)

public:
    explicit kwin(win::space_qobject& space);
    ~kwin() override;

    bool showingDesktop() const
    {
        return showing_desktop_impl();
    }

public Q_SLOTS:
    int currentDesktop()
    {
        return current_desktop_impl();
    }
    Q_NOREPLY void showDesktop(bool show);
    Q_NOREPLY void killWindow()
    {
        kill_window_impl();
    }
    void nextDesktop()
    {
        next_desktop_impl();
    }
    void previousDesktop()
    {
        previous_desktop_impl();
    }

    Q_NOREPLY void reconfigure();

    bool setCurrentDesktop(int desktop)
    {
        return set_current_desktop_impl(desktop);
    }

    bool startActivity(const QString& in0);
    bool stopActivity(const QString& in0);

    QString supportInformation()
    {
        return support_information_impl();
    }
    QString activeOutputName()
    {
        return active_output_name_impl();
    }
    Q_NOREPLY void unclutterDesktop()
    {
        unclutter_desktop_impl();
    }

    Q_NOREPLY void showDebugConsole()
    {
        show_debug_console_impl();
    }

    void enableFtrace(bool enable);

    QVariantMap queryWindowInfo()
    {
        return query_window_info_impl();
    }
    QVariantMap getWindowInfo(QString const& uuid)
    {
        return get_window_info_impl(uuid);
    }

Q_SIGNALS:
    void showingDesktopChanged(bool showing);

protected:
    virtual bool showing_desktop_impl() const = 0;
    virtual int current_desktop_impl() = 0;
    virtual void kill_window_impl() = 0;

    virtual void next_desktop_impl() = 0;
    virtual void previous_desktop_impl() = 0;

    virtual void show_desktop_impl(bool show) = 0;
    virtual bool set_current_desktop_impl(int desktop) = 0;

    virtual QString support_information_impl() = 0;
    virtual QString active_output_name_impl() = 0;
    virtual void unclutter_desktop_impl() = 0;

    virtual void show_debug_console_impl() = 0;
    virtual QVariantMap query_window_info_impl() = 0;
    virtual QVariantMap get_window_info_impl(QString const& uuid) = 0;

private:
    QString m_serviceName;
    win::space_qobject& space;
};

template<typename Space>
class kwin_impl : public kwin
{
public:
    kwin_impl(Space& space)
        : kwin(*space.qobject)
        , space{space}
    {
        QObject::connect(space.qobject.get(),
                         &win::space_qobject::showingDesktopChanged,
                         this,
                         [this](auto show) { Q_EMIT showingDesktopChanged(show); });
    }

    bool showing_desktop_impl() const override
    {
        return space.showing_desktop;
    }

    void kill_window_impl() override
    {
        win::start_window_killer(space);
    }

    void unclutter_desktop_impl() override
    {
        win::unclutter_subspace(space);
    }

    QString support_information_impl() override
    {
        return debug::get_support_info(space);
    }

    QString active_output_name_impl() override
    {
        auto output = win::get_current_output(space);
        if (!output) {
            return {};
        }
        return output->name();
    }

    int current_desktop_impl() override
    {
        return win::subspaces_get_current_x11id(*space.subspace_manager);
    }

    void show_desktop_impl(bool show) override
    {
        win::set_showing_desktop(space, show);
    }

    bool set_current_desktop_impl(int desktop) override
    {
        return win::subspaces_set_current(*space.subspace_manager, desktop);
    }

    void next_desktop_impl() override
    {
        win::subspaces_set_current(*space.subspace_manager,
                                   win::subspaces_get_successor_of(*space.subspace_manager,
                                                                   *space.subspace_manager->current,
                                                                   false));
    }

    void previous_desktop_impl() override
    {
        win::subspaces_set_current(
            *space.subspace_manager,
            win::subspaces_get_predecessor_of(
                *space.subspace_manager, *space.subspace_manager->current, false));
    }

    void show_debug_console_impl() override
    {
        space.show_debug_console();
    }

    QVariantMap query_window_info_impl() override
    {
        if (!space.input) {
            return {};
        }

        m_replyQueryWindowInfo = message();
        setDelayedReply(true);

        space.input->start_interactive_window_selection([this](auto win) {
            if (!win) {
                QDBusConnection::sessionBus().send(m_replyQueryWindowInfo.createErrorReply(
                    QStringLiteral("org.kde.KWin.Error.UserCancel"),
                    QStringLiteral("User cancelled the query")));
                return;
            }
            std::visit(
                overload{[&](auto&& win) {
                    if (!win->control) {
                        QDBusConnection::sessionBus().send(m_replyQueryWindowInfo.createErrorReply(
                            QStringLiteral("org.kde.KWin.Error.InvalidWindow"),
                            QStringLiteral(
                                "Tried to query information about an unmanaged window")));
                        return;
                    }
                    QDBusConnection::sessionBus().send(
                        m_replyQueryWindowInfo.createReply(window_to_variant_map(win)));
                }},
                *win);
        });
        return QVariantMap{};
    }

    QVariantMap get_window_info_impl(QString const& uuid) override
    {
        auto const id = QUuid::fromString(uuid);

        for (auto win : space.windows) {
            if (auto map = std::visit(overload{[&](auto&& win) -> QVariantMap {
                                          if (win->meta.internal_id != id) {
                                              return {};
                                          }
                                          if (!win->control) {
                                              return {};
                                          }
                                          return window_to_variant_map(win);
                                      }},
                                      win);
                !map.isEmpty()) {
                return map;
            }
        }
        return {};
    }

private:
    template<typename Win>
    QByteArray get_client_machine(Win const& win)
    {
        if constexpr (requires(Win win, bool local) { win.wmClientMachine(local); }) {
            return win.wmClientMachine(true);
        }
        return {};
    }
    template<typename Win>
    bool is_local_host(Win const& win)
    {
        if constexpr (requires(Win win) { win.isLocalhost(); }) {
            return win.isLocalhost();
        }
        return true;
    }

    template<typename Win>
    QVariantMap window_to_variant_map(Win const* win)
    {
        return {
            {QStringLiteral("resourceClass"), win->meta.wm_class.res_class},
            {QStringLiteral("resourceName"), win->meta.wm_class.res_name},
            {QStringLiteral("desktopFile"), win->control->desktop_file_name},
            {QStringLiteral("role"), win->windowRole()},
            {QStringLiteral("caption"), win->meta.caption.normal},
            {QStringLiteral("clientMachine"), get_client_machine(*win)},
            {QStringLiteral("localhost"), is_local_host(*win)},
            {QStringLiteral("type"), static_cast<int>(win->windowType())},
            {QStringLiteral("x"), win->geo.pos().x()},
            {QStringLiteral("y"), win->geo.pos().y()},
            {QStringLiteral("width"), win->geo.size().width()},
            {QStringLiteral("height"), win->geo.size().height()},
            {QStringLiteral("desktops"), win::subspaces_ids(*win)},
            {QStringLiteral("minimized"), win->control->minimized},
            {QStringLiteral("shaded"), false},
            {QStringLiteral("fullscreen"), win->control->fullscreen},
            {QStringLiteral("keepAbove"), win->control->keep_above},
            {QStringLiteral("keepBelow"), win->control->keep_below},
            {QStringLiteral("noBorder"), win->noBorder()},
            {QStringLiteral("skipTaskbar"), win->control->skip_taskbar()},
            {QStringLiteral("skipPager"), win->control->skip_pager()},
            {QStringLiteral("skipSwitcher"), win->control->skip_switcher()},
            {QStringLiteral("maximizeHorizontal"),
             static_cast<int>(win->maximizeMode() & win::maximize_mode::horizontal)},
            {QStringLiteral("maximizeVertical"),
             static_cast<int>(win->maximizeMode() & win::maximize_mode::vertical)},
            {QStringLiteral("uuid"), win->meta.internal_id.toString()},
        };
    }

    QDBusMessage m_replyQueryWindowInfo;
    Space& space;
};

}
}
