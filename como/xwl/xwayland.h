/*
SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "data_bridge.h"
#include "types.h"

#include "base/wayland/server.h"
#include "base/x11/selection_owner.h"
#include "base/x11/xcb/helpers.h"
#include "input/cursor.h"
#include "render/compositor_start.h"
#include "win/wayland/surface.h"
#include "win/wayland/xwl_window.h"
#include "win/x11/space_setup.h"
#include "win/x11/xcb_event_filter.h"
#include <xwl/socket.h>
#include <xwl/xauthority.h>

#include <KLocalizedString>
#include <QAbstractEventDispatcher>
#include <QFile>
#include <QFutureWatcher>
#include <QObject>
#include <QProcess>
#include <QSocketNotifier>
#include <QTemporaryFile>
#include <QThread>
#include <QtConcurrentRun>

#include <iostream>
#include <sys/socket.h>

#include <memory>
#include <xcb/xproto.h>

namespace como::xwl
{

template<typename Space>
class xwayland : public QObject
{
public:
    using type = xwayland<Space>;
    using window_t = typename Space::window_t;

    xwayland(Space& space)
        : core{&space}
        , space{space}
    {
        socket = std::make_unique<xwl::socket>(socket::mode::transfer_fds_on_exec);
        if (!socket->is_valid()) {
            throw std::runtime_error("Failed to create Xwayland connection sockets");
        }

        for (auto fd : socket->file_descriptors) {
            auto notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            QObject::connect(notifier, &QSocketNotifier::activated, this, &type::start);
        }

        if (!qEnvironmentVariableIsSet("KWIN_WAYLAND_NO_XAUTHORITY")) {
            if (xauthority_generate_file(socket->display, xauthority_file)) {
                auto const name = xauthority_file.fileName().toLatin1();
                qputenv("XAUTHORITY", name);
                space.base.process_environment.insert(QStringLiteral("XAUTHORITY"), name);
            } else {
                qCWarning(KWIN_CORE) << "Failed to create an Xauthority file";
            }
        }

        qputenv("DISPLAY", socket->name().c_str());
        space.base.process_environment.insert(QStringLiteral("DISPLAY"), socket->name().c_str());
    }

private:
    void start()
    {
        if (xwayland_process) {
            return;
        }

        std::vector<int> fds_to_close;
        auto fds_cleanup = qScopeGuard([&fds_to_close] {
            for (auto fd : fds_to_close) {
                close(fd);
            }
        });

        int pipeFds[2];
        if (pipe(pipeFds) != 0) {
            throw std::runtime_error("Failed to create pipe to start Xwayland");
        }
        fds_to_close.push_back(pipeFds[1]);

        int sx[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
            throw std::runtime_error("Failed to open socket to open XCB connection");
        }

        int fd = dup(sx[1]);
        if (fd < 0) {
            throw std::system_error(std::error_code(20, std::generic_category()),
                                    "Failed to dup socket to open XCB connection");
        }

        auto const waylandSocket = space.base.server->create_xwayland_connection();
        if (waylandSocket == -1) {
            close(fd);
            throw std::runtime_error("Failed to open socket for Xwayland");
        }
        auto const wlfd = dup(waylandSocket);
        if (wlfd < 0) {
            close(fd);
            throw std::system_error(std::error_code(20, std::generic_category()),
                                    "Failed to dup socket for Xwayland");
        }

        xcb_connection_fd = sx[0];

        xwayland_process = new QProcess(this);
        xwayland_process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        xwayland_process->setProgram(QStringLiteral("Xwayland"));

        auto env = space.base.process_environment;
        env.insert("WAYLAND_SOCKET", QByteArray::number(wlfd));

        if (qEnvironmentVariableIsSet("KWIN_XWAYLAND_DEBUG")) {
            env.insert("WAYLAND_DEBUG", QByteArrayLiteral("1"));
        }

        xwayland_process->setProcessEnvironment(env);

        QStringList arguments;
        arguments << socket->name().c_str();

        if (auto name = xauthority_file.fileName(); !name.isEmpty()) {
            arguments << QStringLiteral("-auth") << name;
        }

        for (auto socket : socket->file_descriptors) {
            auto dup_socket = dup(socket);
            fds_to_close.push_back(dup_socket);
            arguments << QStringLiteral("-listenfd") << QString::number(dup_socket);
        }

        arguments << QStringLiteral("-displayfd") << QString::number(pipeFds[1]);
        arguments << QStringLiteral("-rootless");
        arguments << QStringLiteral("-wm") << QString::number(fd);

        xwayland_process->setArguments(arguments);
        xwayland_fail_notifier = QObject::connect(
            xwayland_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart) {
                    std::cerr << "FATAL ERROR: failed to start Xwayland" << std::endl;
                } else {
                    std::cerr << "FATAL ERROR: Xwayland failed, going to exit now" << std::endl;
                }
                exit(1);
            });

        // When Xwayland starts writing the display name to displayfd, it is ready. Alternatively,
        // the Xwayland can send us the SIGUSR1 signal, but it's already reserved for VT hand-off.
        ready_notifier = std::make_unique<QSocketNotifier>(pipeFds[0], QSocketNotifier::Read);
        connect(ready_notifier.get(), &QSocketNotifier::activated, this, [this]() {
            close(ready_notifier->socket());
            ready_notifier = {};
            continue_startup_with_x11();
        });

        xwayland_process->start();
    }

public:
    ~xwayland() override
    {
        data_bridge.reset();

        QObject::disconnect(xwayland_fail_notifier);

        win::x11::clear_space(space);

        if (space.base.x11_data.connection) {
            xcb_set_input_focus(space.base.x11_data.connection,
                                XCB_INPUT_FOCUS_POINTER_ROOT,
                                XCB_INPUT_FOCUS_POINTER_ROOT,
                                space.base.x11_data.time);
            space.atoms.reset();
            core.x11.atoms = nullptr;
            win::x11::net::reset_atoms();

            space.base.mod.render->selection_owner = {};
            space.base.x11_data.connection = nullptr;
            Q_EMIT space.base.qobject->x11_reset();
        }

        if (xwayland_process && xwayland_process->state() != QProcess::NotRunning) {
            QObject::disconnect(xwayland_process, nullptr, this, nullptr);
            xwayland_process->terminate();
            xwayland_process->waitForFinished(5000);
        }

        delete xwayland_process;
        xwayland_process = nullptr;

        space.base.server->destroy_xwayland_connection();
    }

    drag_event_reply drag_move_filter(std::optional<window_t> target, QPoint const& pos)
    {
        if (!data_bridge) {
            return drag_event_reply::wayland;
        }
        return data_bridge->drag_move_filter(target, pos);
    }

    std::unique_ptr<xwl::data_bridge<Space>> data_bridge;
    std::unique_ptr<xwl::socket> socket;

private:
    void continue_startup_with_x11()
    {
        assert(xcb_connection_fd != -1);
        core.x11.connection = xcb_connect_to_fd(xcb_connection_fd, nullptr);

        if (int error = xcb_connection_has_error(core.x11.connection)) {
            std::cerr << "FATAL ERROR connecting to Xwayland server: " << error << std::endl;
            exit(1);
        }

        auto iter = xcb_setup_roots_iterator(xcb_get_setup(core.x11.connection));
        core.x11.screen = iter.data;
        assert(core.x11.screen);

        space.base.x11_data.connection = core.x11.connection;

        // we don't support X11 multi-head in Wayland
        space.base.x11_data.screen_number = 0;
        space.base.x11_data.root_window = base::x11::get_default_screen(space.base.x11_data)->root;
        base::x11::xcb::extensions::create(space.base.x11_data);

        xcb_read_notifier.reset(new QSocketNotifier(xcb_get_file_descriptor(core.x11.connection),
                                                    QSocketNotifier::Read));

        auto processXcbEvents = [this] {
            while (auto event = xcb_poll_for_event(core.x11.connection)) {
                if (data_bridge->filter_event(event)) {
                    free(event);
                    continue;
                }
                qintptr result = 0;
                QThread::currentThread()->eventDispatcher()->filterNativeEvent(
                    QByteArrayLiteral("xcb_generic_event_t"), event, &result);
                free(event);
            }
            xcb_flush(core.x11.connection);
        };

        QObject::connect(
            xcb_read_notifier.get(), &QSocketNotifier::activated, this, processXcbEvents);
        QObject::connect(QThread::currentThread()->eventDispatcher(),
                         &QAbstractEventDispatcher::aboutToBlock,
                         this,
                         processXcbEvents);
        QObject::connect(QThread::currentThread()->eventDispatcher(),
                         &QAbstractEventDispatcher::awake,
                         this,
                         processXcbEvents);

        // create selection owner for WM_S0 - magic X display number expected by XWayland
        wm_owner = std::make_unique<base::x11::selection_owner>(
            "WM_S0", core.x11.connection, space.base.x11_data.root_window);
        wm_owner->claim(true);

        space.atoms = std::make_unique<base::x11::atoms>(core.x11.connection);
        core.x11.atoms = space.atoms.get();
        event_filter = std::make_unique<win::x11::xcb_event_filter<Space>>(space);
        qApp->installNativeEventFilter(event_filter.get());

        QObject::connect(
            space.qobject.get(),
            &Space::qobject_t::surface_id_changed,
            this,
            [this, xwayland_connection = space.base.server->xwayland_connection()](auto win_id,
                                                                                   auto id) {
                if (auto surface = space.compositor->getSurface(id, xwayland_connection)) {
                    auto win = space.windows_map.at(win_id);
                    assert(std::holds_alternative<win::wayland::xwl_window<Space>*>(win));
                    auto xwl_win = std::get<win::wayland::xwl_window<Space>*>(win);
                    win::wayland::set_surface(xwl_win, surface);
                }
            });

        base::x11::xcb::define_cursor(space.base.x11_data.connection,
                                      space.base.x11_data.root_window,
                                      win::x11::xcb_cursor_get(space, Qt::ArrowCursor));

        win::x11::init_space(space);
        Q_EMIT space.base.qobject->x11_reset();

        // Trigger possible errors, there's still a chance to abort
        base::x11::xcb::sync(space.base.x11_data.connection);

        data_bridge = std::make_unique<xwl::data_bridge<Space>>(core);
    }

    int xcb_connection_fd{-1};
    QProcess* xwayland_process{nullptr};
    QMetaObject::Connection xwayland_fail_notifier;

    runtime<Space> core;

    std::unique_ptr<QSocketNotifier> xcb_read_notifier;
    std::unique_ptr<win::x11::xcb_event_filter<Space>> event_filter;

    Space& space;
    std::unique_ptr<QSocketNotifier> ready_notifier;
    std::unique_ptr<base::x11::selection_owner> wm_owner;
    QTemporaryFile xauthority_file;
};

}
