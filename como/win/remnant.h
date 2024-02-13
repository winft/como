/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "deco/renderer.h"

#include <como/base/logging.h>

#include <QMargins>
#include <QRect>
#include <QRegion>
#include <cassert>
#include <memory>

namespace como::win
{

template<typename Space>
std::vector<typename Space::window_t> get_remnants(Space const& space)
{
    std::vector<typename Space::window_t> ret;
    for (auto const& window : space.windows) {
        std::visit(overload{[&](auto&& win) {
                       if (win->remnant) {
                           ret.push_back(win);
                       }
                   }},
                   window);
    }
    return ret;
}

class remnant
{
public:
    remnant() = default;
    remnant(remnant&& other) noexcept
    {
        *this = std::move(other);
    }
    remnant& operator=(remnant&& other) noexcept
    {
        data = std::move(other.data);
        refcount = other.refcount;
        other.refcount = 0;
        return *this;
    }

    ~remnant()
    {
        if (refcount != 0) {
            qCCritical(KWIN_CORE) << "Remnant on destroy still with" << refcount << "refs.";
        }
    }

    void ref()
    {
        ++refcount;
    }

    void unref()
    {
        --refcount;
    }

    int refcount{1};

    struct {
        void layout_decoration_rects(QRect& left, QRect& top, QRect& right, QRect& bottom) const
        {
            left = decoration_left;
            top = decoration_top;
            right = decoration_right;
            bottom = decoration_bottom;
        }

        QMargins frame_margins;
        QRegion render_region;

        int desk;

        xcb_window_t frame{XCB_WINDOW_NONE};

        bool no_border{true};
        QRect decoration_left;
        QRect decoration_right;
        QRect decoration_top;
        QRect decoration_bottom;

        bool minimized{false};

        std::unique_ptr<deco::render_data> deco_render;
        double opacity{1};
        QByteArray window_role;
        QString caption;

        bool fullscreen{false};
        bool keep_above{false};
        bool keep_below{false};
        bool was_active{false};

        bool was_x11_client{false};
        bool was_wayland_client{false};

        bool was_group_transient{false};
        bool was_popup_window{false};
        bool was_lock_screen{false};

        double buffer_scale{1};
    } data;
};

}
