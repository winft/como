/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "win/x11/space_event.h"
#include "workspace.h"

#include <QAbstractNativeEventFilter>
#include <xcb/xcb.h>

namespace KWin::base::x11
{

class xcb_event_filter : public QAbstractNativeEventFilter
{
public:
    bool
    nativeEventFilter(QByteArray const& event_type, void* message, long int* /*result*/) override
    {
        if (event_type != "xcb_generic_event_t") {
            return false;
        }

        auto event = static_cast<xcb_generic_event_t*>(message);
        kwinApp()->update_x11_time_from_event(event);

        if (!Workspace::self()) {
            // Workspace not yet created
            // TODO(romangg): remove this check.
            return false;
        }

        return win::x11::space_event(*Workspace::self(), event);
    }
};

}
