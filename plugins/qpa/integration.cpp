/*
SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "integration.h"

#include "backingstore.h"
#include "offscreensurface.h"
#include "screen.h"
#include "sharingplatformcontext.h"
#include "window.h"
#include <como/base/app_singleton.h>
#include <como/base/platform_qobject.h>
#include <como/base/singleton_interface.h>
#include <como/render/singleton_interface.h>

#include <QCoreApplication>
#include <QtConcurrentRun>

#include <qpa/qplatformaccessibility.h>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformwindow.h>
#include <qpa/qwindowsysteminterface.h>

#include <QtGui/private/qgenericunixfontdatabase_p.h>
#include <QtGui/private/qgenericunixthemes_p.h>
#include <QtGui/private/qspiaccessiblebridge_p.h>
#include <QtGui/private/qunixeventdispatcher_qpa_p.h>

namespace como
{

namespace QPA
{

Integration::Integration()
    : QObject()
    , QPlatformIntegration()
    , m_fontDb(new QGenericUnixFontDatabase())
    , m_nativeInterface(new QPlatformNativeInterface())
    , m_services(new QGenericUnixServices())
{
}

Integration::~Integration()
{
    for (QPlatformScreen* platformScreen : std::as_const(m_screens)) {
        QWindowSystemInterface::handleScreenRemoved(platformScreen);
    }
    if (m_dummyScreen) {
        QWindowSystemInterface::handleScreenRemoved(m_dummyScreen);
    }
}

QHash<como::base::output*, Screen*> Integration::screens() const
{
    return m_screens;
}

bool Integration::hasCapability(Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps:
        return true;
    case OpenGL:
        return true;
    case ThreadedOpenGL:
        return false;
    case BufferQueueingOpenGL:
        return false;
    case MultipleWindows:
    case NonFullScreenWindows:
        return true;
    case RasterGLSurface:
        return false;
    default:
        return QPlatformIntegration::hasCapability(cap);
    }
}

void Integration::initialize()
{
    assert(base::singleton_interface::app_singleton);
    connect(base::singleton_interface::app_singleton,
            &base::app_singleton::platform_created,
            this,
            &Integration::handle_platform_created);

    QPlatformIntegration::initialize();

    assert(m_screens.empty());
    assert(!m_dummyScreen);
    m_dummyScreen = new placeholder_screen;
    QWindowSystemInterface::handleScreenAdded(m_dummyScreen);
}

QAbstractEventDispatcher* Integration::createEventDispatcher() const
{
    return new QUnixEventDispatcherQPA;
}

QPlatformBackingStore* Integration::createPlatformBackingStore(QWindow* window) const
{
    return new BackingStore(window);
}

QPlatformWindow* Integration::createPlatformWindow(QWindow* window) const
{
    return new Window(window);
}

QPlatformOffscreenSurface*
Integration::createPlatformOffscreenSurface(QOffscreenSurface* surface) const
{
    return new OffscreenSurface(surface);
}

QPlatformFontDatabase* Integration::fontDatabase() const
{
    return m_fontDb.get();
}

QPlatformTheme* Integration::createPlatformTheme(const QString& name) const
{
    return QGenericUnixTheme::createUnixTheme(name);
}

QStringList Integration::themeNames() const
{
    if (qEnvironmentVariableIsSet("KDE_FULL_SESSION")) {
        return QStringList({QStringLiteral("kde")});
    }
    return QStringList({QLatin1String(QGenericUnixTheme::name)});
}

QPlatformOpenGLContext* Integration::createPlatformOpenGLContext(QOpenGLContext* context) const
{
    assert(render::singleton_interface::supports_surfaceless_context);
    if (render::singleton_interface::supports_surfaceless_context()) {
        return new SharingPlatformContext(context);
    }
    return nullptr;
}

QPlatformAccessibility* Integration::accessibility() const
{
    if (!m_accessibility) {
        m_accessibility.reset(new QSpiAccessibleBridge());
    }
    return m_accessibility.get();
}

void Integration::handle_platform_created()
{
    auto platform = base::singleton_interface::platform;
    assert(platform);

    assert(base::singleton_interface::get_outputs().empty());
    QObject::connect(
        platform, &base::platform_qobject::output_added, this, &Integration::handle_output_added);
    QObject::connect(platform,
                     &base::platform_qobject::output_removed,
                     this,
                     &Integration::handle_output_removed);
    QObject::connect(platform,
                     &base::platform_qobject::destroyed,
                     this,
                     &Integration::handle_platform_destroyed);
}

void Integration::handle_platform_destroyed()
{
    if (!m_dummyScreen) {
        m_dummyScreen = new placeholder_screen;
        QWindowSystemInterface::handleScreenAdded(m_dummyScreen);
    }

    auto const screens_copy = m_screens;
    m_screens.clear();
    for (auto screen : std::as_const(screens_copy)) {
        QWindowSystemInterface::handleScreenRemoved(screen);
    }
}

void Integration::handle_output_added(como::base::output* output)
{
    assert(!m_screens.contains(output));

    auto screen = new Screen(output, this);
    m_screens.insert(output, screen);
    QWindowSystemInterface::handleScreenAdded(screen);

    if (m_dummyScreen) {
        QWindowSystemInterface::handleScreenRemoved(m_dummyScreen);
        m_dummyScreen = nullptr;
    }
}

void Integration::handle_output_removed(como::base::output* output)
{
    assert(m_screens.contains(output));

    if (m_screens.size() == 1) {
        m_dummyScreen = new placeholder_screen;
        QWindowSystemInterface::handleScreenAdded(m_dummyScreen);
    }

    QWindowSystemInterface::handleScreenRemoved(m_screens.take(output));
}

QPlatformNativeInterface* Integration::nativeInterface() const
{
    return m_nativeInterface.data();
}

QPlatformServices* Integration::services() const
{
    return m_services.data();
}

}
}
