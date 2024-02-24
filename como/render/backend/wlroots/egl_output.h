/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "egl_helpers.h"
#include "wlr_includes.h"

#include <como/base/logging.h>
#include <como/render/wayland/egl_data.h>

#include <como/render/gl/interface/texture.h>
#include <como/render/gl/interface/utils.h>

#include <QRegion>
#include <deque>
#include <epoxy/egl.h>
#include <memory>
#include <optional>

namespace como::render::backend::wlroots
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

#if WLR_HAVE_NEW_PIXEL_COPY_API
        if (!wlr_output_test_state(base.native, base.next_state->get_native())) {
            qCWarning(KWIN_CORE) << "Atomic output test failed on present.";
            base.next_state.reset();
            return false;
        }
        if (!wlr_output_commit_state(base.native, base.next_state->get_native())) {
            qCWarning(KWIN_CORE) << "Atomic output commit failed on present.";
            base.next_state.reset();
            return false;
        }

        base.next_state.reset();
#else
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
#endif
        return true;
    }

    Output* out;
    int bufferAge{0};
    wayland::egl_data egl_data;

    /** Damage history for the past 10 frames. */
    std::deque<QRegion> damageHistory;
};

}
