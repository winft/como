/*
    SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <Wrapland/Server/client.h>

namespace como::win::wayland
{

template<typename Space>
bool screen_lock_is_supported(Space const& space)
{
    if constexpr (requires(decltype(space.mod.desktop) desktop) { desktop->screen_locker; }) {
        return space.mod.desktop && space.mod.desktop->screen_locker;
    }
    return false;
}

template<typename Space>
Wrapland::Server::Client* screen_lock_get_client(Space const& space)
{
    if constexpr (requires(decltype(space.mod.desktop) desktop) { desktop->screen_locker; }) {
        if (space.mod.desktop && space.mod.desktop->screen_locker) {
            space.mod.desktop->screen_locker->get_client();
        }
    }

    return nullptr;
}

template<typename Space>
bool screen_lock_is_locked(Space const& space)
{
    if (!screen_lock_is_supported(space)) {
        return false;
    }

    if constexpr (requires(decltype(space.mod.desktop) desktop) { desktop->screen_locker; }) {
        return space.mod.desktop->screen_locker->is_locked();
    }

    return false;
}

}
