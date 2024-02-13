/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "utils/algorithm.h"
#include <win/subspace.h>

#include <Wrapland/Server/plasma_window.h>

namespace como::win::wayland
{

template<typename Win>
void subspaces_announce(Win& win, std::vector<subspace*> subs)
{
    auto management = win.control->plasma_wayland_integration;
    if (!management) {
        return;
    }

    if (subs.empty()) {
        management->setOnAllDesktops(true);
        return;
    }

    management->setOnAllDesktops(false);

    auto currentDesktops = management->plasmaVirtualDesktops();
    for (auto sub : subs) {
        auto id = sub->id().toStdString();
        if (!contains(currentDesktops, id)) {
            management->addPlasmaVirtualDesktop(id);
        } else {
            remove_all(currentDesktops, id);
        }
    }

    for (auto desktop : currentDesktops) {
        management->removePlasmaVirtualDesktop(desktop);
    }
}

}
