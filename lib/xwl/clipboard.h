/*
SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "event_x11.h"
#include "selection_data.h"
#include "selection_wl.h"
#include "selection_x11.h"
#include "sources_ext.h"

#include "base/wayland/server.h"

#include <Wrapland/Server/data_source.h>
#include <Wrapland/Server/seat.h>

namespace como::xwl
{

/**
 * Represents the X clipboard, which is on Wayland side just called
 * @e selection.
 */
template<typename Space>
class clipboard
{
public:
    using space_t = Space;

    selection_data<Space, Wrapland::Server::data_source, data_source_ext> data;

    clipboard(runtime<Space> const& core)
        : space{*core.space}
    {
        data = create_selection_data<Space, Wrapland::Server::data_source, data_source_ext>(
            core.space->atoms->clipboard, core);

        register_x11_selection(this, QSize(10, 10));

        QObject::connect(space.base.server->seat(),
                         &Wrapland::Server::Seat::selectionChanged,
                         data.qobject.get(),
                         [this] { handle_wl_selection_change(this); });
    }

    Wrapland::Server::data_source* get_current_source() const
    {
        return space.base.server->seat()->selection();
    }

    void set_selection(Wrapland::Server::data_source* source) const
    {
        space.base.server->seat()->setSelection(source);
    }

    void handle_x11_offer_change(std::vector<std::string> const& added,
                                 std::vector<std::string> const& removed)
    {
        xwl::handle_x11_offer_change(this, added, removed);
    }

    Space& space;
};

}
