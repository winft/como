/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "filtered_display.h"
#include "output_helpers.h"

#include "como_export.h"
#include <como/base/logging.h>
#include <como/base/wayland/platform_helpers.h>
#include <como/base/wayland/server_helpers.h>

#include <QObject>
#include <QThread>
#include <Wrapland/Server/client.h>
#include <Wrapland/Server/data_control_v1.h>
#include <Wrapland/Server/data_device_manager.h>
#include <Wrapland/Server/dpms.h>
#include <Wrapland/Server/keystate.h>
#include <Wrapland/Server/linux_dmabuf_v1.h>
#include <Wrapland/Server/output.h>
#include <Wrapland/Server/output_manager.h>
#include <Wrapland/Server/pointer_constraints_v1.h>
#include <Wrapland/Server/pointer_gestures_v1.h>
#include <Wrapland/Server/primary_selection.h>
#include <Wrapland/Server/relative_pointer_v1.h>
#include <Wrapland/Server/seat.h>
#include <Wrapland/Server/security_context_v1.h>
#include <Wrapland/Server/viewporter.h>
#include <Wrapland/Server/wlr_output_manager_v1.h>
#include <Wrapland/Server/xdg_output.h>
#include <memory>
#include <sys/socket.h>
#include <vector>

namespace como::base::wayland
{

class COMO_EXPORT server_qobject : public QObject
{
};

template<typename Base>
class server
{
public:
    server(Base& base, std::string const& socket, start_options flags)
        : server(base, flags)
    {
        display->set_socket_name(socket);
        display->start();
        create_globals();
    }

    void terminateClientConnections()
    {
        for (auto client : display->clients()) {
            client->destroy();
        }
    }

    Wrapland::Server::Seat* seat() const
    {
        if (seats.empty()) {
            return nullptr;
        }
        return seats.front().get();
    }

    /**
     * @returns file descriptor for Xwayland to connect to.
     */
    int create_xwayland_connection()
    {
        auto const socket = server_create_connection(*display);
        if (!socket.connection) {
            return -1;
        }
        m_xwayland.client = socket.connection;
        m_xwayland.destroyConnection = QObject::connect(m_xwayland.client,
                                                        &Wrapland::Server::Client::disconnected,
                                                        qobject.get(),
                                                        [] { qFatal("Xwayland Connection died"); });
        return socket.fd;
    }

    void destroy_xwayland_connection()
    {
        if (!m_xwayland.client) {
            return;
        }
        QObject::disconnect(m_xwayland.destroyConnection);
        m_xwayland.client->destroy();
        m_xwayland.client = nullptr;
    }

    /**
     * @returns whether any kind of global shortcuts are supported.
     */
    bool has_global_shortcut_support() const
    {
        return !(m_initFlags & start_options::no_global_shortcuts);
    }

    Wrapland::Server::Client* xwayland_connection() const
    {
        return m_xwayland.client;
    }

    std::unique_ptr<server_qobject> qobject;
    std::unique_ptr<Wrapland::Server::Display> display;
    std::unique_ptr<Wrapland::Server::output_manager> output_manager;

    std::unique_ptr<Wrapland::Server::linux_dmabuf_v1> linux_dmabuf;
    std::unique_ptr<Wrapland::Server::Viewporter> viewporter;
    std::vector<std::unique_ptr<Wrapland::Server::Seat>> seats;
    std::unique_ptr<Wrapland::Server::data_device_manager> data_device_manager;
    std::unique_ptr<Wrapland::Server::primary_selection_device_manager>
        primary_selection_device_manager;
    std::unique_ptr<Wrapland::Server::KeyState> key_state;
    std::unique_ptr<Wrapland::Server::PointerGesturesV1> pointer_gestures_v1;
    std::unique_ptr<Wrapland::Server::PointerConstraintsV1> pointer_constraints_v1;
    std::unique_ptr<Wrapland::Server::data_control_manager_v1> data_control_manager_v1;
    std::unique_ptr<Wrapland::Server::ShadowManager> shadow_manager;
    std::unique_ptr<Wrapland::Server::DpmsManager> dpms_manager;
    std::unique_ptr<Wrapland::Server::RelativePointerManagerV1> relative_pointer_manager_v1;
    std::unique_ptr<Wrapland::Server::security_context_manager_v1> security_context_manager_v1;

private:
    explicit server(Base& base, start_options flags)
        : qobject{std::make_unique<server_qobject>()}
        , display(std::make_unique<filtered_display>())
        , output_manager{std::make_unique<Wrapland::Server::output_manager>(*display)}
        , m_initFlags{flags}
        , base{base}

    {
        qRegisterMetaType<Wrapland::Server::output_dpms_mode>();
    }

    void create_globals()
    {
        if (!display->running()) {
            qCCritical(KWIN_CORE) << "Wayland server failed to start.";
            throw std::exception();
        }

        display->createShm();
        seats.emplace_back(std::make_unique<Wrapland::Server::Seat>(display.get()));

        output_manager->create_xdg_manager();
        pointer_gestures_v1 = std::make_unique<Wrapland::Server::PointerGesturesV1>(display.get());
        pointer_constraints_v1
            = std::make_unique<Wrapland::Server::PointerConstraintsV1>(display.get());
        data_device_manager
            = std::make_unique<Wrapland::Server::data_device_manager>(display.get());
        primary_selection_device_manager
            = std::make_unique<Wrapland::Server::primary_selection_device_manager>(display.get());
        data_control_manager_v1
            = std::make_unique<Wrapland::Server::data_control_manager_v1>(display.get());

        shadow_manager = std::make_unique<Wrapland::Server::ShadowManager>(display.get());
        dpms_manager = std::make_unique<Wrapland::Server::DpmsManager>(display.get());
        security_context_manager_v1
            = std::make_unique<Wrapland::Server::security_context_manager_v1>(display.get());

        auto& wlr_output_manager = output_manager->create_wlr_manager_v1();
        QObject::connect(&wlr_output_manager,
                         &Wrapland::Server::wlr_output_manager_v1::test_config,
                         qobject.get(),
                         [this](auto config) {
                             if (outputs_test_config(base, *config)) {
                                 config->send_succeeded();
                             } else {
                                 config->send_failed();
                             }
                         });
        QObject::connect(&wlr_output_manager,
                         &Wrapland::Server::wlr_output_manager_v1::apply_config,
                         qobject.get(),
                         [this](auto config) {
                             if (!outputs_apply_config(base, *config)) {
                                 config->send_failed();
                                 return;
                             }

                             // TODO(romangg): Should we wait until the render module has committed
                             //                the first frame and only then report success?
                             config->send_succeeded();
                             output_manager->commit_changes();
                         });

        key_state = std::make_unique<Wrapland::Server::KeyState>(display.get());
        viewporter = std::make_unique<Wrapland::Server::Viewporter>(display.get());

        relative_pointer_manager_v1
            = std::make_unique<Wrapland::Server::RelativePointerManagerV1>(display.get());
    }

    struct {
        Wrapland::Server::Client* client = nullptr;
        QMetaObject::Connection destroyConnection;
    } m_xwayland;

    QHash<Wrapland::Server::Client*, quint16> m_clientIds;
    start_options m_initFlags;
    Base& base;
};

}
