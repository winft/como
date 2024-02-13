/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "strut_rect.h"
#include "types.h"
#include <como/win/subspaces_get.h>

#include <QRegion>

namespace como::win
{

template<typename Space>
QRegion struts_to_region(Space const& space,
                         int subspace,
                         win::strut_area areas,
                         std::vector<win::strut_rects> const& struts)
{
    if (subspace == x11_desktop_number_on_all || subspace == x11_desktop_number_undefined) {
        subspace = subspaces_get_current_x11id(*space.subspace_manager);
    }

    QRegion region;
    auto const& rects = struts[subspace];

    for (auto const& rect : rects) {
        if (flags(areas & rect.area())) {
            region += rect;
        }
    }

    return region;
}

template<typename Space>
QRegion restricted_move_area(Space const& space, int desktop, win::strut_area areas)
{
    return struts_to_region(space, desktop, areas, space.areas.restrictedmove);
}

template<typename Space>
QRegion previous_restricted_move_area(Space const& space, int desktop, win::strut_area areas)
{
    return struts_to_region(space, desktop, areas, space.oldrestrictedmovearea);
}

}
