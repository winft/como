/*
SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>
SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "lib/setup.h"

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationSettings>
#include <QDBusConnection>
#include <Wrapland/Client/appmenu.h>
#include <Wrapland/Client/compositor.h>
#include <Wrapland/Client/connection_thread.h>
#include <Wrapland/Client/output.h>
#include <Wrapland/Client/subsurface.h>
#include <Wrapland/Client/surface.h>
#include <Wrapland/Client/xdg_shell.h>
#include <Wrapland/Client/xdgdecoration.h>
#include <Wrapland/Server/client.h>
#include <Wrapland/Server/display.h>
#include <Wrapland/Server/surface.h>
#include <Wrapland/Server/xdg_decoration.h>
#include <catch2/generators/catch_generators.hpp>
#include <csignal>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace como::detail::test
{

TEST_CASE("xdg-shell window", "[win]")
{
    test::setup setup("xdg-shell-window");
    setup.start();
    setup.set_outputs(2);
    test_outputs_default();
    cursor()->set_pos(QPoint(1280, 512));
    setup_wayland_connection(global_selection::xdg_decoration | global_selection::appmenu);

    SECTION("map unmap map")
    {
        // this test verifies that mapping a previously mapped window works correctly
        QSignalSpy clientAddedSpy(setup.base->mod.space->qobject.get(),
                                  &space::qobject_t::wayland_window_added);
        QVERIFY(clientAddedSpy.isValid());

        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        // now let's render
        render(surface, QSize(100, 50), Qt::blue);

        QVERIFY(clientAddedSpy.isEmpty());
        QVERIFY(clientAddedSpy.wait());

        auto client_id = clientAddedSpy.first().first().value<quint32>();
        auto client = get_wayland_window(setup.base->mod.space->windows_map.at(client_id));
        QVERIFY(client);

        QSignalSpy effectsWindowShownSpy(client->render->effect.get(), &EffectWindow::windowShown);
        QVERIFY(effectsWindowShownSpy.isValid());
        QSignalSpy effectsWindowHiddenSpy(client->render->effect.get(),
                                          &EffectWindow::windowHidden);
        QVERIFY(effectsWindowHiddenSpy.isValid());

        QVERIFY(client->isShown());
        QCOMPARE(client->isHiddenInternal(), false);
        QCOMPARE(client->render_data.ready_for_painting, true);
        QCOMPARE(client->render_data.bit_depth, 32);
        QVERIFY(win::has_alpha(*client));
        QCOMPARE(client->control->icon.name(), QStringLiteral("wayland"));
        QCOMPARE(get_wayland_window(setup.base->mod.space->stacking.active), client);
        QVERIFY(effectsWindowShownSpy.isEmpty());
        QVERIFY(client->isMaximizable());
        QVERIFY(client->isMovable());
        QVERIFY(client->isMovableAcrossScreens());
        QVERIFY(client->isResizable());
        QVERIFY(client->render);
        QVERIFY(client->render->effect);
        QVERIFY(!client->render->effect->internalWindow());
        QCOMPARE(client->meta.internal_id.isNull(), false);
        auto const uuid = client->meta.internal_id;
        QUuid deletedUuid;
        QCOMPARE(deletedUuid.isNull(), true);

        QObject::connect(client->space.qobject.get(),
                         &space::qobject_t::remnant_created,
                         client->qobject.get(),
                         [&setup, &deletedUuid](auto win_id) {
                             auto remnant_win = get_wayland_window(
                                 setup.base->mod.space->windows_map.at(win_id));
                             deletedUuid = remnant_win->meta.internal_id;
                         });

        // now unmap
        QSignalSpy hiddenSpy(client->qobject.get(), &win::window_qobject::windowHidden);
        QVERIFY(hiddenSpy.isValid());
        QSignalSpy windowClosedSpy(client->space.qobject.get(), &space::qobject_t::remnant_created);
        QVERIFY(windowClosedSpy.isValid());
        surface->attachBuffer(Wrapland::Client::Buffer::Ptr());
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);
        QVERIFY(hiddenSpy.wait());
        QCOMPARE(client->render_data.ready_for_painting, true);
        QCOMPARE(client->isHiddenInternal(), true);
        QVERIFY(windowClosedSpy.isEmpty());
        QVERIFY(!setup.base->mod.space->stacking.active);
        QCOMPARE(effectsWindowHiddenSpy.count(), 1);
        QCOMPARE(effectsWindowHiddenSpy.first().first().value<EffectWindow*>(),
                 client->render->effect.get());

        QSignalSpy windowShownSpy(client->qobject.get(), &win::window_qobject::windowShown);
        QVERIFY(windowShownSpy.isValid());
        render(surface, QSize(100, 50), Qt::blue, QImage::Format_RGB32);
        QCOMPARE(clientAddedSpy.count(), 1);
        QVERIFY(windowShownSpy.wait());
        QCOMPARE(windowShownSpy.count(), 1);
        QCOMPARE(clientAddedSpy.count(), 1);
        QCOMPARE(client->render_data.ready_for_painting, true);
        QCOMPARE(client->isHiddenInternal(), false);
        QCOMPARE(client->render_data.bit_depth, 24);
        QVERIFY(!win::has_alpha(*client));
        QCOMPARE(get_wayland_window(setup.base->mod.space->stacking.active), client);
        QCOMPARE(effectsWindowShownSpy.count(), 1);
        QCOMPARE(effectsWindowShownSpy.first().first().value<EffectWindow*>(),
                 client->render->effect.get());

        // let's unmap again
        surface->attachBuffer(Wrapland::Client::Buffer::Ptr());
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);
        QVERIFY(hiddenSpy.wait());
        QCOMPARE(hiddenSpy.count(), 2);
        QCOMPARE(client->render_data.ready_for_painting, true);
        QCOMPARE(client->isHiddenInternal(), true);
        QCOMPARE(client->meta.internal_id, uuid);
        QVERIFY(windowClosedSpy.isEmpty());
        QCOMPARE(effectsWindowHiddenSpy.count(), 2);
        QCOMPARE(effectsWindowHiddenSpy.last().first().value<EffectWindow*>(),
                 client->render->effect.get());

        shellSurface.reset();
        surface.reset();
        QVERIFY(windowClosedSpy.wait());
        QCOMPARE(windowClosedSpy.count(), 1);
        QCOMPARE(effectsWindowHiddenSpy.count(), 2);
        QCOMPARE(deletedUuid.isNull(), false);
        QCOMPARE(deletedUuid, uuid);
    }

    SECTION("transient position afeter remap")
    {
        // this test simulates the situation that a transient window gets reused and the parent
        // window moved between the two usages
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(c);

        // create the Transient window
        Wrapland::Client::xdg_shell_positioner_data pos_data;
        pos_data.is_reactive = true;
        pos_data.size = QSize(50, 40);
        pos_data.anchor.rect = QRect(0, 0, 5, 10);
        pos_data.anchor.edge = Qt::BottomEdge | Qt::RightEdge;
        pos_data.gravity = pos_data.anchor.edge;

        auto transientSurface = create_surface();
        QVERIFY(transientSurface);
        auto transientShellSurface
            = create_xdg_shell_popup(transientSurface, shellSurface, pos_data);
        QVERIFY(transientShellSurface);

        QSignalSpy transient_configure_spy(transientShellSurface.get(),
                                           &Wrapland::Client::XdgShellPopup::configureRequested);
        QVERIFY(transient_configure_spy.isValid());

        auto transient = render_and_wait_for_shown(transientSurface, pos_data.size, Qt::blue);
        QVERIFY(transient);
        QCOMPARE(transient->geo.frame,
                 QRect(c->geo.frame.topLeft() + QPoint(5, 10), QSize(50, 40)));

        // unmap the transient
        QSignalSpy windowHiddenSpy(transient->qobject.get(), &win::window_qobject::windowHidden);
        QVERIFY(windowHiddenSpy.isValid());
        transientSurface->attachBuffer(Wrapland::Client::Buffer::Ptr());
        transientSurface->commit(Wrapland::Client::Surface::CommitFlag::None);
        QVERIFY(windowHiddenSpy.wait());

        // now move the parent surface
        c->setFrameGeometry(c->geo.frame.translated(5, 10));

        // now map the transient again
        QSignalSpy windowShownSpy(transient->qobject.get(), &win::window_qobject::windowShown);
        QVERIFY(windowShownSpy.isValid());
        QVERIFY(transient_configure_spy.wait());
        transientShellSurface->ackConfigure(transient_configure_spy.back().back().value<quint32>());
        render(transientSurface, QSize(50, 40), Qt::blue);
        QVERIFY(windowShownSpy.wait());

        QCOMPARE(transient->geo.frame,
                 QRect(c->geo.frame.topLeft() + QPoint(5, 10), QSize(50, 40)));
    }

    SECTION("window outputs")
    {
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        auto size = QSize(200, 200);

        QSignalSpy outputEnteredSpy(surface.get(), &Wrapland::Client::Surface::outputEntered);
        QSignalSpy outputLeftSpy(surface.get(), &Wrapland::Client::Surface::outputLeft);

        auto c = render_and_wait_for_shown(surface, size, Qt::blue);
        // move to be in the first screen
        c->setFrameGeometry(QRect(QPoint(100, 100), size));
        // we don't don't know where the compositor first placed this window,
        // this might fire, it might not
        outputEnteredSpy.wait(5);
        outputEnteredSpy.clear();

        QCOMPARE(surface->outputs().count(), 1);
        QCOMPARE(surface->outputs().first()->globalPosition(), QPoint(0, 0));

        // move to overlapping both first and second screen
        c->setFrameGeometry(QRect(QPoint(1250, 100), size));
        QVERIFY(outputEnteredSpy.wait());
        QCOMPARE(outputEnteredSpy.count(), 1);
        QCOMPARE(outputLeftSpy.count(), 0);
        QCOMPARE(surface->outputs().count(), 2);
        QVERIFY(surface->outputs()[0] != surface->outputs()[1]);

        // move entirely into second screen
        c->setFrameGeometry(QRect(QPoint(1400, 100), size));
        QVERIFY(outputLeftSpy.wait());
        QCOMPARE(outputEnteredSpy.count(), 1);
        QCOMPARE(outputLeftSpy.count(), 1);
        QCOMPARE(surface->outputs().count(), 1);
        QCOMPARE(surface->outputs().first()->globalPosition(), QPoint(1280, 0));
    }

    SECTION("minimize active window")
    {
        // this test verifies that when minimizing the active window it gets deactivated
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(c);
        QVERIFY(c->control->active);
        QCOMPARE(get_wayland_window(setup.base->mod.space->stacking.active), c);
        QVERIFY(c->wantsInput());
        QVERIFY(win::wants_tab_focus(c));
        QVERIFY(c->isShown());

        win::active_window_minimize(*setup.base->mod.space);
        QVERIFY(!c->isShown());
        QVERIFY(c->wantsInput());
        QVERIFY(win::wants_tab_focus(c));
        QVERIFY(!c->control->active);
        QVERIFY(!setup.base->mod.space->stacking.active);
        QVERIFY(c->control->minimized);

        // unminimize again
        win::set_minimized(c, false);
        QVERIFY(!c->control->minimized);
        QVERIFY(c->control->active);
        QVERIFY(c->wantsInput());
        QVERIFY(win::wants_tab_focus(c));
        QVERIFY(c->isShown());
        QCOMPARE(get_wayland_window(setup.base->mod.space->stacking.active), c);
    }

    SECTION("fullscreen")
    {
        // this test verifies that a window can be properly fullscreened
        auto deco_mode = GENERATE(Wrapland::Client::XdgDecoration::Mode::ClientSide,
                                  Wrapland::Client::XdgDecoration::Mode::ServerSide);

        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly);
        QVERIFY(shellSurface);

        // create deco
        auto deco = get_client().interfaces.xdg_decoration->getToplevelDecoration(
            shellSurface.get(), shellSurface.get());
        QSignalSpy decoSpy(deco, &Wrapland::Client::XdgDecoration::modeChanged);
        QVERIFY(decoSpy.isValid());
        deco->setMode(deco_mode);
        QCOMPARE(deco->mode(), Wrapland::Client::XdgDecoration::Mode::ClientSide);
        init_xdg_shell_toplevel(surface, shellSurface);
        QCOMPARE(deco->mode(), deco_mode);

        QSignalSpy configureRequestedSpy(shellSurface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);
        QVERIFY(configureRequestedSpy.isValid());

        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(c);
        QVERIFY(c->control->active);
        QCOMPARE(win::get_layer(*c), win::layer::normal);
        QVERIFY(!c->control->fullscreen);
        QCOMPARE(win::frame_to_client_size(c, c->geo.size()), QSize(100, 50));
        REQUIRE((win::decoration(c) != nullptr)
                == (deco_mode == Wrapland::Client::XdgDecoration::Mode::ServerSide));
        QCOMPARE(win::client_to_frame_size(c, win::frame_to_client_size(c, c->geo.size())),
                 c->geo.frame.size());

        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 1);

        auto cfgdata = shellSurface->get_configure_data();
        QCOMPARE(cfgdata.size, QSize(100, 50));
        QCOMPARE(cfgdata.bounds, get_output(0)->geometry().size());

        QSignalSpy fullscreenChangedSpy(c->qobject.get(), &win::window_qobject::fullScreenChanged);
        QVERIFY(fullscreenChangedSpy.isValid());
        QSignalSpy geometryChangedSpy(c->qobject.get(),
                                      &win::window_qobject::frame_geometry_changed);
        QVERIFY(geometryChangedSpy.isValid());

        shellSurface->setFullscreen(true);

        // After round-trip the server configures the window to the size of the screen.
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 2);

        cfgdata = shellSurface->get_configure_data();
        QCOMPARE(cfgdata.size, get_output(0)->geometry().size());

        shellSurface->ackConfigure(configureRequestedSpy.back().front().value<quint32>());
        render(surface, cfgdata.size, Qt::red);

        // This is the server-side change.
        QVERIFY(fullscreenChangedSpy.wait());

        QVERIFY(c->control->fullscreen);
        QVERIFY(!win::decoration(c));
        QCOMPARE(win::frame_to_client_size(c, c->geo.size()), get_output(0)->geometry().size());
        QVERIFY(!geometryChangedSpy.isEmpty());

        QVERIFY(c->control->fullscreen);
        QVERIFY(!win::decoration(c));
        QCOMPARE(c->geo.frame, QRect(QPoint(0, 0), cfgdata.size));
        QCOMPARE(win::get_layer(*c), win::layer::active);

        // swap back to normal
        shellSurface->setFullscreen(false);

        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 3);

        cfgdata = shellSurface->get_configure_data();
        QCOMPARE(cfgdata.size, QSize(100, 50));

        shellSurface->ackConfigure(configureRequestedSpy.back().front().value<quint32>());
        render(surface, cfgdata.size, Qt::red);

        QVERIFY(fullscreenChangedSpy.wait());

        QVERIFY(!c->control->fullscreen);
        QCOMPARE(win::get_layer(*c), win::layer::normal);
        REQUIRE((win::decoration(c) != nullptr)
                == (deco_mode == Wrapland::Client::XdgDecoration::Mode::ServerSide));
    }

    SECTION("fullscreen restore")
    {
        // this test verifies that windows created fullscreen can be later properly restored
        auto surface = create_surface();
        QVERIFY(surface);
        auto shell_surface = create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly);
        QVERIFY(shell_surface);
        QSignalSpy configureRequestedSpy(shell_surface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);

        // fullscreen the window
        shell_surface->setFullscreen(true);
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        configureRequestedSpy.wait();
        QCOMPARE(configureRequestedSpy.count(), 1);

        auto cfgdata = shell_surface->get_configure_data();
        QCOMPARE(cfgdata.size, get_output(0)->geometry().size());
        QVERIFY(cfgdata.states & Wrapland::Client::xdg_shell_state::fullscreen);
        shell_surface->ackConfigure(configureRequestedSpy.front().front().toUInt());

        auto c = render_and_wait_for_shown(surface, cfgdata.size, Qt::blue);
        QVERIFY(c);
        QVERIFY(c->control->fullscreen);

        configureRequestedSpy.wait(100);

        QSignalSpy fullscreenChangedSpy(c->qobject.get(), &win::window_qobject::fullScreenChanged);
        QVERIFY(fullscreenChangedSpy.isValid());
        QSignalSpy geometryChangedSpy(c->qobject.get(),
                                      &win::window_qobject::frame_geometry_changed);
        QVERIFY(geometryChangedSpy.isValid());

        // swap back to normal
        configureRequestedSpy.clear();
        shell_surface->setFullscreen(false);

        QVERIFY(configureRequestedSpy.wait());

        cfgdata = shell_surface->get_configure_data();
        QCOMPARE(cfgdata.size, QSize(0, 0));

        for (auto const& sig : configureRequestedSpy) {
            shell_surface->ackConfigure(sig.front().toUInt());
        }

        render(surface, QSize(100, 50), Qt::red);

        QVERIFY(fullscreenChangedSpy.wait());
        QCOMPARE(geometryChangedSpy.count(), 1);
        QVERIFY(!c->control->fullscreen);
        QCOMPARE(c->geo.frame.size(), QSize(100, 50));
    }

    SECTION("user can set fullscreen")
    {
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(c);
        QVERIFY(c->control->active);
        QVERIFY(!c->control->fullscreen);
        QVERIFY(c->userCanSetFullScreen());
    }

    SECTION("user set fullscreen")
    {
        auto send_fs_geo = GENERATE(true, false);

        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly);
        QVERIFY(shellSurface);

        // wait for the initial configure event
        QSignalSpy configureRequestedSpy(shellSurface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);
        QVERIFY(configureRequestedSpy.isValid());
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 1);

        shellSurface->ackConfigure(configureRequestedSpy.back().front().value<quint32>());
        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(c);
        QVERIFY(c->control->active);
        QVERIFY(!c->control->fullscreen);

        // The client gets activated, which gets another configure event. Though that's not relevant
        // to the test
        configureRequestedSpy.wait(10);

        QSignalSpy fullscreenChangedSpy(c->qobject.get(), &win::window_qobject::fullScreenChanged);
        QVERIFY(fullscreenChangedSpy.isValid());

        c->setFullScreen(true);

        QVERIFY(c->geo.update.fullscreen);
        QVERIFY(!c->control->fullscreen);

        QTRY_COMPARE(configureRequestedSpy.count(), 3);

        auto cfgdata = shellSurface->get_configure_data();
        QCOMPARE(cfgdata.size, get_output(0)->geometry().size());
        QVERIFY(cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::fullscreen));
        QVERIFY(cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::activated));
        QVERIFY(!cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::maximized));
        QVERIFY(!cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::resizing));

        shellSurface->ackConfigure(configureRequestedSpy.at(2).front().value<quint32>());

        QSignalSpy geometry_spy(c->qobject.get(), &win::window_qobject::frame_geometry_changed);
        QVERIFY(geometry_spy.isValid());

        if (send_fs_geo) {
            render(surface, get_output(0)->geometry().size(), Qt::green);
        }

        QCOMPARE(geometry_spy.wait(100), send_fs_geo);
        QCOMPARE(fullscreenChangedSpy.count(), send_fs_geo);
        QCOMPARE(c->control->fullscreen, send_fs_geo);

        configureRequestedSpy.clear();

        // unset fullscreen again
        c->setFullScreen(false);

        QVERIFY(!c->geo.update.fullscreen);
        QCOMPARE(c->control->fullscreen, send_fs_geo);

        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 1);

        cfgdata = shellSurface->get_configure_data();
        QCOMPARE(cfgdata.size, QSize(100, 50));
        QVERIFY(!cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::fullscreen));

        shellSurface->ackConfigure(configureRequestedSpy.front().front().value<quint32>());

        render(surface, cfgdata.size, Qt::red);
        QCOMPARE(geometry_spy.wait(100), send_fs_geo);

        REQUIRE(fullscreenChangedSpy.count() == (send_fs_geo ? 2 : 0));
        QVERIFY(!c->control->fullscreen);
    }

// TODO(romangg): Fails since recently due to upstream change. Reenable after investigation.
#if 0
    SECTION("maximized to fullscreen")
    {
        // this test verifies that a window can be properly fullscreened after maximizing

        auto deco_mode = GENERATE(XdgDecoration::Mode::ClientSide, XdgDecoration::Mode::ServerSide);

        std::unique_ptr<Wrapland::Client::Surface> surface(create_surface());
        std::unique_ptr<Wrapland::Client::XdgShellToplevel> shellSurface(
            create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly));
        QVERIFY(shellSurface);

        // create deco
        auto deco = get_client().interfaces.xdg_decoration->getToplevelDecoration(
            shellSurface.get(), shellSurface.get());
        QSignalSpy decoSpy(deco, &XdgDecoration::modeChanged);
        QVERIFY(decoSpy.isValid());
        deco->setMode(deco_mode);
        QCOMPARE(deco->mode(), XdgDecoration::Mode::ClientSide);
        init_xdg_shell_toplevel(surface, shellSurface);
        QCOMPARE(deco->mode(), deco_mode);

        auto const has_ssd = deco_mode == XdgDecoration::Mode::ServerSide;

        auto client = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(client);
        QVERIFY(client->control->active);
        QVERIFY(!client->control->fullscreen);
        QCOMPARE(win::frame_to_client_size(client, client->geo.size()), QSize(100, 50));
        REQUIRE((win::decoration(client) != nullptr) == has_ssd);

        QSignalSpy fullscreenChangedSpy(client->qobject.get(),
                                        &win::window_qobject::fullScreenChanged);
        QVERIFY(fullscreenChangedSpy.isValid());
        QSignalSpy geometryChangedSpy(client->qobject.get(),
                                      &win::window_qobject::frame_geometry_changed);
        QVERIFY(geometryChangedSpy.isValid());
        QSignalSpy configureRequestedSpy(shellSurface.get(), &XdgShellToplevel::configured);
        QVERIFY(configureRequestedSpy.isValid());

        QSignalSpy maximize_spy(client->qobject.get(), &win::window_qobject::maximize_mode_changed);

        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 1);

        auto cfgdata = shellSurface->get_configure_data();
        QVERIFY(cfgdata.updates.testFlag(Wrapland::Client::xdg_shell_toplevel_configure_change::size));

        shellSurface->setMaximized(true);
        shellSurface->ackConfigure(configureRequestedSpy.back().front().value<quint32>());

        render(surface, cfgdata.size, Qt::red);

        QVERIFY(configureRequestedSpy.wait());

        cfgdata = shellSurface->get_configure_data();
        shellSurface->ackConfigure(configureRequestedSpy.back().front().value<quint32>());
        render(surface, cfgdata.size, Qt::red);

        maximize_spy.wait();

        QCOMPARE(client->maximizeMode(), win::maximize_mode::full);
        QCOMPARE(geometryChangedSpy.isEmpty(), false);
        geometryChangedSpy.clear();
        configureRequestedSpy.clear();

        // Fullscreen the window client-side.
        shellSurface->setFullscreen(true);

        // Server sends a configure request with or without SSD so client can adapt window geometry.
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 1);
        cfgdata = shellSurface->get_configure_data();

        // With or without deco on fullscreen clients will be requested to provide the screeen size.
        QCOMPARE(cfgdata.size, get_output(0)->geometry().size());

        shellSurface->ackConfigure(configureRequestedSpy.back().front().value<quint32>());
        render(surface, cfgdata.size, Qt::red);

        // Receive request server-side.
        QVERIFY(fullscreenChangedSpy.wait());
        QVERIFY(client->control->fullscreen);

        QVERIFY(client->control->fullscreen);
        QVERIFY(!win::decoration(client));
        QCOMPARE(client->geo.frame, QRect(QPoint(0, 0), cfgdata.size));

        QCOMPARE(configureRequestedSpy.count(), 1);
        cfgdata = shellSurface->get_configure_data();

        // swap back to normal
        shellSurface->setFullscreen(false);
        shellSurface->setMaximized(false);

        QVERIFY(configureRequestedSpy.wait());
        QTRY_COMPARE(configureRequestedSpy.count(), 3);
        cfgdata = shellSurface->get_configure_data();

        if (deco_mode == XdgDecoration::Mode::ServerSide) {
            QCOMPARE(cfgdata.size, QSize(100, 50));
        }

        shellSurface->ackConfigure(configureRequestedSpy.back().front().value<quint32>());
        render(surface, cfgdata.size, Qt::red);

        QVERIFY(fullscreenChangedSpy.wait());

        QVERIFY(!client->control->fullscreen);
        REQUIRE((win::decoration(client) != nullptr)
                == (deco_mode == XdgDecoration::Mode::ServerSide));
    }
#endif

    SECTION("window opens larger than screen")
    {
        // this test creates a window which is as large as the screen, but is decorated
        // the window should get resized to fit into the screen, BUG: 366632
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly);
        QVERIFY(shellSurface);

        QSignalSpy configureRequestedSpy(shellSurface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);
        QVERIFY(configureRequestedSpy.isValid());

        // create deco
        auto deco = get_client().interfaces.xdg_decoration->getToplevelDecoration(
            shellSurface.get(), shellSurface.get());
        QSignalSpy decoSpy(deco, &Wrapland::Client::XdgDecoration::modeChanged);
        QVERIFY(decoSpy.isValid());
        deco->setMode(Wrapland::Client::XdgDecoration::Mode::ServerSide);
        QCOMPARE(deco->mode(), Wrapland::Client::XdgDecoration::Mode::ClientSide);
        init_xdg_shell_toplevel(surface, shellSurface);
        QCOMPARE(deco->mode(), Wrapland::Client::XdgDecoration::Mode::ServerSide);

        auto c = render_and_wait_for_shown(surface, get_output(0)->geometry().size(), Qt::blue);
        QVERIFY(c);
        QVERIFY(c->control->active);
        QCOMPARE(win::frame_to_client_size(c, c->geo.size()), get_output(0)->geometry().size());
        QVERIFY(win::decoration(c));

        QVERIFY(configureRequestedSpy.wait());
        QVERIFY(shellSurface->get_configure_data().updates.testFlag(
            Wrapland::Client::xdg_shell_toplevel_configure_change::size));
    }

    SECTION("hidden")
    {
        // this test verifies that when hiding window it doesn't get shown
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(c);
        QVERIFY(c->control->active);
        QCOMPARE(get_wayland_window(setup.base->mod.space->stacking.active), c);
        QVERIFY(c->wantsInput());
        QVERIFY(win::wants_tab_focus(c));
        QVERIFY(c->isShown());

        c->hideClient(true);
        QVERIFY(!c->isShown());
        QVERIFY(!c->control->active);
        QVERIFY(c->wantsInput());
        QVERIFY(win::wants_tab_focus(c));

        // unhide again
        c->hideClient(false);
        QVERIFY(c->isShown());
        QVERIFY(c->wantsInput());
        QVERIFY(win::wants_tab_focus(c));

        // QCOMPARE(setup.base->mod.space->stacking.active, c);
    }

    SECTION("desktop file name")
    {
        QIcon::setThemeName(QStringLiteral("breeze"));
        // this test verifies that desktop file name is passed correctly to the window
        auto surface = create_surface();
        QVERIFY(surface);

        // only xdg-shell as ShellSurface misses the setter
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);
        shellSurface->setAppId(QByteArrayLiteral("org.kde.foo"));

        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(c);
        QCOMPARE(c->control->desktop_file_name, QByteArrayLiteral("org.kde.foo"));
        QCOMPARE(c->meta.wm_class.res_class, QByteArrayLiteral("org.kde.foo"));
        QVERIFY(c->meta.wm_class.res_name.startsWith("tests"));

        // the desktop file does not exist, so icon should be generic Wayland
        QCOMPARE(c->control->icon.name(), QStringLiteral("wayland"));

        QSignalSpy desktopFileNameChangedSpy(c->qobject.get(),
                                             &win::window_qobject::desktopFileNameChanged);
        QVERIFY(desktopFileNameChangedSpy.isValid());
        QSignalSpy iconChangedSpy(c->qobject.get(), &win::window_qobject::iconChanged);
        QVERIFY(iconChangedSpy.isValid());
        shellSurface->setAppId(QByteArrayLiteral("org.kde.bar"));
        QVERIFY(desktopFileNameChangedSpy.wait());
        QCOMPARE(c->control->desktop_file_name, QByteArrayLiteral("org.kde.bar"));
        QCOMPARE(c->meta.wm_class.res_class, QByteArrayLiteral("org.kde.bar"));
        QVERIFY(c->meta.wm_class.res_name.startsWith("tests"));

        // icon should still be wayland
        QCOMPARE(c->control->icon.name(), QStringLiteral("wayland"));
        QVERIFY(iconChangedSpy.isEmpty());

        QString const dfPath = QFINDTESTDATA("data/example.desktop");
        shellSurface->setAppId(dfPath.toUtf8());
        QVERIFY(desktopFileNameChangedSpy.wait());
        QCOMPARE(iconChangedSpy.count(), 1);
        QCOMPARE(QString::fromUtf8(c->control->desktop_file_name), dfPath);
        QCOMPARE(c->control->icon.name(), QStringLiteral("kwin"));
    }

    SECTION("caption simplified")
    {
        // this test verifies that caption is properly trimmed, see BUG 323798 comment #12
        auto surface = create_surface();
        QVERIFY(surface);

        // only done for xdg-shell as ShellSurface misses the setter
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        const QString origTitle = QString::fromUtf8(QByteArrayLiteral(
            "Was tun, wenn Schüler Autismus haben?\342\200\250\342\200\250\342\200\250 – Marlies "
            "Hübner - Mozilla Firefox"));
        shellSurface->setTitle(origTitle);
        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(c);
        QVERIFY(win::caption(c) != origTitle);
        QCOMPARE(win::caption(c), origTitle.simplified());
    }

    SECTION("unresponsive window")
    {
        // this test verifies that killWindow properly terminates a process
        // for this an external binary is launched

        auto socket_mode = GENERATE(false, true);

        QString const kill = QFINDTESTDATA(QStringLiteral("kill"));
        QVERIFY(!kill.isEmpty());
        QSignalSpy shellClientAddedSpy(setup.base->mod.space->qobject.get(),
                                       &space::qobject_t::wayland_window_added);
        QVERIFY(shellClientAddedSpy.isValid());

        std::unique_ptr<QProcess> process(new QProcess);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

        env.insert("QT_WAYLAND_SHELL_INTEGRATION", "xdg-shell");

        if (socket_mode) {
            int sx[2];
            QVERIFY(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) >= 0);
            setup.base->server->display->createClient(sx[0]);
            int socket = dup(sx[1]);
            QVERIFY(socket != -1);
            env.insert(QStringLiteral("WAYLAND_SOCKET"), QByteArray::number(socket));
            env.remove("WAYLAND_DISPLAY");
        }

        process->setProcessEnvironment(env);
        process->setProcessChannelMode(QProcess::ForwardedChannels);
        process->setProgram(kill);
        QSignalSpy processStartedSpy{process.get(), &QProcess::started};
        QVERIFY(processStartedSpy.isValid());
        process->start();

        QVERIFY(shellClientAddedSpy.wait());
        QCOMPARE(processStartedSpy.count(), 1);
        QCOMPARE(shellClientAddedSpy.count(), 1);

        ::kill(process->processId(), SIGUSR1); // send a signal to freeze the process

        auto kill_client_id = shellClientAddedSpy.first().first().value<quint32>();
        auto killClient = get_wayland_window(setup.base->mod.space->windows_map.at(kill_client_id));
        QVERIFY(killClient);
        QSignalSpy unresponsiveSpy(killClient->qobject.get(),
                                   &win::window_qobject::unresponsiveChanged);
        QSignalSpy killedSpy(
            process.get(),
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished));
        QSignalSpy deletedSpy(killClient->qobject.get(), &QObject::destroyed);

        qint64 startTime = QDateTime::currentMSecsSinceEpoch();

        // wait for the process to be frozen
        QTest::qWait(10);

        // pretend the user clicked the close button
        killClient->closeWindow();

        // client should not yet be marked unresponsive nor killed
        QVERIFY(!killClient->control->unresponsive);
        QVERIFY(killedSpy.isEmpty());

        QVERIFY(unresponsiveSpy.wait());
        // client should be marked unresponsive but not killed
        auto elapsed1 = QDateTime::currentMSecsSinceEpoch() - startTime;

        // ping timer is 1s, but coarse timers on a test across two processes means we need a fuzzy
        // compare
        REQUIRE(elapsed1 > 900);
        REQUIRE(elapsed1 < 1200);
        QVERIFY(killClient->control->unresponsive);
        QVERIFY(killedSpy.isEmpty());

        QVERIFY(deletedSpy.wait());
        if (!socket_mode) {
            // process was killed - because we're across process this could happen in either order
            TRY_REQUIRE(killedSpy.count());
        }

        auto elapsed2 = QDateTime::currentMSecsSinceEpoch() - startTime;
        QVERIFY(elapsed2 > 1800); // second ping comes in a second later
    }

    SECTION("app menu")
    {
        // register a faux appmenu client
        QVERIFY(QDBusConnection::sessionBus().registerService("org.kde.kappmenu"));

        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(c);
        std::unique_ptr<Wrapland::Client::AppMenu> menu(
            get_client().interfaces.app_menu->create(surface.get()));
        QSignalSpy spy(c->qobject.get(), &win::window_qobject::hasApplicationMenuChanged);
        menu->setAddress("service.name", "object/path");
        spy.wait();
        QCOMPARE(c->control->has_application_menu(), true);
        QCOMPARE(c->control->appmenu.address, win::appmenu_address("service.name", "object/path"));

        QVERIFY(QDBusConnection::sessionBus().unregisterService("org.kde.kappmenu"));
    }

    SECTION("no deco mode requested")
    {
        // this test verifies that the decoration follows the default mode if no mode is explicitly
        // requested
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly);
        QVERIFY(shellSurface);

        auto deco = get_client().interfaces.xdg_decoration->getToplevelDecoration(
            shellSurface.get(), shellSurface.get());
        QSignalSpy decoSpy(deco, &Wrapland::Client::XdgDecoration::modeChanged);
        QVERIFY(decoSpy.isValid());
        deco->unsetMode();
        QCOMPARE(deco->mode(), Wrapland::Client::XdgDecoration::Mode::ClientSide);
        init_xdg_shell_toplevel(surface, shellSurface);
        QCOMPARE(decoSpy.count(), 1);
        QCOMPARE(deco->mode(), Wrapland::Client::XdgDecoration::Mode::ServerSide);

        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(c);
        QCOMPARE(c->noBorder(), false);
        QVERIFY(win::decoration(c));
    }

    SECTION("send window with transient to subspace")
    {
        // this test verifies that when sending a client to a subspace all transients are also send
        // to that subspace

        win::subspace_manager_set_count(*setup.base->mod.space->subspace_manager, 2);
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(c);

        // let's create a transient window
        auto transientSurface = create_surface();
        QVERIFY(transientSurface);
        auto transientShellSurface = create_xdg_shell_toplevel(transientSurface);
        QVERIFY(transientShellSurface);
        transientShellSurface->setTransientFor(shellSurface.get());

        auto transient = render_and_wait_for_shown(transientSurface, QSize(100, 50), Qt::blue);
        QVERIFY(transient);
        QCOMPARE(get_wayland_window(setup.base->mod.space->stacking.active), transient);
        QCOMPARE(transient->transient->lead(), c);
        QVERIFY(contains(c->transient->children, transient));

        QCOMPARE(win::get_subspace(*c), 1);
        QVERIFY(!win::on_all_subspaces(*c));
        QCOMPARE(win::get_subspace(*transient), 1);
        QVERIFY(!win::on_all_subspaces(*transient));
        win::active_window_to_subspace(*setup.base->mod.space, 2);

        QCOMPARE(win::get_subspace(*c), 1);
        QCOMPARE(win::get_subspace(*transient), 2);

        // activate c
        win::activate_window(*setup.base->mod.space, *c);
        QCOMPARE(get_wayland_window(setup.base->mod.space->stacking.active), c);
        QVERIFY(c->control->active);

        // and send it to the subspace it's already on
        QCOMPARE(win::get_subspace(*c), 1);
        QCOMPARE(win::get_subspace(*transient), 2);
        win::active_window_to_subspace(*setup.base->mod.space, 1);

        // which should move the transient back to the subspace
        QCOMPARE(win::get_subspace(*c), 1);
        QCOMPARE(win::get_subspace(*transient), 1);
    }

    SECTION("minimize window with transients")
    {
        // this test verifies that when minimizing/unminimizing a window all its
        // transients will be minimized/unminimized as well

        // create the main window
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);
        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(c);
        QVERIFY(!c->control->minimized);

        // create a transient window
        auto transientSurface = create_surface();
        QVERIFY(transientSurface);
        auto transientShellSurface = create_xdg_shell_toplevel(transientSurface);
        QVERIFY(transientShellSurface);
        transientShellSurface->setTransientFor(shellSurface.get());
        auto transient = render_and_wait_for_shown(transientSurface, QSize(100, 50), Qt::red);
        QVERIFY(transient);
        QVERIFY(!transient->control->minimized);
        QCOMPARE(transient->transient->lead(), c);
        QVERIFY(contains(c->transient->children, transient));

        // minimize the main window, the transient should be minimized as well
        win::set_minimized(c, true);
        QVERIFY(c->control->minimized);
        QVERIFY(transient->control->minimized);

        // unminimize the main window, the transient should be unminimized as well
        win::set_minimized(c, false);
        QVERIFY(!c->control->minimized);
        QVERIFY(!transient->control->minimized);
    }

    SECTION("xdg-deco")
    {
        auto deco_mode = GENERATE(Wrapland::Client::XdgDecoration::Mode::ClientSide,
                                  Wrapland::Client::XdgDecoration::Mode::ServerSide);

        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly);
        QVERIFY(shellSurface);
        std::unique_ptr<Wrapland::Client::XdgDecoration> deco(
            get_client().interfaces.xdg_decoration->getToplevelDecoration(shellSurface.get()));

        QSignalSpy decorationConfiguredSpy(deco.get(),
                                           &Wrapland::Client::XdgDecoration::modeChanged);
        QSignalSpy configureRequestedSpy(shellSurface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);

        deco->setMode(deco_mode);

        init_xdg_shell_toplevel(surface, shellSurface);

        QCOMPARE(configureRequestedSpy.count(), 1);
        QCOMPARE(decorationConfiguredSpy.count(), 1);
        QCOMPARE(decorationConfiguredSpy.first()[0].value<Wrapland::Client::XdgDecoration::Mode>(),
                 deco_mode);

        shellSurface->ackConfigure(configureRequestedSpy.back().front().toInt());

        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        REQUIRE(c->userCanSetNoBorder()
                == (deco_mode == Wrapland::Client::XdgDecoration::Mode::ServerSide));
        REQUIRE((win::decoration(c) != nullptr)
                == (deco_mode == Wrapland::Client::XdgDecoration::Mode::ServerSide));
    }

    SECTION("never committed")
    {
        // check we don't crash if we create a shell object but delete the XdgShellClient before
        // committing it
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly);
        QVERIFY(shellSurface);
    }

    SECTION("initial state")
    {
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly);
        QVERIFY(shellSurface);
        QSignalSpy configureRequestedSpy(shellSurface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);
        QVERIFY(configureRequestedSpy.isValid());
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        configureRequestedSpy.wait();

        QCOMPARE(configureRequestedSpy.count(), 1);

        auto cfgdata = shellSurface->get_configure_data();

        // client should chose it's preferred size
        QCOMPARE(cfgdata.size, QSize(0, 0));
        QCOMPARE(cfgdata.bounds, get_output(0)->geometry().size());

        shellSurface->ackConfigure(configureRequestedSpy.front().front().toUInt());

        auto c = render_and_wait_for_shown(surface, QSize(200, 100), Qt::blue);
        QCOMPARE(c->geo.size(), QSize(200, 100));
    }

    SECTION("initially maximized")
    {
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly);
        QVERIFY(shellSurface);
        QSignalSpy configureRequestedSpy(shellSurface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);

        shellSurface->setMaximized(true);
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        configureRequestedSpy.wait();

        QCOMPARE(configureRequestedSpy.count(), 1);

        auto cfgdata = shellSurface->get_configure_data();
        QCOMPARE(cfgdata.size, QSize(1280, 1024));
        REQUIRE(!(cfgdata.states & Wrapland::Client::xdg_shell_state::activated));
        QVERIFY(cfgdata.states & Wrapland::Client::xdg_shell_state::maximized);

        shellSurface->ackConfigure(configureRequestedSpy.front().front().toUInt());

        auto c = render_and_wait_for_shown(surface, cfgdata.size, Qt::blue);
        QCOMPARE(c->maximizeMode(), win::maximize_mode::full);
        QCOMPARE(c->geo.size(), QSize(1280, 1024));

        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 2);

        cfgdata = shellSurface->get_configure_data();
        QVERIFY(cfgdata.states & Wrapland::Client::xdg_shell_state::activated);
        QVERIFY(cfgdata.states & Wrapland::Client::xdg_shell_state::maximized);

        // Unmaximize again, an empty size is returned, that means the client should decide.
        win::active_window_maximize(*setup.base->mod.space);
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 3);

        cfgdata = shellSurface->get_configure_data();
        QVERIFY(cfgdata.size.isEmpty());
    }

    SECTION("initially fullscreen")
    {
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly);
        QVERIFY(shellSurface);
        QSignalSpy configureRequestedSpy(shellSurface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);

        shellSurface->setFullscreen(true);
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        configureRequestedSpy.wait();
        QCOMPARE(configureRequestedSpy.count(), 1);

        auto cfgdata = shellSurface->get_configure_data();
        QCOMPARE(cfgdata.size, QSize(1280, 1024));
        QVERIFY(cfgdata.states & Wrapland::Client::xdg_shell_state::fullscreen);

        shellSurface->ackConfigure(configureRequestedSpy.front().front().toUInt());

        auto c = render_and_wait_for_shown(surface, cfgdata.size, Qt::blue);
        QCOMPARE(c->control->fullscreen, true);
        QCOMPARE(c->geo.size(), QSize(1280, 1024));
    }

    SECTION("initially minimized")
    {
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly);
        QVERIFY(shellSurface);
        QSignalSpy configureRequestedSpy(shellSurface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);

        shellSurface->requestMinimize();
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        configureRequestedSpy.wait();
        QCOMPARE(configureRequestedSpy.count(), 1);

        auto cfgdata = shellSurface->get_configure_data();
        QCOMPARE(cfgdata.size, QSize(0, 0));
        QCOMPARE(cfgdata.states, 0);

        shellSurface->ackConfigure(configureRequestedSpy.front().front().toUInt());

        auto c
            = render_and_wait_for_shown(surface, cfgdata.size, Qt::blue, QImage::Format_ARGB32, 10);
        // TODO(romangg): Client created in a minimised state is not exposed to kwin bug 404838
        REQUIRE_FALSE(c);
        return;
        QVERIFY(c->control->minimized);
    }

    SECTION("window geo isnt set")
    {
        // This test verifies that the effective window geometry corresponds to the
        // bounding rectangle of the main surface and its sub-surfaces if no window
        // geometry is set by the client.
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        auto client = render_and_wait_for_shown(surface, QSize(200, 100), Qt::red);
        QVERIFY(client);
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));
        QCOMPARE(client->geo.frame.size(), QSize(200, 100));

        const QPoint oldPosition = client->geo.pos();

        QSignalSpy geometryChangedSpy(client->qobject.get(),
                                      &win::window_qobject::frame_geometry_changed);
        QVERIFY(geometryChangedSpy.isValid());
        render(surface, QSize(100, 50), Qt::blue);
        QVERIFY(geometryChangedSpy.wait());
        QCOMPARE(client->geo.frame.topLeft(), oldPosition);
        QCOMPARE(client->geo.frame.size(), QSize(100, 50));
        QCOMPARE(win::render_geometry(client).topLeft(), oldPosition);
        QCOMPARE(win::render_geometry(client).size(), QSize(100, 50));

        auto childSurface = create_surface();
        QVERIFY(childSurface);
        auto subSurface = create_subsurface(childSurface, surface);
        QVERIFY(subSurface);

        subSurface->setPosition(QPoint(-20, -10));
        render(childSurface, QSize(100, 50), Qt::blue);
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        QVERIFY(geometryChangedSpy.wait());
        QCOMPARE(client->geo.frame.topLeft(), oldPosition);
        QCOMPARE(client->geo.frame.size(), QSize(120, 60));
        QCOMPARE(win::render_geometry(client).topLeft(), oldPosition + QPoint(20, 10));
        QCOMPARE(win::render_geometry(client).size(), QSize(100, 50));
    }

    SECTION("window geo attach buffer")
    {
        // This test verifies that the effective window geometry remains the same when
        // a new buffer is attached and xdg_surface.set_window_geometry is not called
        // again. Notice that the window geometry must remain the same even if the new
        // buffer is smaller.
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        auto client = render_and_wait_for_shown(surface, QSize(200, 100), Qt::red);
        QVERIFY(client);
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));
        QCOMPARE(client->geo.frame.size(), QSize(200, 100));

        client->setFrameGeometry(QRect(QPoint(100, 100), client->geo.size()));

        auto const first_pos = client->geo.pos();
        auto const first_win_geo = QRect(10, 10, 180, 80);
        auto const second_win_geo = QRect(5, 5, 90, 40);

        QSignalSpy geometryChangedSpy(client->qobject.get(),
                                      &win::window_qobject::frame_geometry_changed);
        QVERIFY(geometryChangedSpy.isValid());

        shellSurface->setWindowGeometry(first_win_geo);
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);
        QVERIFY(geometryChangedSpy.wait());

        QCOMPARE(client->geo.frame.topLeft(), first_pos);
        QCOMPARE(client->geo.frame.size(), QSize(180, 80));
        QCOMPARE(win::render_geometry(client).topLeft(), first_pos - QPoint(10, 10));
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));

        // Now reduce the size from 200x100 to 100x50.
        render(surface, QSize(100, 50), Qt::blue);
        QVERIFY(geometryChangedSpy.wait());
        QCOMPARE(client->geo.frame.topLeft(), first_pos);

        QCOMPARE(client->geo.frame.size(),
                 first_win_geo.intersected(client->surface->expanse()).size());
        QCOMPARE(client->geo.frame.size(), QSize(90, 40));
        QCOMPARE(win::render_geometry(client).topLeft(), first_pos - QPoint(10, 10));
        QCOMPARE(win::render_geometry(client).size(), QSize(100, 50));

        shellSurface->setWindowGeometry(second_win_geo);
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        // The frame geometry must stay the same.
        QVERIFY(!geometryChangedSpy.wait(200));
        QCOMPARE(client->geo.frame.topLeft(), first_pos);
        QCOMPARE(client->geo.frame.size(), QSize(90, 40));
        QCOMPARE(win::render_geometry(client).topLeft(), first_pos - QPoint(5, 5));
        QCOMPARE(win::render_geometry(client).size(), QSize(100, 50));

        shellSurface.reset();
        QVERIFY(wait_for_destroyed(client));
    }

    SECTION("send to screen")
    {
        // This test verifies that we can send xdg-shell toplevels and popups to other screens.
        auto surface = create_surface();
        QVERIFY(surface);
        auto shell_surface = create_xdg_shell_toplevel(surface);
        QVERIFY(shell_surface);

        auto window = render_and_wait_for_shown(surface, QSize(200, 100), Qt::red);
        QVERIFY(window);
        QCOMPARE(get_wayland_window(setup.base->mod.space->stacking.active), window);
        QCOMPARE(window->geo.frame.size(), QSize(200, 100));

        Wrapland::Client::xdg_shell_positioner_data pos_data;
        pos_data.is_reactive = true;
        pos_data.size = QSize(50, 40);
        pos_data.anchor.rect = QRect(0, 0, 5, 10);
        pos_data.anchor.edge = Qt::BottomEdge | Qt::RightEdge;
        pos_data.gravity = pos_data.anchor.edge;

        auto popup_surface = create_surface();
        QVERIFY(popup_surface);
        auto popup_shell_surface = create_xdg_shell_popup(popup_surface, shell_surface, pos_data);
        QVERIFY(popup_shell_surface);

        QSignalSpy popup_configure_spy(popup_shell_surface.get(),
                                       &Wrapland::Client::XdgShellPopup::configureRequested);
        QVERIFY(popup_configure_spy.isValid());

        auto popup = render_and_wait_for_shown(popup_surface, pos_data.size, Qt::blue);
        QVERIFY(popup);
        QCOMPARE(popup->geo.frame,
                 QRect(window->geo.frame.topLeft() + QPoint(5, 10), QSize(50, 40)));

        QSignalSpy popup_geo_spy(popup->qobject.get(),
                                 &win::window_qobject::frame_geometry_changed);
        QVERIFY(popup_geo_spy.isValid());

        auto const& outputs = setup.base->outputs;
        QCOMPARE(window->topo.central_output, outputs.at(0));
        QCOMPARE(popup->topo.central_output, outputs.at(0));

        auto output = base::get_output(outputs, 1);
        QVERIFY(output);
        win::send_to_screen(*setup.base->mod.space, window, *output);
        QCOMPARE(window->topo.central_output, outputs.at(1));
        QCOMPARE(popup->topo.central_output, outputs.at(0));

        QVERIFY(popup_configure_spy.wait());
        popup_shell_surface->ackConfigure(popup_configure_spy.back().back().value<quint32>());
        popup_surface->commit();

        QVERIFY(popup_geo_spy.wait());
        QCOMPARE(popup->geo.frame,
                 QRect(window->geo.frame.topLeft() + QPoint(5, 10), QSize(50, 40)));
        QCOMPARE(popup->topo.central_output, outputs.at(1));
    }

    SECTION("window geo attach subsurface")
    {
        // This test verifies that the effective window geometry remains the same
        // when a new sub-surface is added and xdg_surface.set_window_geometry is
        // not called again.
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        auto client = render_and_wait_for_shown(surface, QSize(200, 100), Qt::red);
        QVERIFY(client);
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));
        QCOMPARE(client->geo.frame.size(), QSize(200, 100));

        client->setFrameGeometry(QRect(QPoint(100, 100), client->geo.size()));

        auto const first_pos = client->geo.pos();
        auto const first_win_geo = QRect(10, 10, 180, 80);
        auto const second_win_geo = QRect(-15, -15, 50, 40);
        auto const subsurface_offset = QPoint(-20, -20);

        QSignalSpy geometryChangedSpy(client->qobject.get(),
                                      &win::window_qobject::frame_geometry_changed);
        QVERIFY(geometryChangedSpy.isValid());

        shellSurface->setWindowGeometry(first_win_geo);
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);
        QVERIFY(geometryChangedSpy.wait());

        QCOMPARE(client->geo.frame.topLeft(), first_pos);
        QCOMPARE(client->geo.frame.size(), QSize(180, 80));
        QCOMPARE(win::render_geometry(client).topLeft(), first_pos - QPoint(10, 10));
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));

        auto childSurface = create_surface();
        QVERIFY(childSurface);
        auto subSurface = create_subsurface(childSurface, surface);
        QVERIFY(subSurface);

        subSurface->setPosition(subsurface_offset);
        render(childSurface, QSize(100, 50), Qt::blue);
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        QCOMPARE(client->geo.frame.topLeft(), first_pos);
        QCOMPARE(client->geo.frame.size(), QSize(180, 80));
        QCOMPARE(win::render_geometry(client).topLeft(), first_pos - QPoint(10, 10));
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));

        shellSurface->setWindowGeometry(second_win_geo);
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);
        QVERIFY(geometryChangedSpy.wait());

        // TODO: Is the buffer relative to the main surface's top-left corner or to the union of it
        // with all subsurfaces?

        QCOMPARE(client->geo.frame.topLeft(), first_pos);
        QCOMPARE(client->geo.frame.size(), QSize(50, 40));
        QCOMPARE(win::render_geometry(client).topLeft(), first_pos - QPoint(-15, -15));
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));
    }

    SECTION("window geo interactive resize")
    {
        // This test verifies that correct window geometry is provided along each
        // configure event when an xdg-shell is being interactively resized.
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        auto client = render_and_wait_for_shown(surface, QSize(200, 100), Qt::red);
        QVERIFY(client);
        QVERIFY(client->control->active);
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));
        QCOMPARE(client->geo.frame.size(), QSize(200, 100));

        QSignalSpy configureRequestedSpy(shellSurface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);
        QVERIFY(configureRequestedSpy.isValid());
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 1);

        QSignalSpy geometryChangedSpy(client->qobject.get(),
                                      &win::window_qobject::frame_geometry_changed);
        QVERIFY(geometryChangedSpy.isValid());
        shellSurface->setWindowGeometry(QRect(10, 10, 180, 80));
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);
        QVERIFY(geometryChangedSpy.wait());
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));
        QCOMPARE(client->geo.frame.size(), QSize(180, 80));

        QSignalSpy clientStartMoveResizedSpy(client->qobject.get(),
                                             &win::window_qobject::clientStartUserMovedResized);
        QVERIFY(clientStartMoveResizedSpy.isValid());
        QSignalSpy clientStepUserMovedResizedSpy(client->qobject.get(),
                                                 &win::window_qobject::clientStepUserMovedResized);
        QVERIFY(clientStepUserMovedResizedSpy.isValid());
        QSignalSpy clientFinishUserMovedResizedSpy(
            client->qobject.get(), &win::window_qobject::clientFinishUserMovedResized);
        QVERIFY(clientFinishUserMovedResizedSpy.isValid());

        // Start interactively resizing the client.
        QVERIFY(!setup.base->mod.space->move_resize_window);
        win::active_window_resize(*setup.base->mod.space);
        QCOMPARE(get_wayland_window(setup.base->mod.space->move_resize_window), client);
        QCOMPARE(clientStartMoveResizedSpy.count(), 1);
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 2);

        auto cfgdata = shellSurface->get_configure_data();
        QVERIFY(cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::resizing));

        // Go right.
        auto cursorPos = cursor()->pos();
        win::key_press_event(client, Qt::Key_Right);
        win::update_move_resize(client, cursor()->pos());
        QCOMPARE(cursor()->pos(), cursorPos + QPoint(8, 0));

        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 3);

        cfgdata = shellSurface->get_configure_data();
        QVERIFY(cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::resizing));
        QCOMPARE(cfgdata.size, QSize(188, 80));

        shellSurface->setWindowGeometry(QRect(10, 10, 188, 80));
        shellSurface->ackConfigure(configureRequestedSpy.back().front().value<quint32>());
        render(surface, QSize(208, 100), Qt::blue);

        QVERIFY(geometryChangedSpy.wait());
        QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
        QCOMPARE(win::render_geometry(client).size(), QSize(208, 100));
        QCOMPARE(client->geo.frame.size(), QSize(188, 80));

        // Go down.
        cursorPos = cursor()->pos();
        win::key_press_event(client, Qt::Key_Down);
        win::update_move_resize(client, cursor()->pos());
        QCOMPARE(cursor()->pos(), cursorPos + QPoint(0, 8));

        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 4);

        cfgdata = shellSurface->get_configure_data();
        QVERIFY(cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::resizing));
        QCOMPARE(cfgdata.size, QSize(188, 88));

        shellSurface->setWindowGeometry(QRect(10, 10, 188, 88));
        shellSurface->ackConfigure(configureRequestedSpy.back().front().value<quint32>());
        render(surface, QSize(208, 108), Qt::blue);

        QVERIFY(geometryChangedSpy.wait());
        QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
        QCOMPARE(win::render_geometry(client).size(), QSize(208, 108));
        QCOMPARE(client->geo.frame.size(), QSize(188, 88));

        // Finish resizing the client.
        win::key_press_event(client, Qt::Key_Enter);
        QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
        QVERIFY(!setup.base->mod.space->move_resize_window);
#if 0
        // TODO(romangg): XdgShellClient currently doesn't send final configure event
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 5);

        cfgdata = shellSurface->get_configure_data();
        QVERIFY(!cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::resizing));
#endif

        shellSurface.reset();
        QVERIFY(wait_for_destroyed(client));
    }

    SECTION("window geo fullscreen")
    {
        // This test verifies that an xdg-shell receives correct window geometry when
        // its fullscreen state gets changed.
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        auto client = render_and_wait_for_shown(surface, QSize(200, 100), Qt::red);
        QVERIFY(client);
        QVERIFY(client->control->active);
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));
        QCOMPARE(client->geo.frame.size(), QSize(200, 100));

        QSignalSpy configureRequestedSpy(shellSurface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);
        QVERIFY(configureRequestedSpy.isValid());
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 1);

        QSignalSpy window_geometry_spy(client->shell_surface,
                                       &Wrapland::Server::XdgShellSurface::window_geometry_changed);
        QVERIFY(window_geometry_spy.isValid());
        QSignalSpy geometryChangedSpy(client->qobject.get(),
                                      &win::window_qobject::frame_geometry_changed);
        QVERIFY(geometryChangedSpy.isValid());

        shellSurface->setWindowGeometry(QRect(10, 10, 180, 80));
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        QVERIFY(geometryChangedSpy.wait());
        QCOMPARE(geometryChangedSpy.count(), 1);
        QCOMPARE(window_geometry_spy.count(), 1);

        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));
        QCOMPARE(client->geo.frame.size(), QSize(180, 80));

        win::active_window_set_fullscreen(*setup.base->mod.space);
        QCOMPARE(client->geo.restore.max, QRect(0, 0, 180, 80));

        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 2);

        auto cfgdata = shellSurface->get_configure_data();
        QCOMPARE(cfgdata.size, QSize(1280, 1024));
        QVERIFY(cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::fullscreen));

        shellSurface->setWindowGeometry(QRect(0, 0, 1280, 1024));
        shellSurface->ackConfigure(configureRequestedSpy.back().front().value<quint32>());

        render(surface, QSize(1280, 1024), Qt::blue);
        QVERIFY(geometryChangedSpy.wait());
        QCOMPARE(win::render_geometry(client).size(), QSize(1280, 1024));
        QCOMPARE(client->geo.frame.size(), QSize(1280, 1024));

        win::active_window_set_fullscreen(*setup.base->mod.space);
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 3);

        cfgdata = shellSurface->get_configure_data();
        QCOMPARE(cfgdata.size, QSize(180, 80));
        QVERIFY(!cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::fullscreen));

        shellSurface->setWindowGeometry(QRect(10, 10, 180, 80));
        shellSurface->ackConfigure(configureRequestedSpy.back().front().value<quint32>());

        render(surface, QSize(200, 100), Qt::blue);
        QVERIFY(geometryChangedSpy.wait());
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));
        QCOMPARE(client->geo.frame.size(), QSize(180, 80));

        shellSurface.reset();
        QVERIFY(wait_for_destroyed(client));
    }

    SECTION("window geo maximize")
    {
        // This test verifies that an xdg-shell receives correct window geometry when
        // its maximized state gets changed.
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface);
        QVERIFY(shellSurface);

        auto client = render_and_wait_for_shown(surface, QSize(200, 100), Qt::red);
        QVERIFY(client);
        QVERIFY(client->control->active);
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));
        QCOMPARE(client->geo.frame.size(), QSize(200, 100));

        QSignalSpy configureRequestedSpy(shellSurface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);
        QVERIFY(configureRequestedSpy.isValid());
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 1);

        QSignalSpy geometryChangedSpy(client->qobject.get(),
                                      &win::window_qobject::frame_geometry_changed);
        QVERIFY(geometryChangedSpy.isValid());
        shellSurface->setWindowGeometry(QRect(10, 10, 180, 80));
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);
        QVERIFY(geometryChangedSpy.wait());
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));
        QCOMPARE(client->geo.frame.size(), QSize(180, 80));

        win::active_window_maximize(*setup.base->mod.space);
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 2);

        auto cfgdata = shellSurface->get_configure_data();
        QCOMPARE(cfgdata.size, QSize(1280, 1024));
        QVERIFY(cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::maximized));

        shellSurface->setWindowGeometry(QRect(0, 0, 1280, 1024));
        shellSurface->ackConfigure(configureRequestedSpy.back().front().value<quint32>());

        render(surface, QSize(1280, 1024), Qt::blue);
        QVERIFY(geometryChangedSpy.wait());
        QCOMPARE(win::render_geometry(client).size(), QSize(1280, 1024));
        QCOMPARE(client->geo.frame.size(), QSize(1280, 1024));

        win::active_window_maximize(*setup.base->mod.space);
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 3);

        cfgdata = shellSurface->get_configure_data();
        QCOMPARE(cfgdata.size, QSize(180, 80));
        QVERIFY(!cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::maximized));

        shellSurface->setWindowGeometry(QRect(10, 10, 180, 80));
        shellSurface->ackConfigure(configureRequestedSpy.back().front().value<quint32>());

        render(surface, QSize(200, 100), Qt::blue);
        QVERIFY(geometryChangedSpy.wait());
        QCOMPARE(win::render_geometry(client).size(), QSize(200, 100));
        QCOMPARE(client->geo.frame.size(), QSize(180, 80));

        shellSurface.reset();
        QVERIFY(wait_for_destroyed(client));
    }

    SECTION("multi maximize")
    {
        // This test verifies that the case where a client issues two set_maximized() requests
        // separated by the initial commit is handled properly.

        // Create the test surface.
        auto surface = create_surface();
        QVERIFY(surface);
        auto shell_surface = create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly);
        QVERIFY(shell_surface);

        shell_surface->setMaximized(true);
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        // Wait for the compositor to respond with a configure event.
        QSignalSpy configureRequestedSpy(shell_surface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 1);

        auto cfgdata = shell_surface->get_configure_data();
        QCOMPARE(cfgdata.size, QSize(1280, 1024));
        REQUIRE(!(cfgdata.states & Wrapland::Client::xdg_shell_state::activated));
        QVERIFY(cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::maximized));

        // Send another set_maximized() request, but do not attach any buffer yet.
        shell_surface->setMaximized(true);
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        // The compositor must respond with another configure event even if the state hasn't
        // changed.
        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 2);

        cfgdata = shell_surface->get_configure_data();
        QCOMPARE(cfgdata.size, QSize(1280, 1024));
        QVERIFY(cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::maximized));
        REQUIRE(!(cfgdata.states & Wrapland::Client::xdg_shell_state::activated));

        shell_surface->ackConfigure(configureRequestedSpy.back().front().toUInt());

        auto client = render_and_wait_for_shown(surface, cfgdata.size, Qt::blue);

        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 3);

        cfgdata = shell_surface->get_configure_data();
        QVERIFY(cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::maximized));
        QVERIFY(cfgdata.states & Wrapland::Client::xdg_shell_state::activated);

        QCOMPARE(client->maximizeMode(), win::maximize_mode::full);
        QCOMPARE(client->geo.size(), QSize(1280, 1024));

        // Now request to maximize again. This will change nothing, but we receive another configure
        // event.
        shell_surface->setMaximized(true);
        shell_surface->ackConfigure(configureRequestedSpy.back().front().toUInt());
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 4);

        cfgdata = shell_surface->get_configure_data();
        QVERIFY(cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::maximized));

        QCOMPARE(client->maximizeMode(), win::maximize_mode::full);
        QCOMPARE(client->geo.size(), QSize(1280, 1024));

        // Now request to unmaximize. This will change the maximization state and we receive another
        // configure event, this time with an empty size.
        shell_surface->setMaximized(false);
        shell_surface->ackConfigure(configureRequestedSpy.back().front().toUInt());
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 5);

        cfgdata = shell_surface->get_configure_data();
        QVERIFY(!cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::maximized));
        QVERIFY(cfgdata.size.isEmpty());

        // Request to unmaximize again. This will change nothing, but we receive another configure
        // event.
        shell_surface->setMaximized(false);
        shell_surface->ackConfigure(configureRequestedSpy.back().front().toUInt());
        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        QVERIFY(configureRequestedSpy.wait());
        QCOMPARE(configureRequestedSpy.count(), 6);

        cfgdata = shell_surface->get_configure_data();
        QVERIFY(!cfgdata.states.testFlag(Wrapland::Client::xdg_shell_state::maximized));

        // TODO(romangg): We change the synced geometry on commit. Use other geometry or don't do
        // that.
        REQUIRE_FALSE(cfgdata.size.isEmpty());
    }

    SECTION("wm capabilities")
    {
        // Checks that capabilities are initially sent correctly
        auto surface = create_surface();
        QVERIFY(surface);
        auto shellSurface = create_xdg_shell_toplevel(surface, CreationSetup::CreateOnly);
        QVERIFY(shellSurface);

        QSignalSpy configureRequestedSpy(shellSurface.get(),
                                         &Wrapland::Client::XdgShellToplevel::configured);
        QVERIFY(configureRequestedSpy.isValid());

        surface->commit(Wrapland::Client::Surface::CommitFlag::None);

        configureRequestedSpy.wait();
        QCOMPARE(configureRequestedSpy.count(), 1);

        auto cfgdata = shellSurface->get_configure_data();
        QVERIFY(cfgdata.wm_capabilities.contains(
            Wrapland::Client::xdg_shell_wm_capability::fullscreen));
        QVERIFY(
            cfgdata.wm_capabilities.contains(Wrapland::Client::xdg_shell_wm_capability::minimize));
        QVERIFY(
            cfgdata.wm_capabilities.contains(Wrapland::Client::xdg_shell_wm_capability::maximize));
        QVERIFY(cfgdata.wm_capabilities.contains(
            Wrapland::Client::xdg_shell_wm_capability::window_menu));
    }

    SECTION("popup reposition")
    {
        // Checks that xdg-popups are positioned and repositioned correctly.
        auto surface = create_surface();
        QVERIFY(surface);
        auto shell_surface = create_xdg_shell_toplevel(surface);
        QVERIFY(shell_surface);

        auto window = render_and_wait_for_shown(surface, QSize(200, 100), Qt::red);
        QVERIFY(window);
        QCOMPARE(get_wayland_window(setup.base->mod.space->stacking.active), window);
        QCOMPARE(window->geo.frame.size(), QSize(200, 100));

        Wrapland::Client::xdg_shell_positioner_data pos_data;
        pos_data.size = QSize(50, 40);
        pos_data.anchor.rect = QRect(0, 0, 5, 10);
        pos_data.anchor.edge = Qt::BottomEdge | Qt::RightEdge;
        pos_data.gravity = pos_data.anchor.edge;

        auto popup_surface = create_surface();
        auto popup_shell_surface = create_xdg_shell_popup(popup_surface, shell_surface, pos_data);
        QVERIFY(popup_surface);
        QVERIFY(popup_shell_surface);

        QSignalSpy repositioned_spy(popup_shell_surface.get(),
                                    &Wrapland::Client::XdgShellPopup::repositioned);
        QSignalSpy configure_spy(popup_shell_surface.get(),
                                 &Wrapland::Client::XdgShellPopup::configureRequested);
        QVERIFY(repositioned_spy.isValid());
        QVERIFY(configure_spy.isValid());

        auto popup = render_and_wait_for_shown(popup_surface, pos_data.size, Qt::blue);
        QVERIFY(popup);
        QCOMPARE(popup->geo.frame,
                 QRect(window->geo.frame.topLeft() + QPoint(5, 10), QSize(50, 40)));

        QSignalSpy popup_geo_spy(popup->qobject.get(),
                                 &win::window_qobject::frame_geometry_changed);
        QVERIFY(popup_geo_spy.isValid());

        // Reposition the popup
        pos_data.anchor.rect = QRect(0, 0, 15, 20);
        auto positioner = get_client().interfaces.xdg_shell->create_positioner(pos_data);
        uint32_t token = 1234;
        popup_shell_surface->reposition(positioner, token);
        delete positioner;

        QVERIFY(configure_spy.wait());
        QCOMPARE(configure_spy.size(), 1);
        QCOMPARE(repositioned_spy.size(), 1);
        QCOMPARE(repositioned_spy.front().front().value<quint32>(), token);

        popup_shell_surface->ackConfigure(configure_spy.front().back().value<quint32>());
        popup_surface->commit();

        QVERIFY(popup_geo_spy.wait());
        QCOMPARE(popup_geo_spy.size(), 1);
        QCOMPARE(popup->geo.frame,
                 QRect(window->geo.frame.topLeft() + QPoint(15, 20), QSize(50, 40)));
    }

    SECTION("popup reactive")
    {
        // move, resize and move-resize
        auto parent_change_rect
            = GENERATE(QRect{10, 15, 0, 0},
                       QRect{0, 0, 50, 100},
                       QRect{QRect{10, 15, 0, 0}.topLeft(), QRect{0, 0, 50, 100}.size()});

        auto parent_surface = create_surface();
        auto parent_shell_surface = create_xdg_shell_toplevel(parent_surface);
        QSignalSpy parent_configure_spy(parent_shell_surface.get(),
                                        &Wrapland::Client::XdgShellToplevel::configured);
        QVERIFY(parent_configure_spy.isValid());

        QSize parent_size(200, 100);
        auto window = render_and_wait_for_shown(parent_surface, parent_size, Qt::red);
        QVERIFY(window);
        QCOMPARE(get_wayland_window(setup.base->mod.space->stacking.active), window);
        QCOMPARE(window->geo.frame.size(), parent_size);

        QSignalSpy parent_geo_spy(window->qobject.get(),
                                  &win::window_qobject::frame_geometry_changed);
        QVERIFY(parent_geo_spy.isValid());

        Wrapland::Client::xdg_shell_positioner_data pos_data;
        pos_data.size = QSize(50, 40);
        pos_data.anchor.rect = QRect(0, 0, 5, 10);
        pos_data.anchor.edge = Qt::BottomEdge | Qt::RightEdge;
        pos_data.gravity = pos_data.anchor.edge;

        auto popup_surface = create_surface();
        auto popup_shell_surface
            = create_xdg_shell_popup(popup_surface, parent_shell_surface, pos_data);
        QVERIFY(popup_surface);
        QVERIFY(popup_shell_surface);

        QSignalSpy popup_done_spy(popup_shell_surface.get(),
                                  &Wrapland::Client::XdgShellPopup::popupDone);
        QSignalSpy configure_spy(popup_shell_surface.get(),
                                 &Wrapland::Client::XdgShellPopup::configureRequested);
        QVERIFY(popup_done_spy.isValid());
        QVERIFY(configure_spy.isValid());

        auto popup = render_and_wait_for_shown(popup_surface, pos_data.size, Qt::blue);
        QVERIFY(popup);
        QCOMPARE(popup->geo.frame,
                 QRect(window->geo.frame.topLeft() + QPoint(5, 10), QSize(50, 40)));

        // Resize the parent surface
        QRect const frame_geo{window->geo.frame.topLeft() + parent_change_rect.topLeft(),
                              window->geo.frame.size() + parent_change_rect.size()};

        auto const orig_frame_geo = window->geo.frame;

        auto parent_set_and_ack_geo = [&](auto const& geo) {
            window->setFrameGeometry(geo);

            if (window->geo.frame.size() == geo.size()) {
                return;
            }
            QVERIFY(parent_configure_spy.wait());
            parent_shell_surface->ackConfigure(parent_configure_spy.back().back().value<quint32>());
            render(get_client(), parent_surface, geo.size(), Qt::red);
            QVERIFY(parent_geo_spy.wait());
        };

        parent_set_and_ack_geo(frame_geo);

        // By default popups are not reactive. So marks the popup as done.
        QVERIFY(popup_done_spy.wait());
        QVERIFY(configure_spy.empty());

        parent_set_and_ack_geo(orig_frame_geo);

        // Now create a new popup, which is reactive.
        pos_data.is_reactive = true;
        popup_surface = create_surface();
        popup_shell_surface = create_xdg_shell_popup(popup_surface, parent_shell_surface, pos_data);
        QVERIFY(popup_surface);
        QVERIFY(popup_shell_surface);

        QSignalSpy popup_done_spy2(popup_shell_surface.get(),
                                   &Wrapland::Client::XdgShellPopup::popupDone);
        QSignalSpy configure_spy2(popup_shell_surface.get(),
                                  &Wrapland::Client::XdgShellPopup::configureRequested);
        QVERIFY(popup_done_spy2.isValid());
        QVERIFY(configure_spy2.isValid());

        popup = render_and_wait_for_shown(popup_surface, pos_data.size, Qt::blue);
        QVERIFY(popup);
        QCOMPARE(popup->geo.frame,
                 QRect(window->geo.frame.topLeft() + QPoint(5, 10), QSize(50, 40)));
        QVERIFY(configure_spy2.empty());

        // Resize the parent surface
        parent_set_and_ack_geo(frame_geo);

        // Popup is reactive. So configure is requested.
        QVERIFY(configure_spy2.wait());
        QCOMPARE(configure_spy2.size(), 1);
        QVERIFY(popup_done_spy2.empty());
    }
}

}
