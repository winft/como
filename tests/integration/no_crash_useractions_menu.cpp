/*
SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "lib/setup.h"

#include <Wrapland/Client/compositor.h>
#include <Wrapland/Client/keyboard.h>
#include <Wrapland/Client/pointer.h>
#include <Wrapland/Client/seat.h>
#include <Wrapland/Client/shm_pool.h>
#include <Wrapland/Client/surface.h>
#include <Wrapland/Client/touch.h>
#include <linux/input.h>

namespace como::detail::test
{

TEST_CASE("no crash useractions menu", "[win]")
{
    // This test creates the condition of BUG 382063.

    // Force style to breeze as that's the one which triggered the crash.
    QVERIFY(qApp->setStyle(QStringLiteral("breeze")));

    test::setup setup("no-crash-useractions-menu");
    setup.start();
    setup.set_outputs(2);
    test_outputs_default();

    setup_wayland_connection();
    cursor()->set_pos(QPoint(1280, 512));

    auto surface = create_surface();
    auto shellSurface = create_xdg_shell_toplevel(surface);
    QVERIFY(surface);
    QVERIFY(shellSurface);

    auto client = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
    QVERIFY(client);

    setup.base->mod.space->user_actions_menu->show(QRect(), client);
    auto& userActionsMenu = setup.base->mod.space->user_actions_menu;
    QTRY_VERIFY(userActionsMenu->isShown());
    QVERIFY(userActionsMenu->hasClient());

    keyboard_key_pressed(KEY_ESC, 0);
    keyboard_key_released(KEY_ESC, 1);
    QTRY_VERIFY(!userActionsMenu->isShown());
    QVERIFY(!userActionsMenu->hasClient());

    // and show again, this triggers BUG 382063
    setup.base->mod.space->user_actions_menu->show(QRect(), client);
    QTRY_VERIFY(userActionsMenu->isShown());
    QVERIFY(userActionsMenu->hasClient());
}

}
