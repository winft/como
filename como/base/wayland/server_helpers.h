/*
    SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/base/logging.h>

#include <Wrapland/Server/client.h>
#include <Wrapland/Server/display.h>
#include <sys/socket.h>

namespace como::base::wayland
{

struct socket_pair_connection {
    Wrapland::Server::Client* connection{nullptr};
    int fd = -1;
};

inline socket_pair_connection server_create_connection(Wrapland::Server::Display& display)
{
    socket_pair_connection ret;
    int sx[2];

    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        qCWarning(KWIN_CORE) << "Could not create socket";
        return ret;
    }

    ret.connection = display.createClient(sx[0]);
    ret.fd = sx[1];
    return ret;
}

}
