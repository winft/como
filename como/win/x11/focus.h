/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "client.h"

#include <como/win/activation.h>

namespace como::win::x11
{

template<typename Win>
void focus_take(Win& win)
{
    if (win.control->rules.checkAcceptFocus(win.net_info->input())) {
        win.xcb_windows.client.focus();
    } else {
        // window cannot take input, at least withdraw urgency
        set_demands_attention(&win, false);
    }

    if (win.net_info->supportsProtocol(net::TakeFocusProtocol)) {
        base::x11::update_time_from_clock(win.space.base);
        send_client_message(win.space.base.x11_data,
                            win.xcb_windows.client,
                            win.space.atoms->wm_protocols,
                            win.space.atoms->wm_take_focus);
    }

    win.space.stacking.should_get_focus.push_back(&win);

    // E.g. fullscreens have different layer when active/not-active.
    win.space.stacking.order.update_order();

    auto breakShowingDesktop = !win.control->keep_above;

    if (breakShowingDesktop) {
        for (auto const& c : win.group->members) {
            if (is_desktop(c)) {
                breakShowingDesktop = false;
                break;
            }
        }
    }

    if (breakShowingDesktop) {
        set_showing_desktop(win.space, false);
    }
}

}
