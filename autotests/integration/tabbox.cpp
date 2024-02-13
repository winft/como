/*
SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "lib/setup.h"

#include <KConfigGroup>
#include <Wrapland/Client/surface.h>
#include <catch2/generators/catch_generators.hpp>
#include <linux/input.h>

using namespace Wrapland::Client;

namespace como::detail::test
{

TEST_CASE("tabbox", "[win]")
{
    qputenv("KWIN_XKB_DEFAULT_KEYMAP", "1");

#if USE_XWL
    auto operation_mode = GENERATE(base::operation_mode::wayland, base::operation_mode::xwayland);
#else
    auto operation_mode = GENERATE(base::operation_mode::wayland);
#endif

    test::setup setup("tabbox", operation_mode);

    auto c = setup.base->config.main;
    c->group(QStringLiteral("TabBox")).writeEntry("ShowTabBox", false);
    c->sync();

    setup.start();
    setup_wayland_connection();
    cursor()->set_pos(QPoint(640, 512));

    SECTION("move forward")
    {
        // this test verifies that Alt+tab works correctly moving forward

        // first create three windows
        std::unique_ptr<Surface> surface1(create_surface());
        std::unique_ptr<XdgShellToplevel> shellSurface1(create_xdg_shell_toplevel(surface1));
        QVERIFY(surface1);
        QVERIFY(shellSurface1);

        auto c1 = render_and_wait_for_shown(surface1, QSize(100, 50), Qt::blue);
        QVERIFY(c1);
        QVERIFY(c1->control->active);
        std::unique_ptr<Surface> surface2(create_surface());
        std::unique_ptr<XdgShellToplevel> shellSurface2(create_xdg_shell_toplevel(surface2));
        QVERIFY(surface2);
        QVERIFY(shellSurface2);

        auto c2 = render_and_wait_for_shown(surface2, QSize(100, 50), Qt::red);
        QVERIFY(c2);
        QVERIFY(c2->control->active);
        std::unique_ptr<Surface> surface3(create_surface());
        std::unique_ptr<XdgShellToplevel> shellSurface3(create_xdg_shell_toplevel(surface3));
        QVERIFY(surface3);
        QVERIFY(shellSurface3);

        auto c3 = render_and_wait_for_shown(surface3, QSize(100, 50), Qt::red);
        QVERIFY(c3);
        QVERIFY(c3->control->active);

        // Setup tabbox signal spies
        QSignalSpy tabboxAddedSpy(setup.base->mod.space->tabbox->qobject.get(),
                                  &win::tabbox_qobject::tabbox_added);
        QVERIFY(tabboxAddedSpy.isValid());
        QSignalSpy tabboxClosedSpy(setup.base->mod.space->tabbox->qobject.get(),
                                   &win::tabbox_qobject::tabbox_closed);
        QVERIFY(tabboxClosedSpy.isValid());

        // press alt+tab
        quint32 timestamp = 0;
        keyboard_key_pressed(KEY_LEFTALT, timestamp++);
        QCOMPARE(input::xkb::get_active_keyboard_modifiers(*setup.base->mod.input),
                 Qt::AltModifier);
        keyboard_key_pressed(KEY_TAB, timestamp++);
        keyboard_key_released(KEY_TAB, timestamp++);

        QVERIFY(tabboxAddedSpy.wait());
        QVERIFY(setup.base->mod.space->tabbox->is_grabbed());

        // release alt
        keyboard_key_released(KEY_LEFTALT, timestamp++);
        QCOMPARE(tabboxClosedSpy.count(), 1);
        QCOMPARE(setup.base->mod.space->tabbox->is_grabbed(), false);
        QCOMPARE(get_wayland_window(setup.base->mod.space->stacking.active), c2);

        surface3.reset();
        QVERIFY(wait_for_destroyed(c3));
        surface2.reset();
        QVERIFY(wait_for_destroyed(c2));
        surface1.reset();
        QVERIFY(wait_for_destroyed(c1));
    }

    SECTION("move backward")
    {
        // this test verifies that Alt+Shift+tab works correctly moving backward

        // first create three windows
        std::unique_ptr<Surface> surface1(create_surface());
        std::unique_ptr<XdgShellToplevel> shellSurface1(create_xdg_shell_toplevel(surface1));
        QVERIFY(surface1);
        QVERIFY(shellSurface1);

        auto c1 = render_and_wait_for_shown(surface1, QSize(100, 50), Qt::blue);
        QVERIFY(c1);
        QVERIFY(c1->control->active);
        std::unique_ptr<Surface> surface2(create_surface());
        std::unique_ptr<XdgShellToplevel> shellSurface2(create_xdg_shell_toplevel(surface2));
        QVERIFY(surface2);
        QVERIFY(shellSurface2);

        auto c2 = render_and_wait_for_shown(surface2, QSize(100, 50), Qt::red);
        QVERIFY(c2);
        QVERIFY(c2->control->active);
        std::unique_ptr<Surface> surface3(create_surface());
        std::unique_ptr<XdgShellToplevel> shellSurface3(create_xdg_shell_toplevel(surface3));
        QVERIFY(surface3);
        QVERIFY(shellSurface3);

        auto c3 = render_and_wait_for_shown(surface3, QSize(100, 50), Qt::red);
        QVERIFY(c3);
        QVERIFY(c3->control->active);

        // Setup tabbox signal spies
        QSignalSpy tabboxAddedSpy(setup.base->mod.space->tabbox->qobject.get(),
                                  &win::tabbox_qobject::tabbox_added);
        QVERIFY(tabboxAddedSpy.isValid());
        QSignalSpy tabboxClosedSpy(setup.base->mod.space->tabbox->qobject.get(),
                                   &win::tabbox_qobject::tabbox_closed);
        QVERIFY(tabboxClosedSpy.isValid());

        // press alt+shift+tab
        quint32 timestamp = 0;
        keyboard_key_pressed(KEY_LEFTALT, timestamp++);
        QCOMPARE(input::xkb::get_active_keyboard_modifiers(*setup.base->mod.input),
                 Qt::AltModifier);
        keyboard_key_pressed(KEY_LEFTSHIFT, timestamp++);
        REQUIRE(input::xkb::get_active_keyboard_modifiers(*setup.base->mod.input)
                == (Qt::AltModifier | Qt::ShiftModifier));
        keyboard_key_pressed(KEY_TAB, timestamp++);
        keyboard_key_released(KEY_TAB, timestamp++);

        QVERIFY(tabboxAddedSpy.wait());
        QVERIFY(setup.base->mod.space->tabbox->is_grabbed());

        // release alt
        keyboard_key_released(KEY_LEFTSHIFT, timestamp++);
        QCOMPARE(tabboxClosedSpy.count(), 0);
        keyboard_key_released(KEY_LEFTALT, timestamp++);
        QCOMPARE(tabboxClosedSpy.count(), 1);
        QCOMPARE(setup.base->mod.space->tabbox->is_grabbed(), false);
        QCOMPARE(get_wayland_window(setup.base->mod.space->stacking.active), c1);

        surface3.reset();
        QVERIFY(wait_for_destroyed(c3));
        surface2.reset();
        QVERIFY(wait_for_destroyed(c2));
        surface1.reset();
        QVERIFY(wait_for_destroyed(c1));
    }

    SECTION("caps lock")
    {
        // this test verifies that Alt+tab works correctly also when Capslock is on
        // bug 368590

        // first create three windows
        std::unique_ptr<Surface> surface1(create_surface());
        std::unique_ptr<XdgShellToplevel> shellSurface1(create_xdg_shell_toplevel(surface1));
        QVERIFY(surface1);
        QVERIFY(shellSurface1);

        auto c1 = render_and_wait_for_shown(surface1, QSize(100, 50), Qt::blue);
        QVERIFY(c1);
        QVERIFY(c1->control->active);
        std::unique_ptr<Surface> surface2(create_surface());
        std::unique_ptr<XdgShellToplevel> shellSurface2(create_xdg_shell_toplevel(surface2));
        QVERIFY(surface2);
        QVERIFY(shellSurface2);

        auto c2 = render_and_wait_for_shown(surface2, QSize(100, 50), Qt::red);
        QVERIFY(c2);
        QVERIFY(c2->control->active);
        std::unique_ptr<Surface> surface3(create_surface());
        std::unique_ptr<XdgShellToplevel> shellSurface3(create_xdg_shell_toplevel(surface3));
        QVERIFY(surface3);
        QVERIFY(shellSurface3);

        auto c3 = render_and_wait_for_shown(surface3, QSize(100, 50), Qt::red);
        QVERIFY(c3);
        QVERIFY(c3->control->active);

        QTRY_COMPARE(setup.base->mod.space->stacking.order.stack,
                     (std::deque<space::window_t>{c1, c2, c3}));

        // Setup tabbox signal spies
        QSignalSpy tabboxAddedSpy(setup.base->mod.space->tabbox->qobject.get(),
                                  &win::tabbox_qobject::tabbox_added);
        QVERIFY(tabboxAddedSpy.isValid());
        QSignalSpy tabboxClosedSpy(setup.base->mod.space->tabbox->qobject.get(),
                                   &win::tabbox_qobject::tabbox_closed);
        QVERIFY(tabboxClosedSpy.isValid());

        // enable capslock
        quint32 timestamp = 0;
        keyboard_key_pressed(KEY_CAPSLOCK, timestamp++);
        keyboard_key_released(KEY_CAPSLOCK, timestamp++);
        QCOMPARE(input::xkb::get_active_keyboard_modifiers(*setup.base->mod.input),
                 Qt::ShiftModifier);

        // press alt+tab
        keyboard_key_pressed(KEY_LEFTALT, timestamp++);
        REQUIRE(input::xkb::get_active_keyboard_modifiers(*setup.base->mod.input)
                == (Qt::ShiftModifier | Qt::AltModifier));
        keyboard_key_pressed(KEY_TAB, timestamp++);
        keyboard_key_released(KEY_TAB, timestamp++);

        QVERIFY(tabboxAddedSpy.wait());
        QVERIFY(setup.base->mod.space->tabbox->is_grabbed());

        // release alt
        keyboard_key_released(KEY_LEFTALT, timestamp++);
        QCOMPARE(tabboxClosedSpy.count(), 1);
        QCOMPARE(setup.base->mod.space->tabbox->is_grabbed(), false);

        // release caps lock
        keyboard_key_pressed(KEY_CAPSLOCK, timestamp++);
        keyboard_key_released(KEY_CAPSLOCK, timestamp++);
        QCOMPARE(input::xkb::get_active_keyboard_modifiers(*setup.base->mod.input), Qt::NoModifier);
        QCOMPARE(tabboxClosedSpy.count(), 1);
        QCOMPARE(setup.base->mod.space->tabbox->is_grabbed(), false);

        // Has walked backwards to the previously lowest client in the stacking order.
        QCOMPARE(get_wayland_window(setup.base->mod.space->stacking.active), c1);
        QCOMPARE(setup.base->mod.space->stacking.order.stack,
                 (std::deque<space::window_t>{c2, c3, c1}));

        surface3.reset();
        QVERIFY(wait_for_destroyed(c3));
        surface2.reset();
        QVERIFY(wait_for_destroyed(c2));
        surface1.reset();
        QVERIFY(wait_for_destroyed(c1));
    }
}

}
