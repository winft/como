/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "geo.h"

#include <como/win/space_areas.h>

namespace como::win::x11
{

template<typename Space>
void update_work_areas(Space& space)
{
    if (!space.root_info) {
        return;
    }

    for (uint subspace = 1; subspace <= space.subspace_manager->subspaces.size(); subspace++) {
        x11::net::rect rect;
        rect.pos.x = space.areas.work[subspace].x();
        rect.pos.y = space.areas.work[subspace].y();
        rect.size.width = space.areas.work[subspace].width();
        rect.size.height = space.areas.work[subspace].height();
        space.root_info->setWorkArea(subspace, rect);
    }
}

template<typename Window>
void update_space_areas(Window* win,
                        QRect const& desktop_area,
                        std::vector<QRect> const& screens_geos,
                        space_areas& areas)
{
    if (!win->hasStrut()) {
        return;
    }

    auto const& outputs = win->space.base.outputs;
    auto const subspaces_count = static_cast<int>(win->space.subspace_manager->subspaces.size());

    auto client_area = adjusted_client_area(win, desktop_area, desktop_area);

    // Sanity check that a strut doesn't exclude a complete screen geometry. This is a violation
    // to EWMH, as KWin just ignores the strut.
    for (auto output : outputs) {
        if (!client_area.intersects(output->geometry())) {
            // TODO(romangg): Can we give this again a logging category?
            qDebug() << "Adjusted client area would exclude a complete screen, ignore.";
            client_area = desktop_area;
            break;
        }
    }

    auto strut_region = win::x11::get_strut_rects(win);
    auto const clientsScreenRect
        = win->topo.central_output ? win->topo.central_output->geometry() : QRect();

    for (auto strut = strut_region.begin(); strut != strut_region.end(); strut++) {
        *strut = strut_rect((*strut).intersected(clientsScreenRect), (*strut).area());
    }

    // Ignore offscreen xinerama struts. These interfere with the larger monitors on the setup
    // and should be ignored so that applications that use the work area to work out where
    // windows can go can use the entire visible area of the larger monitors.
    // This goes against the EWMH description of the work area but it is a toss up between
    // having unusable sections of the screen (Which can be quite large with newer monitors)
    // or having some content appear offscreen (Relatively rare compared to other).
    auto has_offscreen_xinerama_strut = win::x11::has_offscreen_xinerama_strut(win);

    if (on_all_subspaces(*win)) {
        for (int subspace = 1; subspace <= subspaces_count; ++subspace) {
            if (!has_offscreen_xinerama_strut) {
                areas.work[subspace] = areas.work[subspace].intersected(client_area);
            }

            auto& resmove = areas.restrictedmove[subspace];
            resmove.insert(std::end(resmove), std::begin(strut_region), std::end(strut_region));

            for (size_t screen = 0; screen < outputs.size(); screen++) {
                auto const client_area_on_screen
                    = win::x11::adjusted_client_area(win, desktop_area, screens_geos[screen]);
                auto& screen_area = areas.screen[subspace][screen];
                auto const geo = screen_area.intersected(client_area_on_screen);

                // Ignore the geometry if it results in the screen getting removed completely.
                if (!geo.isEmpty()) {
                    screen_area = geo;
                }
            }
        }
    } else {
        if (!has_offscreen_xinerama_strut) {
            areas.work[get_subspace(*win)]
                = areas.work[get_subspace(*win)].intersected(client_area);
        }

        auto& resmove = areas.restrictedmove[get_subspace(*win)];
        resmove.insert(std::end(resmove), std::begin(strut_region), std::end(strut_region));

        for (size_t screen = 0; screen < outputs.size(); screen++) {
            auto const screen_area = areas.screen[get_subspace(*win)][screen];
            auto const geo = screen_area.intersected(
                win::x11::adjusted_client_area(win, desktop_area, screens_geos[screen]));

            // Ignore the geometry if it results in the screen getting removed completely.
            if (!geo.isEmpty()) {
                areas.screen[get_subspace(*win)][screen] = geo;
            }
        }
    }
}

}
