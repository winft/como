/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/input/event.h>
#include <como/input/event_filter.h>
#include <como/input/keyboard.h>
#include <como/input/keyboard_redirect.h>
#include <como/input/xkb/keyboard.h>

namespace como::input
{

template<typename Redirect>
class terminate_server_filter : public event_filter<Redirect>
{
public:
    explicit terminate_server_filter(Redirect& redirect)
        : event_filter<Redirect>(redirect)
    {
    }

    bool key(key_event const& event) override
    {
        if (event.state == key_state::pressed) {
            if (event.base.dev->xkb->to_keysym(event.keycode) == XKB_KEY_Terminate_Server) {
                qCWarning(KWIN_CORE) << "Request to terminate server";
                QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
                return true;
            }
        }
        return false;
    }
};

}
