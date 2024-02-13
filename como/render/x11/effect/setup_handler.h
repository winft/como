/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/render/x11/effect.h>
#include <como/render/x11/effect/setup_window.h>
#include <como/render/x11/property_notify_filter.h>

#include <QObject>
#include <variant>

namespace como::render::x11
{

template<typename Handler>
void effect_setup_handler(Handler& handler)
{
    auto make_property_filter = [&handler] {
        using filter = render::x11::property_notify_filter<Handler, typename Handler::space_t>;
        auto& base = handler.scene.platform.base;
        handler.x11_property_notify
            = std::make_unique<filter>(handler, *base.mod.space, base.x11_data.root_window);
    };

    QObject::connect(handler.scene.platform.base.qobject.get(),
                     &base::platform_qobject::x11_reset,
                     &handler,
                     [&handler, make_property_filter] {
                         handler.registered_atoms.clear();
                         for (auto it = handler.m_propertiesForEffects.keyBegin();
                              it != handler.m_propertiesForEffects.keyEnd();
                              it++) {
                             render::x11::add_support_property(handler, *it);
                         }
                         if (handler.scene.platform.base.x11_data.connection) {
                             make_property_filter();
                         } else {
                             handler.x11_property_notify.reset();
                         }
                         Q_EMIT handler.xcbConnectionChanged();
                     });

    if (handler.scene.platform.base.x11_data.connection) {
        make_property_filter();
    }

    auto ws = handler.scene.platform.base.mod.space.get();

    // connect all clients
    for (auto& win : ws->windows) {
        std::visit(overload{[&](typename Handler::space_t::x11_window* win) {
                                if (win->control) {
                                    effect::setup_window_connections(*win);
                                }
                            },
                            [](auto&&) {}},
                   win);
    }
    for (auto win : win::x11::get_unmanageds(*ws)) {
        std::visit(overload{[&](auto&& win) {
                       render::x11::effect_setup_unmanaged_window_connections(*win);
                   }},
                   win);
    }
}

}
