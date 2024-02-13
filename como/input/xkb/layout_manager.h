/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "helpers.h"
#include "layout_policies.h"

#include "como_export.h"
#include <como/input/dbus/keyboard_layout.h>
#include <como/input/dbus/keyboard_layouts_v2.h>
#include <como/input/keyboard.h>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QObject>
#include <QString>
#include <QVector>
#include <memory>
#include <string>
#include <xkbcommon/xkbcommon.h>

namespace como::input::xkb
{

inline QString translated_keyboard_layout(std::string const& layout)
{
    return i18nd("xkeyboard-config", layout.c_str());
}

class COMO_EXPORT layout_manager_qobject : public QObject
{
    Q_OBJECT
public:
    layout_manager_qobject(std::function<void()> reconfigure_callback);

Q_SIGNALS:
    void layoutChanged(uint index);
    void layoutsReconfigured();

private Q_SLOTS:
    void reconfigure();

private:
    std::function<void()> reconfigure_callback;
};

template<typename Redirect>
class layout_manager
{
public:
    layout_manager(Redirect& redirect, KSharedConfigPtr const& config)
        : qobject{std::make_unique<layout_manager_qobject>([this] { reconfigure(); })}
        , redirect{redirect}
        , m_configGroup(config->group(QStringLiteral("Layout")))
    {
        auto switchKeyboardAction = new QAction(qobject.get());
        switchKeyboardAction->setObjectName(QStringLiteral("Switch to Next Keyboard Layout"));
        switchKeyboardAction->setProperty("componentName",
                                          QStringLiteral("KDE Keyboard Layout Switcher"));
        QKeySequence sequence{Qt::META | Qt::ALT | Qt::Key_K};
        redirect.platform.shortcuts->register_keyboard_default_shortcut(switchKeyboardAction,
                                                                        {sequence});
        redirect.platform.shortcuts->register_keyboard_shortcut(switchKeyboardAction, {sequence});
        redirect.platform.setup_action_for_global_accel(switchKeyboardAction);
        QObject::connect(switchKeyboardAction, &QAction::triggered, qobject.get(), [this] {
            switchToNextLayout();
        });

        auto switch_last_used_action = new QAction(qobject.get());
        switch_last_used_action->setObjectName(
            QStringLiteral("Switch to Last-Used Keyboard Layout"));
        switch_last_used_action->setProperty("componentName",
                                             QStringLiteral("KDE Keyboard Layout Switcher"));
        switch_last_used_action->setProperty("componentDisplayName",
                                             i18n("Keyboard Layout Switcher"));
        QKeySequence sequence_last_used{Qt::META | Qt::ALT | Qt::Key_L};
        redirect.platform.shortcuts->register_keyboard_default_shortcut(switch_last_used_action,
                                                                        {sequence_last_used});
        redirect.platform.shortcuts->register_keyboard_shortcut(switch_last_used_action,
                                                                {sequence_last_used});
        redirect.platform.setup_action_for_global_accel(switch_last_used_action);

        QObject::connect(switch_last_used_action, &QAction::triggered, qobject.get(), [this] {
            switch_to_last_used_layout();
        });

        reconfigure();
        init_dbus_interface_v2();

        for (auto keyboard : redirect.platform.keyboards) {
            add_keyboard(keyboard);
        }

        QObject::connect(redirect.platform.qobject.get(),
                         &platform_qobject::keyboard_added,
                         qobject.get(),
                         [this](auto keys) { add_keyboard(keys); });
    }

    void switchToNextLayout()
    {
        get_keyboard()->switch_to_next_layout();
    }

    void switchToPreviousLayout()
    {
        get_keyboard()->switch_to_previous_layout();
    }

    void switch_to_last_used_layout()
    {
        get_keyboard()->switch_to_last_used_layout();
    }

    std::unique_ptr<layout_manager_qobject> qobject;
    Redirect& redirect;

private:
    void reconfigure()
    {
        redirect.platform.xkb.reconfigure();

        if (m_configGroup.isValid()) {
            m_configGroup.config()->reparseConfiguration();
            const QString policyKey
                = m_configGroup.readEntry("SwitchMode", QStringLiteral("Global"));
            if (!m_policy || m_policy->name.c_str() != policyKey) {
                m_policy = create_layout_policy(this, m_configGroup, policyKey);
            }
        }

        load_shortcuts();

        if (requires_dbus_interface_v1()) {
            init_dbus_interface_v1();
        } else if (dbus_interface_v1) {
            // Emit change before reset for backwards compatibility (was done so in the past).
            Q_EMIT dbus_interface_v1->layoutListChanged();
            dbus_interface_v1.reset();
        }

        Q_EMIT qobject->layoutsReconfigured();
    }

    void init_dbus_interface_v1()
    {
        assert(requires_dbus_interface_v1());

        if (dbus_interface_v1) {
            return;
        }

        dbus_interface_v1 = std::make_unique<dbus::keyboard_layout>(
            m_configGroup, [this] { return get_primary_xkb_keyboard(this->redirect.platform); });

        QObject::connect(qobject.get(),
                         &layout_manager_qobject::layoutChanged,
                         dbus_interface_v1.get(),
                         &dbus::keyboard_layout::layoutChanged);
        // TODO: the signal might be emitted even if the list didn't change
        QObject::connect(qobject.get(),
                         &layout_manager_qobject::layoutsReconfigured,
                         dbus_interface_v1.get(),
                         &dbus::keyboard_layout::layoutListChanged);
    }

    void init_dbus_interface_v2()
    {
        assert(!dbus_interface_v2);
        dbus_interface_v2 = dbus::keyboard_layouts_v2::create(&redirect.platform);
    }

    void add_keyboard(input::keyboard* keyboard)
    {
        if (!keyboard->control || !keyboard->control->is_alpha_numeric_keyboard()) {
            return;
        }

        auto xkb = keyboard->xkb.get();
        QObject::connect(xkb->qobject.get(),
                         &xkb::keyboard_qobject::layout_changed,
                         qobject.get(),
                         [this, xkb] { handle_layout_change(xkb); });
    }

    void handle_layout_change(xkb::keyboard* xkb)
    {
        if (xkb != get_keyboard()) {
            // We currently only inform about changes on the primary device.
            return;
        }
        send_layout_to_osd(xkb);
        Q_EMIT qobject->layoutChanged(xkb->layout);
    }

    void send_layout_to_osd(xkb::keyboard* xkb)
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                                          QStringLiteral("/org/kde/osdService"),
                                                          QStringLiteral("org.kde.osdService"),
                                                          QStringLiteral("kbdLayoutChanged"));
        msg << translated_keyboard_layout(xkb->layout_name());
        QDBusConnection::sessionBus().asyncCall(msg);
    }

    void switchToLayout(xkb_layout_index_t index)
    {
        get_keyboard()->switch_to_layout(index);
    }

    void load_shortcuts()
    {
        qDeleteAll(m_layoutShortcuts);
        m_layoutShortcuts.clear();

        auto xkb = get_keyboard();
        const QString componentName = QStringLiteral("KDE Keyboard Layout Switcher");
        auto const count = xkb->layouts_count();

        for (uint i = 0; i < count; ++i) {
            // layout name is translated in the action name in keyboard kcm!
            const QString action
                = QStringLiteral("Switch keyboard layout to %1")
                      .arg(translated_keyboard_layout(xkb->layout_name_from_index(i)));
            auto const shortcuts
                = redirect.platform.shortcuts->get_keyboard_shortcut(componentName, action);
            if (shortcuts.isEmpty()) {
                continue;
            }
            auto a = new QAction(qobject.get());
            a->setObjectName(action);
            a->setProperty("componentName", componentName);
            QObject::connect(
                a, &QAction::triggered, qobject.get(), [this, i] { switchToLayout(i); });
            redirect.platform.shortcuts->register_keyboard_shortcut(a, shortcuts);
            m_layoutShortcuts << a;
        }
    }

    auto get_keyboard()
    {
        return get_primary_xkb_keyboard(redirect.platform);
    }

    // The interface is advertised iff there are more than one layouts.
    bool requires_dbus_interface_v1()
    {
        return get_keyboard()->layouts_count() > 1;
    }

    KConfigGroup m_configGroup;
    QVector<QAction*> m_layoutShortcuts;
    std::unique_ptr<dbus::keyboard_layout> dbus_interface_v1;
    std::unique_ptr<dbus::keyboard_layouts_v2> dbus_interface_v2;
    std::unique_ptr<xkb::layout_policy<layout_manager>> m_policy;
};

}
