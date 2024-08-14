/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/base/utils.h>
#include <como/base/wayland/output.h>

#include <wayland-server-core.h>

extern "C" {
#define static
#include <wlr/types/wlr_output.h>
#undef static
}

namespace como::base::backend::wlroots
{

struct output_state {
    output_state()
        : native{std::make_unique<wlr_output_state>()}
    {
        wlr_output_state_init(native.get());
    }
    ~output_state()
    {
        wlr_output_state_finish(native.get());
    }
    output_state(output_state const&) = delete;
    output_state& operator=(output_state const&) = delete;
    output_state(output_state&&) = default;
    output_state& operator=(output_state&&) = default;

    wlr_output_state* get_native() const
    {
        return native.get();
    }

private:
    std::unique_ptr<wlr_output_state> native;
};

template<typename Output>
void output_handle_destroy(wl_listener* listener, void* /*data*/)
{
    base::event_receiver<Output>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto output = event_receiver_struct->receiver;

    output->native = nullptr;
    delete output;
}

template<typename Backend>
class output : public base::wayland::output<typename Backend::frontend_type>
{
public:
    using type = output;
    using abstract_type = base::wayland::output<typename Backend::frontend_type>;
    using render_t = typename Backend::render_t::output_t;

    output(wlr_output* wlr_out, Backend* backend)
        : abstract_type(*backend->frontend)
        , native{wlr_out}
        , backend{backend}
    {
        wlr_out->data = this;

        destroy_rec.receiver = this;
        destroy_rec.event.notify = output_handle_destroy<type>;
        wl_signal_add(&wlr_out->events.destroy, &destroy_rec.event);

        QVector<Wrapland::Server::output_mode> modes;

        Wrapland::Server::output_mode current_mode;
        if (auto wlr_mode = wlr_out->current_mode) {
            current_mode.size = QSize(wlr_mode->width, wlr_mode->height);
            current_mode.refresh_rate = wlr_mode->refresh;
        }

        auto add_mode
            = [&modes, &current_mode, &wlr_out](int id, int width, int height, int refresh) {
                  Wrapland::Server::output_mode mode;
                  mode.id = id;
                  mode.size = QSize(width, height);

                  if (wlr_out->current_mode && mode.size == current_mode.size
                      && refresh == current_mode.refresh_rate) {
                      current_mode.id = id;
                  }

                  // TODO(romangg): We fall back to 60000 here as we assume >0 in other code paths,
                  //    but in general 0 is a valid value in Wayland protocol which specifies that
                  //    the refresh rate is undefined.
                  mode.refresh_rate = refresh ? refresh : 60000;

                  modes.push_back(mode);
              };

        if (wl_list_empty(&wlr_out->modes)) {
            add_mode(0, wlr_out->width, wlr_out->height, wlr_out->refresh);
        } else {
            wlr_output_mode* wlr_mode;
            auto count = 0;
            wl_list_for_each(wlr_mode, &wlr_out->modes, link)
            {
                add_mode(count, wlr_mode->width, wlr_mode->height, wlr_mode->refresh);
                count++;
            }
        }

        auto const make = std::string(wlr_out->make ? wlr_out->make : "");
        auto const model = std::string(wlr_out->model ? wlr_out->model : "");
        auto const serial = std::string(wlr_out->serial ? wlr_out->serial : "");

        this->init_interfaces(wlr_out->name,
                              make,
                              model,
                              serial,
                              QSize(wlr_out->phys_width, wlr_out->phys_height),
                              modes,
                              current_mode.id != -1 ? &current_mode : nullptr);

        this->render = std::make_unique<render_t>(*this, backend->frontend->mod.render->backend);
    }

    ~output() override
    {
        wl_list_remove(&destroy_rec.event.link);
        if (native) {
            wlr_output_destroy(native);
        }
        if (backend) {
            remove_all(backend->frontend->outputs, this);
            remove_all(backend->frontend->all_outputs, this);
            backend->frontend->server->output_manager->commit_changes();
            Q_EMIT backend->frontend->qobject->output_removed(this);
        }
    }

    int gamma_ramp_size() const override
    {
        return wlr_output_get_gamma_size(native);
    }

    void update_dpms(base::dpms_mode mode) override
    {
        output_state state;
        wlr_output_state_set_enabled(state.get_native(), true);

        if (mode == base::dpms_mode::on) {
            wlr_output_commit_state(native, state.get_native());
            wayland::output_set_dpms_on(*this, *backend->frontend);
            return;
        }

        if (!wlr_output_test_state(native, state.get_native())) {
            qCWarning(KWIN_CORE) << "Failed test commit on disabling output for DPMS.";
            return;
        }

        get_render(this->render)->disable();
        wlr_output_commit_state(native, state.get_native());
        wayland::output_set_dmps_off(mode, *this, *backend->frontend);
    }

    bool change_backend_state(Wrapland::Server::output_state const& state) override
    {
        auto wrapper_state = std::make_unique<output_state>();
        wlr_output_state_set_enabled(wrapper_state->get_native(), state.enabled);

        if (state.enabled) {
            set_native_mode(*native, *wrapper_state, state.mode.id);
            wlr_output_state_set_transform(wrapper_state->get_native(),
                                           static_cast<wl_output_transform>(state.transform));
            wlr_output_state_set_adaptive_sync_enabled(wrapper_state->get_native(),
                                                       state.adaptive_sync);
        }

        if (!wlr_output_test_state(native, wrapper_state->get_native())) {
            return false;
        }

        next_state = std::move(wrapper_state);
        return true;
    }

    bool set_gamma_ramp(gamma_ramp const& gamma) override
    {
        output_state state;
        wlr_output_state_set_gamma_lut(
            state.get_native(), gamma.size(), gamma.red(), gamma.green(), gamma.blue());

        if (!wlr_output_test_state(native, state.get_native())) {
            qCWarning(KWIN_CORE) << "Failed test commit on set gamma ramp call.";
            return false;
        }
        if (!wlr_output_commit_state(native, state.get_native())) {
            qCWarning(KWIN_CORE) << "Failed commit on set gamma ramp call.";
            return false;
        }
        return true;
    }

    void ensure_next_state()
    {
        if (!next_state) {
            next_state = std::make_unique<output_state>();
        }
    }

    wlr_output* native;
    std::unique_ptr<output_state> next_state;
    Backend* backend;

private:
    base::event_receiver<output> destroy_rec;

    static void set_native_mode(wlr_output const& output, output_state& state, int mode_index)
    {
        // TODO(romangg): Determine target mode more precisly with semantic properties instead of
        // index.
        wlr_output_mode* wlr_mode;
        auto count = 0;

        wl_list_for_each(wlr_mode, &output.modes, link)
        {
            if (count == mode_index) {
                wlr_output_state_set_mode(state.get_native(), wlr_mode);
                return;
            }
            count++;
        }
    }

    template<typename AbstractRenderOutput>
    render_t* get_render(std::unique_ptr<AbstractRenderOutput>& output)
    {
        return static_cast<render_t*>(output.get());
    }
};

}
