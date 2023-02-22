/*
SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "lib/setup.h"

#include "base/wayland/server.h"
#include "input/keyboard_redirect.h"
#include "input/xkb/layout_manager.h"
#include "win/space.h"
#include "win/virtual_desktops.h"

#include <KConfigGroup>

#include <linux/input.h>

using namespace Wrapland::Client;

namespace KWin::detail::test
{

TEST_CASE("keymap creation failure", "[input]")
{
    // situation for for BUG 381210
    // this will fail to create keymap
    qputenv("XKB_DEFAULT_RULES", "no");
    qputenv("XKB_DEFAULT_MODEL", "no");
    qputenv("XKB_DEFAULT_LAYOUT", "no");
    qputenv("XKB_DEFAULT_VARIANT", "no");
    qputenv("XKB_DEFAULT_OPTIONS", "no");

    test::setup setup("keymap-create-fail");
    setup.start();

    setup.base->input->xkb.setConfig(KSharedConfig::openConfig({}, KConfig::SimpleConfig));

    auto layoutGroup = setup.base->input->config.xkb->group("Layout");
    layoutGroup.writeEntry("LayoutList", QStringLiteral("no"));
    layoutGroup.writeEntry("Model", "no");
    layoutGroup.writeEntry("Options", "no");
    layoutGroup.sync();

    Test::setup_wayland_connection();

    // now create the crashing condition
    // which is sending in a pointer event
    Test::pointer_button_pressed(BTN_LEFT, 0);
    Test::pointer_button_released(BTN_LEFT, 1);
}

}
