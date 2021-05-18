/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2019 David Edmundson <davidedmundson@kde.org>
Copyright (C) 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

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
#include "cursor.h"
#include "kwin_wayland_test.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include "win/placement.h"
#include "win/wayland/window.h"

#include <Wrapland/Client/compositor.h>
#include <Wrapland/Client/plasmashell.h>
#include <Wrapland/Client/shm_pool.h>
#include <Wrapland/Client/surface.h>
#include <Wrapland/Client/xdgdecoration.h>
#include <Wrapland/Client/xdg_shell.h>

using namespace KWin;
using namespace Wrapland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_placement-0");

struct PlaceWindowResult
{
    QSize initiallyConfiguredSize;
    Wrapland::Client::XdgShellToplevel::States initiallyConfiguredStates;
    QRect finalGeometry;
};

class TestPlacement : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void initTestCase();

    void testPlaceSmart();
    void testPlaceZeroCornered();
    void testPlaceMaximized();
    void testPlaceMaximizedLeavesFullscreen();
    void testPlaceCentered();
    void testPlaceUnderMouse();
    void testPlaceRandom();

private:
    void setPlacementPolicy(win::placement policy);
    /*
     * Create a window with the lifespan of parent and return relevant results for testing
     * defaultSize is the buffer size to use if the compositor returns an empty size in the first configure
     * event.
     */
    PlaceWindowResult createAndPlaceWindow(const QSize &defaultSize, QObject *parent);
};

void TestPlacement::init()
{
    Test::setupWaylandConnection(Test::AdditionalWaylandInterface::XdgDecoration |
                                 Test::AdditionalWaylandInterface::PlasmaShell);

    screens()->setCurrent(0);
    KWin::Cursor::setPos(QPoint(512, 512));
}

void TestPlacement::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestPlacement::initTestCase()
{
    qRegisterMetaType<win::wayland::window*>();

    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TestPlacement::setPlacementPolicy(win::placement policy)
{
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", win::policy_to_string(policy));
    group.sync();
    Workspace::self()->slotReconfigure();
}

PlaceWindowResult TestPlacement::createAndPlaceWindow(const QSize &defaultSize, QObject *parent)
{
    PlaceWindowResult rc;

    QSignalSpy window_spy(waylandServer(), &WaylandServer::window_added);
    assert(window_spy.isValid());

    // create a new window
    auto surface = Test::createSurface(parent);
    auto shellSurface = Test::create_xdg_shell_toplevel(surface, surface, Test::CreationSetup::CreateOnly);
    QSignalSpy configSpy(shellSurface, &XdgShellToplevel::configureRequested);
    assert(configSpy.isValid());

    surface->commit(Surface::CommitFlag::None);
    configSpy.wait();

    // First configure is always sent with empty size.
    assert(configSpy[0][0].toSize().isEmpty());
    shellSurface->ackConfigure(configSpy[0][2].toUInt());
    configSpy.clear();

    Test::render(surface, defaultSize, Qt::red);
    configSpy.wait();

    auto window = window_spy.first().first().value<win::wayland::window*>();

    rc.initiallyConfiguredSize = configSpy[0][0].toSize();
    rc.initiallyConfiguredStates = configSpy[0][1].value<Wrapland::Client::XdgShellToplevel::States>();
    shellSurface->ackConfigure(configSpy[0][2].toUInt());

    Test::render(surface, rc.initiallyConfiguredSize, Qt::red);
    configSpy.wait(100);

    rc.finalGeometry = window->frameGeometry();
    return rc;
}

void TestPlacement::testPlaceSmart()
{
    setPlacementPolicy(win::placement::smart);

    QScopedPointer<QObject> testParent(new QObject); //dumb QObject just for scoping surfaces to the test

    QRegion usedArea;

    for (int i = 0; i < 4; i++) {
        PlaceWindowResult windowPlacement = createAndPlaceWindow(QSize(600, 500), testParent.data());
        // smart placement shouldn't define a size on clients
        QCOMPARE(windowPlacement.initiallyConfiguredSize, QSize(600, 500));
        QCOMPARE(windowPlacement.finalGeometry.size(), QSize(600, 500));

        // exact placement isn't a defined concept that should be tested
        // but the goal of smart placement is to make sure windows don't overlap until they need to
        // 4 windows of 600, 500 should fit without overlap
        QVERIFY(!usedArea.intersects(windowPlacement.finalGeometry));
        usedArea += windowPlacement.finalGeometry;
    }
}

void TestPlacement::testPlaceZeroCornered()
{
    setPlacementPolicy(win::placement::zero_cornered);

    QScopedPointer<QObject> testParent(new QObject);

    for (int i = 0; i < 4; i++) {
        PlaceWindowResult windowPlacement = createAndPlaceWindow(QSize(600, 500), testParent.data());
        // smart placement shouldn't define a size on clients
        QCOMPARE(windowPlacement.initiallyConfiguredSize, QSize(600, 500));
        // size should match our buffer
        QCOMPARE(windowPlacement.finalGeometry.size(), QSize(600, 500));
        //and it should be in the corner
        QCOMPARE(windowPlacement.finalGeometry.topLeft(), QPoint(0, 0));
    }
}

void TestPlacement::testPlaceMaximized()
{
    setPlacementPolicy(win::placement::maximizing);

    // add a top panel
    QScopedPointer<Surface> panelSurface(Test::createSurface());
    QScopedPointer<QObject> panelShellSurface(Test::create_xdg_shell_toplevel(panelSurface.data()));
    QScopedPointer<PlasmaShellSurface> plasmaSurface(Test::waylandPlasmaShell()->createSurface(panelSurface.data()));
    plasmaSurface->setRole(PlasmaShellSurface::Role::Panel);
    plasmaSurface->setPosition(QPoint(0, 0));
    Test::renderAndWaitForShown(panelSurface.data(), QSize(1280, 20), Qt::blue);

    QScopedPointer<QObject> testParent(new QObject);

    // all windows should be initially maximized with an initial configure size sent
    for (int i = 0; i < 4; i++) {
        PlaceWindowResult windowPlacement = createAndPlaceWindow(QSize(600, 500), testParent.data());
        QVERIFY(windowPlacement.initiallyConfiguredStates & XdgShellToplevel::State::Maximized);
        QCOMPARE(windowPlacement.initiallyConfiguredSize, QSize(1280, 1024 - 20));
        QCOMPARE(windowPlacement.finalGeometry, QRect(0, 20, 1280, 1024 - 20)); // under the panel
    }
}

void TestPlacement::testPlaceMaximizedLeavesFullscreen()
{
    setPlacementPolicy(win::placement::maximizing);

    // add a top panel
    QScopedPointer<Surface> panelSurface(Test::createSurface());
    QScopedPointer<QObject> panelShellSurface(Test::create_xdg_shell_toplevel(panelSurface.data()));
    QScopedPointer<PlasmaShellSurface> plasmaSurface(Test::waylandPlasmaShell()->createSurface(panelSurface.data()));
    plasmaSurface->setRole(PlasmaShellSurface::Role::Panel);
    plasmaSurface->setPosition(QPoint(0, 0));
    Test::renderAndWaitForShown(panelSurface.data(), QSize(1280, 20), Qt::blue);

    QScopedPointer<QObject> testParent(new QObject);

    // all windows should be initially fullscreen with an initial configure size sent, despite the policy
    for (int i = 0; i < 4; i++) {
        auto surface = Test::createSurface(testParent.data());
        auto shellSurface = Test::create_xdg_shell_toplevel(surface, surface, Test::CreationSetup::CreateOnly);
        shellSurface->setFullscreen(true);
        QSignalSpy configSpy(shellSurface, &XdgShellToplevel::configureRequested);
        surface->commit(Surface::CommitFlag::None);
        configSpy.wait();

        auto initiallyConfiguredSize = configSpy[0][0].toSize();
        auto initiallyConfiguredStates = configSpy[0][1].value<Wrapland::Client::XdgShellToplevel::States>();
        shellSurface->ackConfigure(configSpy[0][2].toUInt());

        auto c = Test::renderAndWaitForShown(surface, initiallyConfiguredSize, Qt::red);

        QVERIFY(initiallyConfiguredStates & XdgShellToplevel::State::Fullscreen);
        QCOMPARE(initiallyConfiguredSize, QSize(1280, 1024 ));
        QCOMPARE(c->frameGeometry(), QRect(0, 0, 1280, 1024));
    }
}

void TestPlacement::testPlaceCentered()
{
    // This test verifies that Centered placement policy works.

    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", win::policy_to_string(win::placement::centered));
    group.sync();
    workspace()->slotReconfigure();

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellToplevel> shellSurface(Test::create_xdg_shell_toplevel(surface.data()));
    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::red);
    QVERIFY(client);
    QCOMPARE(client->frameGeometry(), QRect(590, 487, 100, 50));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestPlacement::testPlaceUnderMouse()
{
    // This test verifies that Under Mouse placement policy works.

    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", win::policy_to_string(win::placement::under_mouse));
    group.sync();
    workspace()->slotReconfigure();

    KWin::Cursor::setPos(QPoint(200, 300));
    QCOMPARE(KWin::Cursor::pos(), QPoint(200, 300));

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellToplevel> shellSurface(Test::create_xdg_shell_toplevel(surface.data()));
    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::red);
    QVERIFY(client);
    QCOMPARE(client->frameGeometry(), QRect(151, 276, 100, 50));

    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestPlacement::testPlaceRandom()
{
    // This test verifies that Random placement policy works.

    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("Placement", win::policy_to_string(win::placement::random));
    group.sync();
    workspace()->slotReconfigure();

    QScopedPointer<Surface> surface1(Test::createSurface());
    QScopedPointer<XdgShellToplevel> shellSurface1(Test::create_xdg_shell_toplevel(surface1.data()));
    auto client1 = Test::renderAndWaitForShown(surface1.data(), QSize(100, 50), Qt::red);
    QVERIFY(client1);
    QCOMPARE(client1->size(), QSize(100, 50));

    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<XdgShellToplevel> shellSurface2(Test::create_xdg_shell_toplevel(surface2.data()));
    auto client2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client2);
    QVERIFY(client2->pos() != client1->pos());
    QCOMPARE(client2->size(), QSize(100, 50));

    QScopedPointer<Surface> surface3(Test::createSurface());
    QScopedPointer<XdgShellToplevel> shellSurface3(Test::create_xdg_shell_toplevel(surface3.data()));
    auto client3 = Test::renderAndWaitForShown(surface3.data(), QSize(100, 50), Qt::green);
    QVERIFY(client3);
    QVERIFY(client3->pos() != client1->pos());
    QVERIFY(client3->pos() != client2->pos());
    QCOMPARE(client3->size(), QSize(100, 50));

    shellSurface3.reset();
    QVERIFY(Test::waitForWindowDestroyed(client3));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(client2));
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(client1));
}

WAYLANDTEST_MAIN(TestPlacement)
#include "placement_test.moc"
