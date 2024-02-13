/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "control_destroy.h"
#include "desktop_set.h"

#include <como/win/control.h>

namespace como::win::wayland
{

template<typename Win>
class control : public win::control<Win>
{
public:
    control(Win& window)
        : win::control<Win>(&window)
        , window{window}
    {
    }

    void set_subspaces(std::vector<subspace*> subs) override
    {
        wayland::subspaces_announce(window, subs);
    }

    void destroy_plasma_wayland_integration() override
    {
        destroy_plasma_integration(*this);
    }

private:
    Win& window;
};

}
