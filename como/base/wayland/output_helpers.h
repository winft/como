/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "output_transform.h"

#include "base/logging.h"
#include "base/output.h"
#include "base/output_helpers.h"
#include <utils/algorithm.h>

#include <Wrapland/Server/output.h>
#include <Wrapland/Server/wlr_output_configuration_head_v1.h>
#include <Wrapland/Server/wlr_output_configuration_v1.h>
#include <algorithm>

namespace como::base::wayland
{

template<typename Base>
auto find_output(Base const& base, Wrapland::Server::output const* output) ->
    typename Base::output_t*
{
    auto const& outs = base.all_outputs;
    auto it = std::find_if(outs.cbegin(), outs.cend(), [output](auto out) {
        return out->wrapland_output() == output;
    });
    if (it != outs.cend()) {
        return *it;
    }
    return nullptr;
}

template<typename Base>
auto outputs_get_states(Base& base)
    -> std::map<typename Base::output_t*, Wrapland::Server::output_state>
{
    std::map<typename Base::output_t*, Wrapland::Server::output_state> map;

    for (auto&& output : base.all_outputs) {
        map.insert({output, output->wrapland_output()->get_state()});
    }

    return map;
}

template<typename Base>
auto outputs_get_states(Base& base, Wrapland::Server::wlr_output_configuration_v1 const& config)
    -> std::map<typename Base::output_t*, Wrapland::Server::output_state>
{
    std::map<typename Base::output_t*, Wrapland::Server::output_state> map;
    auto config_heads = config.enabled_heads();

    for (auto&& output : base.all_outputs) {
        auto it = std::find_if(config_heads.begin(), config_heads.end(), [&](auto head) {
            return output->wrapland_output() == &head->get_output();
        });
        if (it == config_heads.end()) {
            auto state = output->wrapland_output()->get_state();
            state.enabled = false;
            map.insert({output, state});
        } else {
            map.insert({output, (*it)->get_state()});
        }
    }

    return map;
}

template<typename Output>
bool outputs_apply_states(std::map<Output*, Wrapland::Server::output_state> const& states)
{
    // We try to apply as many states as possible even if some outputs are with errors.
    bool has_error{false};

    for (auto const& [output, state] : states) {
        auto success = output->apply_state(state);
        has_error |= !success;
    }

    return !has_error;
}

template<typename Base>
bool outputs_test_config(Base& base, Wrapland::Server::wlr_output_configuration_v1 const& config)
{
    auto const current_states = outputs_get_states(base);
    auto config_states = outputs_get_states(base, config);

    auto success = outputs_apply_states(config_states);
    outputs_apply_states(current_states);
    return success;
}

template<typename Base>
bool outputs_apply_config(Base& base, Wrapland::Server::wlr_output_configuration_v1 const& config)
{
    auto const old_states = outputs_get_states(base);
    auto const config_states = outputs_get_states(base, config);

    if (!outputs_apply_states(config_states)) {
        outputs_apply_states(old_states);
        return false;
    }

    for (auto const& [output, state] : config_states) {
        if (old_states.at(output).enabled != state.enabled) {
            if (state.enabled) {
                assert(!contains(base.outputs, output));
                base.outputs.push_back(output);
                Q_EMIT base.qobject->output_added(output);
            } else {
                assert(contains(base.outputs, output));
                remove_all(base.outputs, output);
                Q_EMIT base.qobject->output_removed(output);
            }
        }

        if (state.enabled) {
            output->render->reset();
        } else {
            output->render->disable();
        }
    }

    update_output_topology(base);
    return true;
}

template<typename Base, typename Filter>
void turn_outputs_on(Base const& base, Filter& filter)
{
    filter.reset();

    for (auto& out : base.outputs) {
        out->update_dpms(base::dpms_mode::on);
    }
}

template<typename Base>
void check_outputs_on(Base const& base)
{
    if (!base.mod.space || !base.mod.space->input || !base.mod.space->input->dpms_filter) {
        // No DPMS filter exists, all outputs are on.
        return;
    }

    auto const& outs = base.outputs;
    if (std::all_of(outs.cbegin(), outs.cend(), [](auto&& out) { return out->is_dpms_on(); })) {
        // All outputs are on, disable the filter.
        base.mod.space->input->dpms_filter.reset();
    }
}

inline Wrapland::Server::output_dpms_mode to_wayland_dpms_mode(base::dpms_mode mode)
{
    switch (mode) {
    case base::dpms_mode::on:
        return Wrapland::Server::output_dpms_mode::on;
    case base::dpms_mode::standby:
        return Wrapland::Server::output_dpms_mode::standby;
    case base::dpms_mode::suspend:
        return Wrapland::Server::output_dpms_mode::suspend;
    case base::dpms_mode::off:
        return Wrapland::Server::output_dpms_mode::off;
    default:
        Q_UNREACHABLE();
    }
}

template<typename Base>
void output_set_dpms_on(typename Base::output_t& output, Base& base)
{
    qCDebug(KWIN_CORE) << "DPMS mode set for output" << output.name() << "to On.";
    output.m_dpms = base::dpms_mode::on;

    if (output.is_enabled()) {
        output.m_output->set_dpms_mode(Wrapland::Server::output_dpms_mode::on);
    }

    check_outputs_on(base);
}

template<typename Redirect>
void create_dpms_filter(Redirect& redirect)
{
    using Filter = typename Redirect::dpms_filter_t;

    if (redirect.dpms_filter) {
        // Already another output is off.
        return;
    }

    redirect.dpms_filter = std::make_unique<Filter>(redirect);
    redirect.prependInputEventFilter(redirect.dpms_filter.get());
}

template<typename Base>
void output_set_dmps_off(base::dpms_mode mode, typename Base::output_t& output, Base& base)
{
    qCDebug(KWIN_CORE) << "DPMS mode set for output" << output.name() << "to Off.";

    if (!base.mod.space || !base.mod.space->input) {
        qCWarning(KWIN_CORE) << "Abort setting DPMS. Can't create filter to set DPMS to on again.";
        return;
    }

    output.m_dpms = mode;

    if (output.is_enabled()) {
        output.m_output->set_dpms_mode(to_wayland_dpms_mode(mode));
        create_dpms_filter(*base.mod.space->input);
    }
}

}
