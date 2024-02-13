/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "geo.h"
#include "scene.h"
#include "screen.h"

#include "base/output.h"
#include "base/output_helpers.h"
#include <base/platform_qobject.h>

namespace como::win
{

template<typename Win>
void window_setup_geometry(Win& win)
{
    QObject::connect(win.qobject.get(),
                     &Win::qobject_t::frame_geometry_changed,
                     win.qobject.get(),
                     [&win](auto const& old_geo) {
                         if (render_geometry(&win).size()
                             == frame_to_render_rect(&win, old_geo).size()) {
                             // Size unchanged. No need to update.
                             return;
                         }
                         discard_shape(win);
                         Q_EMIT win.qobject->visible_geometry_changed();
                     });

    QObject::connect(win.qobject.get(),
                     &Win::qobject_t::damaged,
                     win.qobject.get(),
                     &Win::qobject_t::needsRepaint);

    auto qbase = win.space.base.qobject.get();
    QObject::connect(qbase, &base::platform_qobject::topology_changed, win.qobject.get(), [&win] {
        check_screen(win);
    });
    QObject::connect(
        qbase, &base::platform_qobject::output_added, win.qobject.get(), [&win](auto output) {
            handle_output_added(win, static_cast<typename Win::output_t*>(output));
        });
    QObject::connect(
        qbase, &base::platform_qobject::output_removed, win.qobject.get(), [&win](auto output) {
            handle_output_removed(win, static_cast<typename Win::output_t*>(output));
        });

    setup_check_screen(win);
}

}
