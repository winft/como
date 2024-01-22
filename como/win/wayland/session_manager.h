/*
    SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/win/dbus/session_manager.h>

namespace como::win::wayland
{

class session_manager : public dbus::session_manager
{
public:
    bool closeWaylandWindows() override
    {
        return true;
    }
};

}
