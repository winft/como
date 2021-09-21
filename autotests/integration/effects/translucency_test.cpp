/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "effect_builtins.h"
#include "effectloader.h"
#include "effects.h"
#include "input/cursor.h"
#include "kwin_wayland_test.h"
#include "platform.h"
#include "render/compositor.h"
#include "wayland_server.h"
#include "workspace.h"

#include "win/move.h"
#include "win/x11/window.h"

#include <KConfigGroup>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

namespace KWin
{

class TranslucencyTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMoveAfterDesktopChange();
    void testDialogClose();

private:
    Effect* m_translucencyEffect = nullptr;
};

void TranslucencyTest::initTestCase()
{
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());
    qRegisterMetaType<win::wayland::window*>();
    qRegisterMetaType<KWin::win::x11::window*>();
    qRegisterMetaType<KWin::Effect*>();

    QSignalSpy startup_spy(kwinApp(), &Application::startup_finished);
    QVERIFY(startup_spy.isValid());
    kwinApp()->platform->setInitialWindowSize(QSize(1280, 1024));

    // disable all effects - we don't want to have it interact with the rendering
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    ScriptedEffectLoader loader;
    const auto builtinNames = BuiltInEffects::availableEffectNames() << loader.listOfKnownEffects();
    for (QString name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }
    config->group("Outline").writeEntry(QStringLiteral("QmlPath"), QString("/does/not/exist.qml"));
    config->group("Effect-kwin4_effect_translucency").writeEntry(QStringLiteral("Dialogs"), 90);

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", "1");
    kwinApp()->start();
    QVERIFY(startup_spy.wait());
    QVERIFY(render::compositor::self());
}

void TranslucencyTest::init()
{
    // load the translucency effect
    EffectsHandlerImpl* e = static_cast<EffectsHandlerImpl*>(effects);
    // find the effectsloader
    auto effectloader = e->findChild<AbstractEffectLoader*>();
    QVERIFY(effectloader);
    QSignalSpy effectLoadedSpy(effectloader, &AbstractEffectLoader::effectLoaded);
    QVERIFY(effectLoadedSpy.isValid());

    QVERIFY(!e->isEffectLoaded(QStringLiteral("kwin4_effect_translucency")));
    QVERIFY(e->loadEffect(QStringLiteral("kwin4_effect_translucency")));
    QVERIFY(e->isEffectLoaded(QStringLiteral("kwin4_effect_translucency")));

    QCOMPARE(effectLoadedSpy.count(), 1);
    m_translucencyEffect = effectLoadedSpy.first().first().value<Effect*>();
    QVERIFY(m_translucencyEffect);
}

void TranslucencyTest::cleanup()
{
    EffectsHandlerImpl* e = static_cast<EffectsHandlerImpl*>(effects);
    if (e->isEffectLoaded(QStringLiteral("kwin4_effect_translucency"))) {
        e->unloadEffect(QStringLiteral("kwin4_effect_translucency"));
    }
    QVERIFY(!e->isEffectLoaded(QStringLiteral("kwin4_effect_translucency")));
    m_translucencyEffect = nullptr;
}

void xcb_connection_deleter(xcb_connection_t* pointer)
{
    xcb_disconnect(pointer);
}

using xcb_connection_ptr = std::unique_ptr<xcb_connection_t, void (*)(xcb_connection_t*)>;

xcb_connection_ptr create_xcb_connection()
{
    return xcb_connection_ptr(xcb_connect(nullptr, nullptr), xcb_connection_deleter);
}

void TranslucencyTest::testMoveAfterDesktopChange()
{
    // test tries to simulate the condition of bug 366081
    QVERIFY(!m_translucencyEffect->isActive());

    QSignalSpy windowAddedSpy(effects, &EffectsHandler::windowAdded);
    QVERIFY(windowAddedSpy.isValid());

    // create an xcb window
    auto c = create_xcb_connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t w = xcb_generate_id(c.get());
    xcb_create_window(c.get(),
                      XCB_COPY_FROM_PARENT,
                      w,
                      rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_COPY_FROM_PARENT,
                      0,
                      nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), w, &hints);
    xcb_map_window(c.get(), w);
    xcb_flush(c.get());

    // we should get a client for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    auto client = windowCreatedSpy.first().first().value<win::x11::window*>();
    QVERIFY(client);
    QCOMPARE(client->xcb_window(), w);
    QVERIFY(win::decoration(client));

    QVERIFY(windowAddedSpy.wait());
    QVERIFY(!m_translucencyEffect->isActive());
    // let's send the window to desktop 2
    effects->setNumberOfDesktops(2);
    QCOMPARE(effects->numberOfDesktops(), 2);
    workspace()->sendClientToDesktop(client, 2, false);
    effects->setCurrentDesktop(2);
    QVERIFY(!m_translucencyEffect->isActive());
    input::get_cursor()->set_pos(client->frameGeometry().center());
    workspace()->performWindowOperation(client, Options::MoveOp);
    QVERIFY(m_translucencyEffect->isActive());
    QTest::qWait(200);
    QVERIFY(m_translucencyEffect->isActive());

    // now end move resize
    win::end_move_resize(client);

    QVERIFY(m_translucencyEffect->isActive());
    QTest::qWait(500);
    QTRY_VERIFY(!m_translucencyEffect->isActive());

    // and destroy the window again
    xcb_unmap_window(c.get(), w);
    xcb_flush(c.get());

    QSignalSpy windowClosedSpy(client, &win::x11::window::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());
    xcb_destroy_window(c.get(), w);
    c.reset();
}

void TranslucencyTest::testDialogClose()
{
    // this test simulates the condition of BUG 342716
    // with translucency settings for window type dialog the effect never ends when the window gets
    // destroyed
    QVERIFY(!m_translucencyEffect->isActive());
    QSignalSpy windowAddedSpy(effects, &EffectsHandler::windowAdded);
    QVERIFY(windowAddedSpy.isValid());

    // create an xcb window
    auto c = create_xcb_connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t w = xcb_generate_id(c.get());
    xcb_create_window(c.get(),
                      XCB_COPY_FROM_PARENT,
                      w,
                      rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_COPY_FROM_PARENT,
                      0,
                      nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), w, &hints);
    NETWinInfo winInfo(c.get(), w, rootWindow(), NET::Properties(), NET::Properties2());
    winInfo.setWindowType(NET::Dialog);
    xcb_map_window(c.get(), w);
    xcb_flush(c.get());

    // we should get a client for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    auto client = windowCreatedSpy.first().first().value<win::x11::window*>();
    QVERIFY(client);
    QCOMPARE(client->xcb_window(), w);
    QVERIFY(win::decoration(client));
    QVERIFY(win::is_dialog(client));

    QVERIFY(windowAddedSpy.wait());
    QTRY_VERIFY(m_translucencyEffect->isActive());
    // and destroy the window again
    xcb_unmap_window(c.get(), w);
    xcb_flush(c.get());

    QSignalSpy windowClosedSpy(client, &win::x11::window::windowClosed);
    QVERIFY(windowClosedSpy.isValid());

    QSignalSpy windowDeletedSpy(effects, &EffectsHandler::windowDeleted);
    QVERIFY(windowDeletedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());
    if (windowDeletedSpy.isEmpty()) {
        QVERIFY(windowDeletedSpy.wait());
    }
    QCOMPARE(windowDeletedSpy.count(), 1);
    QTRY_VERIFY(!m_translucencyEffect->isActive());
    xcb_destroy_window(c.get(), w);
    c.reset();
}

}

WAYLANDTEST_MAIN(KWin::TranslucencyTest)
#include "translucency_test.moc"
