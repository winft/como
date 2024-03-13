/*
    SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screen_locker.h"

#include <como/base/wayland/server_helpers.h>

namespace como::desktop::kde
{

bool screen_locker::is_locked() const
{
    return ScreenLocker::KSldApp::self()->lockState() == ScreenLocker::KSldApp::Locked
        || ScreenLocker::KSldApp::self()->lockState() == ScreenLocker::KSldApp::AcquiringLock;
}

Wrapland::Server::Client* screen_locker::get_client() const
{
    return client;
}

int screen_locker::create_client(Wrapland::Server::Display& display)
{
    auto const socket = base::wayland::server_create_connection(display);
    if (!socket.connection) {
        return -1;
    }

    client = socket.connection;
    QObject::connect(
        client, &Wrapland::Server::Client::disconnected, this, [this] { client = nullptr; });

    return socket.fd;
}

}
