/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/win/activation.h>

namespace como::win::x11
{

template<typename Win>
void update_urgency(Win* win)
{
    if (win->net_info->urgency()) {
        set_demands_attention(win, true);
    }
}

template<typename Win>
void cancel_focus_out_timer(Win* win)
{
    if (win->focus_out_timer) {
        win->focus_out_timer->stop();
    }
}

template<typename Win>
void do_set_active(Win& win)
{
    // Demand attention again if it's still urgent.
    update_urgency(&win);
    win.net_info->setState(win.control->active ? net::Focused : net::States(), net::Focused);
}

}
