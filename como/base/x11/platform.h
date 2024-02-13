/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/base/backend/x11/wm_selection_owner.h>
#include <como/base/logging.h>
#include <como/base/platform_helpers.h>
#include <como/base/platform_qobject.h>
#include <como/base/singleton_interface.h>
#include <como/base/x11/data.h>
#include <como/base/x11/event_filter.h>
#include <como/base/x11/event_filter_manager.h>
#include <como/base/x11/output.h>
#include <como/base/x11/output_helpers.h>
#include <como/base/x11/randr_filter.h>
#include <como/input/x11/platform.h>
#include <como/render/x11/platform.h>
#include <como/win/x11/space.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace como::base::x11
{

template<typename Mod>
class platform;

struct platform_mod {
    using platform_t = base::x11::platform<platform_mod>;
    using render_t = render::x11::platform<platform_t>;
    using input_t = input::x11::platform<platform_t>;
    using space_t = win::x11::space<platform_t>;

    std::unique_ptr<render_t> render;
    std::unique_ptr<input_t> input;
    std::unique_ptr<space_t> space;
};

template<typename Mod = platform_mod>
class platform
{
public:
    using type = platform<Mod>;
    using qobject_t = platform_qobject;
    using output_t = base::x11::output<type>;

    using render_t = typename Mod::render_t;
    using input_t = typename Mod::input_t;
    using space_t = typename Mod::space_t;

    platform(base::config config)
        : qobject{std::make_unique<platform_qobject>([this] { return topology.max_scale; })}
        , config{std::move(config)}
        , x11_event_filters{std::make_unique<base::x11::event_filter_manager>()}
    {
        operation_mode = operation_mode::x11;

        x11_data.connection = QX11Info::connection();
        x11_data.root_window = QX11Info::appRootWindow();
        x11_data.screen_number = QX11Info::appScreen();

        platform_init(*this);
    }

    virtual ~platform()
    {
        if (owner && owner->ownerWindow() != XCB_WINDOW_NONE) {
            xcb_set_input_focus(x11_data.connection,
                                XCB_INPUT_FOCUS_POINTER_ROOT,
                                XCB_INPUT_FOCUS_POINTER_ROOT,
                                x11_data.time);
        }

        singleton_interface::get_outputs = {};
        singleton_interface::platform = nullptr;
    }

    void update_outputs()
    {
        if (!randr_filter) {
            randr_filter = std::make_unique<base::x11::randr_filter<type>>(*this);
            update_outputs_impl<base::x11::xcb::randr::screen_resources>();
            return;
        }

        update_outputs_impl<base::x11::xcb::randr::current_resources>();
    }

    std::unique_ptr<platform_qobject> qobject;
    base::operation_mode operation_mode;
    output_topology topology;
    base::config config;
    base::x11::data x11_data;
    int crash_count{0};

    std::unique_ptr<backend::x11::wm_selection_owner> owner;
    std::unique_ptr<base::options> options;
    std::unique_ptr<base::seat::session> session;
    std::unique_ptr<x11::event_filter_manager> x11_event_filters;

    std::unordered_map<output_t*, std::unique_ptr<output_t>> owned_outputs;
    std::vector<output_t*> outputs;
    std::unique_ptr<base::x11::event_filter> randr_filter;

    Mod mod;

private:
    template<typename Resources>
    void update_outputs_impl()
    {
        auto res_outs = base::x11::get_outputs_from_resources(
            *this, Resources(x11_data.connection, x11_data.root_window));

        qCDebug(KWIN_CORE) << "Update outputs:" << this->outputs.size() << "-->" << res_outs.size();

        // First check for removed outputs (we go backwards through the outputs, LIFO).
        for (auto old_it = this->outputs.rbegin(); old_it != this->outputs.rend();) {
            auto x11_old_out = static_cast<output_t*>(*old_it);

            auto is_in_new_outputs = [x11_old_out, &res_outs] {
                auto it = std::find_if(
                    res_outs.begin(), res_outs.end(), [x11_old_out](auto const& out) {
                        return x11_old_out->data.crtc == out->data.crtc
                            && x11_old_out->data.name == out->data.name;
                    });
                return it != res_outs.end();
            };

            if (is_in_new_outputs()) {
                // The old output is still there. Keep it in the base outputs.
                old_it++;
                continue;
            }

            qCDebug(KWIN_CORE) << "  removed:" << x11_old_out->name();
            auto old_out = *old_it;
            old_it = static_cast<decltype(old_it)>(this->outputs.erase(std::next(old_it).base()));
            Q_EMIT qobject->output_removed(old_out);
            owned_outputs.erase(old_out);
        }

        // Second check for added and changed outputs.
        for (auto& out : res_outs) {
            auto it = std::find_if(
                this->outputs.begin(), this->outputs.end(), [&out](auto const& old_out) {
                    auto old_x11_out = static_cast<output_t*>(old_out);
                    return old_x11_out->data.crtc == out->data.crtc
                        && old_x11_out->data.name == out->data.name;
                });

            if (it == this->outputs.end()) {
                qCDebug(KWIN_CORE) << "  added:" << out->name();
                auto out_ptr = out.get();
                owned_outputs.insert({out_ptr, std::move(out)});
                this->outputs.push_back(out_ptr);
                Q_EMIT qobject->output_added(out_ptr);
            } else {
                // Update data of lasting output.
                (*it)->data = out->data;
            }
        }

        update_output_topology(*this);
    }
};

}
