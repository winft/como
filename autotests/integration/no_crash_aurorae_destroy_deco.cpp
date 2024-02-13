/*
SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "lib/setup.h"

#include <KDecoration2/Decoration>
#include <QQuickItem>
#include <linux/input.h>

namespace como::detail::test
{

TEST_CASE("no crash aurorae destroy deco", "[win],[xwl]")
{
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());

    // this test needs to enforce OpenGL compositing to get into the crashy condition
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));

    test::setup setup("debug-console", base::operation_mode::xwayland);

    auto config = app()->base->config.main;
    config->group(QStringLiteral("org.kde.kdecoration2"))
        .writeEntry("library", "org.kde.kwin.aurorae");
    config->sync();

    setup.start();
    setup.set_outputs(2);
    test_outputs_default();

    auto& scene = app()->base->mod.render->scene;
    QVERIFY(scene);
    REQUIRE(scene->isOpenGl());

    cursor()->set_pos(QPoint(640, 512));

    SECTION("borderless maximized window")
    {
        // Verifies that Aurorae doesn't crash when clicking the maximize button with kwin config
        // option BorderlessMaximizedWindows, see BUG 362772.

        // first adjust the config
        auto group = app()->base->config.main->group(QStringLiteral("Windows"));
        group.writeEntry("BorderlessMaximizedWindows", true);
        group.sync();

        win::space_reconfigure(*app()->base->mod.space);
        QCOMPARE(app()->base->mod.space->options->qobject->borderlessMaximizedWindows(), true);

        // create an xcb window
        auto con = xcb_connection_create();
        QVERIFY(!xcb_connection_has_error(con.get()));

        xcb_window_t w = xcb_generate_id(con.get());
        xcb_create_window(con.get(),
                          XCB_COPY_FROM_PARENT,
                          w,
                          app()->base->x11_data.root_window,
                          0,
                          0,
                          100,
                          200,
                          0,
                          XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          XCB_COPY_FROM_PARENT,
                          0,
                          nullptr);
        xcb_map_window(con.get(), w);
        xcb_flush(con.get());

        // we should get a client for it
        QSignalSpy windowCreatedSpy(app()->base->mod.space->qobject.get(),
                                    &space::qobject_t::clientAdded);
        QVERIFY(windowCreatedSpy.isValid());
        QVERIFY(windowCreatedSpy.wait());

        auto client_id = windowCreatedSpy.first().first().value<quint32>();
        auto client = get_x11_window(app()->base->mod.space->windows_map.at(client_id));
        QVERIFY(client);
        QCOMPARE(client->xcb_windows.client, w);
        QVERIFY(win::decoration(client) != nullptr);
        QCOMPARE(client->maximizeMode(), win::maximize_mode::restore);
        QCOMPARE(client->noBorder(), false);
        // verify that the deco is Aurorae
        QCOMPARE(qstrcmp(win::decoration(client)->metaObject()->className(), "Aurorae::Decoration"),
                 0);
        // find the maximize button
        auto item = win::decoration(client)->findChild<QQuickItem*>("maximizeButton");
        QVERIFY(item);
        const QPointF scenePoint = item->mapToScene(QPoint(0, 0));

        // Wait until the window is ready for painting, otherwise it doesn't get input events.
        TRY_REQUIRE(client->render_data.ready_for_painting);

        // simulate click on maximize button
        QSignalSpy maximizedStateChangedSpy(client->qobject.get(),
                                            &win::window_qobject::maximize_mode_changed);
        QVERIFY(maximizedStateChangedSpy.isValid());
        quint32 timestamp = 1;
        pointer_motion_absolute(client->geo.frame.topLeft() + scenePoint.toPoint(), timestamp++);
        pointer_button_pressed(BTN_LEFT, timestamp++);
        pointer_button_released(BTN_LEFT, timestamp++);
        QVERIFY(maximizedStateChangedSpy.wait());
        QCOMPARE(client->maximizeMode(), win::maximize_mode::full);
        QCOMPARE(client->noBorder(), true);

        // and destroy the window again
        xcb_unmap_window(con.get(), w);
        xcb_destroy_window(con.get(), w);
        xcb_flush(con.get());

        QSignalSpy windowClosedSpy(client->qobject.get(), &win::window_qobject::closed);
        QVERIFY(windowClosedSpy.isValid());
        QVERIFY(windowClosedSpy.wait());
    }
}

}
