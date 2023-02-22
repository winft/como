/*
SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "lib/setup.h"

#include "base/wayland/server.h"
#include "input/cursor.h"
#include "render/compositor.h"
#include "render/scene.h"
#include "win/deco.h"
#include "win/screen_edges.h"
#include "win/space.h"
#include "win/space_reconfigure.h"
#include "win/wayland/window.h"
#include "win/x11/window.h"

#include <KDecoration2/Decoration>

#include <QQuickItem>

#include <linux/input.h>

namespace KWin::detail::test
{

TEST_CASE("no crash aurorae destroy deco", "[win],[xwl]")
{
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());

    // this test needs to enforce OpenGL compositing to get into the crashy condition
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));

    test::setup setup("debug-console", base::operation_mode::xwayland);

    auto config = Test::app()->base->config.main;
    config->group("org.kde.kdecoration2").writeEntry("library", "org.kde.kwin.aurorae");
    config->sync();

    setup.start();
    setup.set_outputs(2);
    Test::test_outputs_default();

    auto& scene = Test::app()->base->render->compositor->scene;
    QVERIFY(scene);
    QCOMPARE(scene->compositingType(), KWin::OpenGLCompositing);

    Test::cursor()->set_pos(QPoint(640, 512));

    SECTION("borderless maximized window")
    {
        // Verifies that Aurorae doesn't crash when clicking the maximize button with kwin config
        // option BorderlessMaximizedWindows, see BUG 362772.

        // first adjust the config
        auto group = Test::app()->base->config.main->group("Windows");
        group.writeEntry("BorderlessMaximizedWindows", true);
        group.sync();

        win::space_reconfigure(*Test::app()->base->space);
        QCOMPARE(Test::app()->base->options->qobject->borderlessMaximizedWindows(), true);

        // create an xcb window
        xcb_connection_t* c = xcb_connect(nullptr, nullptr);
        QVERIFY(!xcb_connection_has_error(c));

        xcb_window_t w = xcb_generate_id(c);
        xcb_create_window(c,
                          XCB_COPY_FROM_PARENT,
                          w,
                          Test::app()->base->x11_data.root_window,
                          0,
                          0,
                          100,
                          200,
                          0,
                          XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          XCB_COPY_FROM_PARENT,
                          0,
                          nullptr);
        xcb_map_window(c, w);
        xcb_flush(c);

        // we should get a client for it
        QSignalSpy windowCreatedSpy(Test::app()->base->space->qobject.get(),
                                    &win::space::qobject_t::clientAdded);
        QVERIFY(windowCreatedSpy.isValid());
        QVERIFY(windowCreatedSpy.wait());

        auto client_id = windowCreatedSpy.first().first().value<quint32>();
        auto client = Test::get_x11_window(Test::app()->base->space->windows_map.at(client_id));
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

        // mark the window as ready for painting, otherwise it doesn't get input events
        win::set_ready_for_painting(*client);
        QVERIFY(client->render_data.ready_for_painting);

        // simulate click on maximize button
        QSignalSpy maximizedStateChangedSpy(client->qobject.get(),
                                            &win::window_qobject::maximize_mode_changed);
        QVERIFY(maximizedStateChangedSpy.isValid());
        quint32 timestamp = 1;
        Test::pointer_motion_absolute(client->geo.frame.topLeft() + scenePoint.toPoint(),
                                      timestamp++);
        Test::pointer_button_pressed(BTN_LEFT, timestamp++);
        Test::pointer_button_released(BTN_LEFT, timestamp++);
        QVERIFY(maximizedStateChangedSpy.wait());
        QCOMPARE(client->maximizeMode(), win::maximize_mode::full);
        QCOMPARE(client->noBorder(), true);

        // and destroy the window again
        xcb_unmap_window(c, w);
        xcb_destroy_window(c, w);
        xcb_flush(c);
        xcb_disconnect(c);

        QSignalSpy windowClosedSpy(client->qobject.get(), &win::window_qobject::closed);
        QVERIFY(windowClosedSpy.isValid());
        QVERIFY(windowClosedSpy.wait());
    }
}

}
