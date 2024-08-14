/*
SPDX-FileCopyrightText: 2018 Martin Flöser <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "lib/setup.h"

#include <KConfigGroup>
#include <Wrapland/Client/seat.h>
#include <Wrapland/Client/surface.h>
#include <Wrapland/Client/xdgdecoration.h>
#include <Wrapland/Server/display.h>
#include <Wrapland/Server/output.h>
#include <Wrapland/Server/wl_output.h>

namespace como::detail::test
{

TEST_CASE("no crash cursor physical size empty", "[win]")
{
    if (!QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                   QStringLiteral("icons/DMZ-White/index.theme"))
             .isEmpty()) {
        qputenv("XCURSOR_THEME", QByteArrayLiteral("DMZ-White"));
    } else {
        // might be vanilla-dmz (e.g. Arch, FreeBSD)
        qputenv("XCURSOR_THEME", QByteArrayLiteral("Vanilla-DMZ"));
    }
    qputenv("XCURSOR_SIZE", QByteArrayLiteral("0"));

    test::setup setup("no-crash-cursor-empty");
    setup.start();

    setup_wayland_connection(global_selection::xdg_decoration);
    cursor()->set_pos(QPoint(640, 512));

    SECTION("move cursor over deco")
    {
        // This test ensures that there is no endless recursion if the cursor theme cannot be
        // created a reason for creation failure could be physical size not existing see BUG: 390314
        auto surface = create_surface();
        auto shellSurface = create_xdg_shell_toplevel(surface);
        get_client().interfaces.xdg_decoration->getToplevelDecoration(shellSurface.get(),
                                                                      shellSurface.get());

        auto c = render_and_wait_for_shown(surface, QSize(100, 50), Qt::blue);
        QVERIFY(c);
        QVERIFY(win::decoration(c));

        // destroy physical size
        auto output = setup.base->server->output_manager->outputs.front();

        auto metadata = output->get_metadata();
        metadata.physical_size = {0, 0};
        output->set_metadata(metadata);

        // and fake a cursor theme change, so that the theme gets recreated
        Q_EMIT cursor() -> theme_changed();

        cursor()->set_pos(
            QPoint(c->geo.frame.center().x(), win::frame_to_client_pos(c, QPoint()).y() / 2));
    }
}

}
