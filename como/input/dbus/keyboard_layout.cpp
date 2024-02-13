/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "keyboard_layout.h"

#include <como/input/event.h>
#include <como/input/xkb/helpers.h>
#include <como/input/xkb/layout_manager.h>
#include <como/input/xkb/layout_policies.h>

#include <KLocalizedString>
#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>

namespace como::input::dbus
{

static const QString s_keyboardService = QStringLiteral("org.kde.keyboard");
static const QString s_keyboardObject = QStringLiteral("/Layouts");

keyboard_layout::keyboard_layout(KConfigGroup const& configGroup,
                                 std::function<xkb::keyboard*()> xkb_getter)
    : m_configGroup(configGroup)
    , xkb_getter{xkb_getter}
{
    qRegisterMetaType<QVector<LayoutNames>>("QVector<LayoutNames>");
    qDBusRegisterMetaType<LayoutNames>();
    qDBusRegisterMetaType<QVector<LayoutNames>>();

    QDBusConnection::sessionBus().registerObject(s_keyboardObject,
                                                 this,
                                                 QDBusConnection::ExportAllSlots
                                                     | QDBusConnection::ExportAllSignals);
    QDBusConnection::sessionBus().registerService(s_keyboardService);
}

keyboard_layout::~keyboard_layout()
{
    QDBusConnection::sessionBus().unregisterService(s_keyboardService);
}

void keyboard_layout::switchToNextLayout()
{
    Q_EMIT next_layout_requested();
}

void keyboard_layout::switchToPreviousLayout()
{
    Q_EMIT previous_layout_requested();
}

bool keyboard_layout::setLayout(uint index)
{
    auto xkb = xkb_getter();

    if (!xkb->switch_to_layout(index)) {
        return false;
    }

    return true;
}

uint keyboard_layout::getLayout() const
{
    return xkb_getter()->layout;
}

QVector<keyboard_layout::LayoutNames> keyboard_layout::getLayoutsList() const
{
    auto xkb = xkb_getter();

    // TODO: - should be handled by layout applet itself, it has nothing to do with KWin
    auto const display_names = m_configGroup.readEntry("DisplayNames", QStringList());

    QVector<LayoutNames> ret;
    auto const layouts_count = xkb->layouts_count();
    size_t const display_names_count = display_names.size();

    for (size_t i = 0; i < layouts_count; ++i) {
        ret.append({QString::fromStdString(xkb->layout_short_name_from_index(i)),
                    i < display_names_count ? display_names.at(i) : QString(),
                    xkb::translated_keyboard_layout(xkb->layout_name_from_index(i))});
    }
    return ret;
}

QDBusArgument& operator<<(QDBusArgument& argument, const keyboard_layout::LayoutNames& layoutNames)
{
    argument.beginStructure();
    argument << layoutNames.shortName << layoutNames.displayName << layoutNames.longName;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument,
                                keyboard_layout::LayoutNames& layoutNames)
{
    argument.beginStructure();
    argument >> layoutNames.shortName >> layoutNames.displayName >> layoutNames.longName;
    argument.endStructure();
    return argument;
}

}
