/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "blur_update.h"
#include "update.h"

#include <como/render/effect/interface/effect_integration.h>
#include <como/render/x11/effect.h>

#include <map>
#include <string_view>
#include <variant>

namespace como::render::x11
{

template<typename Effects>
class blur_integration : public effect::region_integration
{
public:
    blur_integration(Effects& effects)
        : effects{effects}
        , internal_properties{get_internal_blur_properties()}
    {
        setup_effect_window_add(*this);
        setup_effect_property_notify(*this);
        setup_effect_internal_window_add(*this);
        setup_effect_screen_geometry_changes(*this);
    }

    void add(Effect& effect, update_function const& update) override
    {
        registry.insert({&effect, update});
        support.atom = announce_support_property(effects, &effect, support.atom_name.data());

        auto const windows = effects.stackingOrder();
        for (auto window : windows) {
            this->update(*window);
        }
    }

    void remove(Effect& effect) override
    {
        registry.erase(&effect);
        remove_support_property(effects, &effect, support.atom_name.data());
    }

    void reset()
    {
        auto const reg_cp = registry;
        for (auto const& [effect, update_call] : reg_cp) {
            update_call({});
        }
    }

    void update(EffectWindow& window)
    {
        auto upd = get_blur_update(*this, window);
        if (!upd.base.window) {
            return;
        }
        for (auto const& [effect, update_call] : registry) {
            update_call(upd);
        }
    }

    std::map<Effect*, update_function> registry;
    Effects& effects;

    struct support_t {
        long atom{0};
        static constexpr std::string_view atom_name{"_KDE_NET_WM_BLUR_BEHIND_REGION"};
    } support;

    decltype(get_internal_blur_properties()) const internal_properties;
};

}
