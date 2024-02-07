/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "egl_helpers.h"
#include "wlr_includes.h"

#include "base/logging.h"
#include "render/wayland/egl_data.h"

#include <render/gl/interface/texture.h>
#include <render/gl/interface/utils.h>

#include <QRegion>
#include <deque>
#include <epoxy/egl.h>
#include <memory>
#include <optional>

namespace KWin::render::backend::wlroots
{

template<typename Output>
class egl_output
{
public:
    egl_output(Output& out, wayland::egl_data egl_data)
        : out{&out}
        , egl_data{egl_data}
    {
    }

    egl_output(egl_output const&) = delete;
    egl_output& operator=(egl_output const&) = delete;

    egl_output(egl_output&& other) noexcept
    {
        *this = std::move(other);
    }

    egl_output& operator=(egl_output&& other) noexcept
    {
        out = other.out;
        bufferAge = other.bufferAge;
        egl_data = std::move(other.egl_data);
        damageHistory = std::move(other.damageHistory);

        return *this;
    }

    void make_current() const
    {
        make_context_current(egl_data);
    }

    bool present()
    {
        auto& base = static_cast<typename Output::base_t&>(out->base);
        out->swap_pending = true;

        if (!base.native->enabled) {
            wlr_output_enable(base.native, true);
        }

        if (!wlr_output_test(base.native)) {
            qCWarning(KWIN_CORE) << "Atomic output test failed on present.";
            wlr_output_rollback(base.native);
            return false;
        }
        if (!wlr_output_commit(base.native)) {
            qCWarning(KWIN_CORE) << "Atomic output commit failed on present.";
            return false;
        }
        return true;
    }

    Output* out;
    int bufferAge{0};
    wayland::egl_data egl_data;

    /** Damage history for the past 10 frames. */
    std::deque<QRegion> damageHistory;
};

}