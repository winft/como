/*
SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screen.h"

#include "integration.h"

#include "platformcursor.h"
#include <como/base/output_helpers.h>
#include <qpa/qwindowsysteminterface.h>

namespace como::QPA
{

namespace
{
int get_forced_dpi()
{
    return qEnvironmentVariableIsSet("QT_WAYLAND_FORCE_DPI")
        ? qEnvironmentVariableIntValue("QT_WAYLAND_FORCE_DPI")
        : -1;
}
}

Screen::Screen(base::output* output, Integration* integration)
    : output{output}
    , m_cursor(new PlatformCursor)
    , m_integration(integration)
{
    QObject::connect(output->qobject.get(), &base::output_qobject::geometry_changed, this, [this] {
        QWindowSystemInterface::handleScreenGeometryChange(screen(), geometry(), geometry());
    });
    QObject::connect(
        output->qobject.get(), &QObject::destroyed, this, [this] { this->output = nullptr; });
}

Screen::~Screen() = default;

QList<QPlatformScreen*> Screen::virtualSiblings() const
{
    auto const screens = m_integration->screens();

    QList<QPlatformScreen*> siblings;
    siblings.reserve(siblings.size());

    for (auto screen : screens) {
        siblings << screen;
    }

    return siblings;
}

int Screen::depth() const
{
    return 32;
}

QImage::Format Screen::format() const
{
    return QImage::Format_ARGB32_Premultiplied;
}

QRect Screen::geometry() const
{
    return output ? output->geometry() : QRect(0, 0, 1, 1);
}

QSizeF Screen::physicalSize() const
{
    return output ? output->physical_size() : QPlatformScreen::physicalSize();
}

QPlatformCursor* Screen::cursor() const
{
    return m_cursor.data();
}

QDpi Screen::logicalDpi() const
{
    auto const dpi = get_forced_dpi();
    if (dpi > 0) {
        return {dpi, dpi};
    }

    return {96, 96};
}

qreal Screen::devicePixelRatio() const
{
    return output ? output->scale() : 1.;
}

QString Screen::name() const
{
    return output ? output->name() : QString();
}

QDpi placeholder_screen::logicalDpi() const
{
    auto const dpi = get_forced_dpi();
    return dpi > 0 ? QDpi{dpi, dpi} : QDpi{96, 96};
}

}
