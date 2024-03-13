/*
    SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KScreenLocker/KsldApp>
#include <QProcessEnvironment>
#include <Wrapland/Server/client.h>
#include <Wrapland/Server/display.h>
#include <Wrapland/Server/seat.h>

#include <como/desktop/screen_locker.h>
#include <como_export.h>

namespace como::desktop::kde
{

class COMO_EXPORT screen_locker : public desktop::screen_locker
{
public:
    template<typename Server>
    screen_locker(Server& server, QProcessEnvironment const& env, bool start_locked)
    {
        auto sld_app = ScreenLocker::KSldApp::self();

        ScreenLocker::KSldApp::self()->setGreeterEnvironment(env);
        ScreenLocker::KSldApp::self()->initialize();

        QObject::connect(ScreenLocker::KSldApp::self(),
                         &ScreenLocker::KSldApp::aboutToLock,
                         this,
                         [this, &server, sld_app]() {
                             if (client) {
                                 // Already sent data to KScreenLocker.
                                 return;
                             }

                             int client_fd = create_client(*server.display);
                             if (client_fd < 0) {
                                 return;
                             }

                             ScreenLocker::KSldApp::self()->setWaylandFd(client_fd);

                             for (auto&& seat : server.seats) {
                                 QObject::connect(seat.get(),
                                                  &Wrapland::Server::Seat::timestampChanged,
                                                  sld_app,
                                                  &ScreenLocker::KSldApp::userActivity);
                             }
                         });

        QObject::connect(ScreenLocker::KSldApp::self(),
                         &ScreenLocker::KSldApp::unlocked,
                         this,
                         [this, &server, sld_app]() {
                             if (client) {
                                 client->destroy();
                                 delete client;
                                 client = nullptr;
                             }

                             for (auto& seat : server.seats) {
                                 QObject::disconnect(seat.get(),
                                                     &Wrapland::Server::Seat::timestampChanged,
                                                     sld_app,
                                                     &ScreenLocker::KSldApp::userActivity);
                             }
                             ScreenLocker::KSldApp::self()->setWaylandFd(-1);
                         });

        if (start_locked) {
            ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);
        }
    }

    bool is_locked() const override;
    Wrapland::Server::Client* get_client() const override;

private:
    int create_client(Wrapland::Server::Display& display);
    Wrapland::Server::Client* client{nullptr};
};

}
