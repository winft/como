/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/base/backend/wlroots/non_desktop_output.h>
#include <como/base/backend/wlroots/output.h>
#include <como/base/logging.h>
#include <como/base/utils.h>

namespace como::base::backend::wlroots
{

template<typename Backend>
static void handle_destroy(struct wl_listener* listener, void* /*data*/)
{
    event_receiver<Backend>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto wlr = event_receiver_struct->receiver;

    wlr->native = nullptr;
}

template<typename Backend>
void add_new_output(Backend& backend, wlr_output* native)
{
    auto& render = backend.frontend->mod.render->backend;
    wlr_output_init_render(native, render.allocator, render.renderer);

    if (!wl_list_empty(&native->modes)) {
        auto mode = wlr_output_preferred_mode(native);

        wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_mode(&state, mode);
        wlr_output_state_set_enabled(&state, true);

        if (!wlr_output_test_state(native, &state)) {
            throw std::runtime_error("wlr_output_test_state failed");
        }
        if (!wlr_output_commit_state(native, &state)) {
            throw std::runtime_error("wlr_output_commit_state failed");
        }
    }

    auto output = new wlroots::output(native, &backend);

    if (backend.align_horizontal) {
        auto shifted_geo = output->geometry();
        auto screens_width = 0;
        for (auto out : backend.frontend->outputs) {
            // +1 for QRect's bottom-right deviation
            screens_width = std::max(out->geometry().right() + 1, screens_width);
        }
        shifted_geo.moveLeft(screens_width);
        output->force_geometry(shifted_geo);
    }

    backend.frontend->all_outputs.push_back(output);
    backend.frontend->outputs.push_back(output);
    backend.frontend->server->output_manager->commit_changes();

    Q_EMIT backend.frontend->qobject->output_added(output);
}

template<typename Backend>
void handle_new_output(struct wl_listener* listener, void* data)
{
    base::event_receiver<Backend>* new_output_struct
        = wl_container_of(listener, new_output_struct, event);
    auto backend = new_output_struct->receiver;
    auto native = reinterpret_cast<wlr_output*>(data);

    if (native->non_desktop) {
        backend->non_desktop_outputs.push_back(new non_desktop_output(native, backend));
        return;
    }

    try {
        add_new_output(*backend, native);
    } catch (std::runtime_error const& e) {
        qCWarning(KWIN_CORE) << "Adding new output" << native->name << "failed:" << e.what();
    }
}

}
