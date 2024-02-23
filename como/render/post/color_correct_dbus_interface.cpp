/*
SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "color_correct_dbus_interface.h"

#include "night_color_data.h"
#include "nightlightadaptor.h"

#include <QDBusMessage>

namespace como::render::post
{

static void send_changed_properties(QVariantMap const& props)
{
    auto message = QDBusMessage::createSignal(QStringLiteral("/org/kde/KWin/NightLight"),
                                              QStringLiteral("org.freedesktop.DBus.Properties"),
                                              QStringLiteral("PropertiesChanged"));

    message.setArguments({
        QStringLiteral("org.kde.KWin.NightLight"),
        props,
        QStringList(), // invalidated_properties
    });

    QDBusConnection::sessionBus().send(message);
}

color_correct_dbus_interface::color_correct_dbus_interface(
    color_correct_dbus_integration integration)
    : integration{std::move(integration)}
    , m_inhibitorWatcher(new QDBusServiceWatcher(this))
{
    m_inhibitorWatcher->setConnection(QDBusConnection::sessionBus());
    m_inhibitorWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_inhibitorWatcher,
            &QDBusServiceWatcher::serviceUnregistered,
            this,
            &color_correct_dbus_interface::removeInhibitorService);

    new NightLightAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin/NightLight"), this);
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.KWin.NightLight"));
}

color_correct_dbus_interface::~color_correct_dbus_interface()
{
    QDBusConnection::sessionBus().unregisterService(QStringLiteral("org.kde.KWin.NightLight"));
}

bool color_correct_dbus_interface::isInhibited() const
{
    return integration.data.inhibit_reference_count;
}

bool color_correct_dbus_interface::isEnabled() const
{
    return integration.data.enabled;
}

bool color_correct_dbus_interface::isRunning() const
{
    return integration.data.running;
}

bool color_correct_dbus_interface::isAvailable() const
{
    return integration.data.available;
}

int color_correct_dbus_interface::currentTemperature() const
{
    return integration.data.temperature.current;
}

int color_correct_dbus_interface::targetTemperature() const
{
    return integration.data.temperature.target;
}

int color_correct_dbus_interface::mode() const
{
    return integration.data.mode;
}

bool color_correct_dbus_interface::daylight() const
{
    return integration.data.daylight;
}

quint64 color_correct_dbus_interface::previousTransitionDateTime() const
{
    auto const dateTime = integration.data.transition.prev.first;
    if (dateTime.isValid()) {
        return quint64(dateTime.toSecsSinceEpoch());
    }
    return 0;
}

quint32 color_correct_dbus_interface::previousTransitionDuration() const
{
    return quint32(integration.data.previous_transition_duration());
}

quint64 color_correct_dbus_interface::scheduledTransitionDateTime() const
{
    auto const dateTime = integration.data.transition.next.first;
    if (dateTime.isValid()) {
        return quint64(dateTime.toSecsSinceEpoch());
    }
    return 0;
}

quint32 color_correct_dbus_interface::scheduledTransitionDuration() const
{
    return quint32(integration.data.scheduled_transition_duration());
}

void color_correct_dbus_interface::send_inhibited(bool inhibited) const
{
    QVariantMap props;
    props.insert(QStringLiteral("inhibited"), inhibited);
    send_changed_properties(props);
}

void color_correct_dbus_interface::send_enabled(bool enabled) const
{
    QVariantMap props;
    props.insert(QStringLiteral("enabled"), enabled);
    send_changed_properties(props);
}

void color_correct_dbus_interface::send_running(bool running) const
{
    QVariantMap props;
    props.insert(QStringLiteral("running"), running);
    send_changed_properties(props);
}

void color_correct_dbus_interface::send_current_temperature(int temp) const
{
    QVariantMap props;
    props.insert(QStringLiteral("currentTemperature"), temp);
    send_changed_properties(props);
}

void color_correct_dbus_interface::send_target_temperature(int temp) const
{
    QVariantMap props;
    props.insert(QStringLiteral("targetTemperature"), temp);
    send_changed_properties(props);
}

void color_correct_dbus_interface::send_mode(night_color_mode mode) const
{
    QVariantMap props;
    props.insert(QStringLiteral("mode"), static_cast<uint>(mode));
    send_changed_properties(props);
}

void color_correct_dbus_interface::send_daylight(bool daylight) const
{
    QVariantMap props;
    props.insert(QStringLiteral("daylight"), static_cast<uint>(daylight));
    send_changed_properties(props);
}

void color_correct_dbus_interface::send_transition_timings() const
{
    QVariantMap props;
    props.insert(QStringLiteral("previousTransitionDateTime"), previousTransitionDateTime());
    props.insert(QStringLiteral("previousTransitionDuration"), previousTransitionDuration());

    props.insert(QStringLiteral("scheduledTransitionDateTime"), scheduledTransitionDateTime());
    props.insert(QStringLiteral("scheduledTransitionDuration"), scheduledTransitionDuration());

    send_changed_properties(props);
}

void color_correct_dbus_interface::setLocation(double latitude, double longitude)
{
    integration.loc_update(latitude, longitude);
}

uint color_correct_dbus_interface::inhibit()
{
    const QString serviceName = QDBusContext::message().service();

    if (!m_inhibitors.contains(serviceName)) {
        m_inhibitorWatcher->addWatchedService(serviceName);
    }

    m_inhibitors.insert(serviceName, ++m_lastInhibitionCookie);

    integration.inhibit(true);

    return m_lastInhibitionCookie;
}

void color_correct_dbus_interface::uninhibit(uint cookie)
{
    const QString serviceName = QDBusContext::message().service();

    uninhibit(serviceName, cookie);
}

void color_correct_dbus_interface::uninhibit(const QString& serviceName, uint cookie)
{
    const int removedCount = m_inhibitors.remove(serviceName, cookie);
    if (!removedCount) {
        return;
    }

    if (!m_inhibitors.contains(serviceName)) {
        m_inhibitorWatcher->removeWatchedService(serviceName);
    }

    integration.inhibit(false);
}

void color_correct_dbus_interface::removeInhibitorService(const QString& serviceName)
{
    const auto cookies = m_inhibitors.values(serviceName);
    for (const uint& cookie : cookies) {
        uninhibit(serviceName, cookie);
    }
}

void color_correct_dbus_interface::preview(uint /*previewTemp*/)
{
    // TODO(romangg): implement
}

void color_correct_dbus_interface::stopPreview()
{
    // TODO(romangg): implement
}

}
