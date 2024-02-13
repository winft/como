/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <wayland-server-core.h>

namespace como::base
{

template<typename T>
struct event_receiver {
    T* receiver{nullptr};
    wl_listener event;
};

}
