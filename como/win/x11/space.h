/*
    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "desktop_space.h"
#include "screen_edge.h"
#include "screen_edges.h"
#include "screen_edges_filter.h"
#include "space_areas.h"
#include "space_setup.h"
#include "window.h"
#include <como/win/x11/subspace_manager.h>

#include <como/base/x11/xcb/helpers.h>
#include <como/debug/console/x11/x11_console.h>
#include <como/utils/blocker.h>
#include <como/win/desktop_space.h>
#include <como/win/kill_window.h>
#include <como/win/screen_edges.h>
#include <como/win/space_reconfigure.h>
#include <como/win/stacking_order.h>
#include <como/win/stacking_state.h>
#include <como/win/user_actions_menu.h>
#include <como/win/x11/debug.h>
#include <como/win/x11/netinfo_helpers.h>
#include <como/win/x11/tabbox_filter.h>
#include <como/win/x11/xcb_event_filter.h>

#include <vector>

namespace como::win::x11
{

struct space_mod {
};

template<typename Base, typename Mod = space_mod>
class space
{
public:
    using type = space<Base, Mod>;
    using qobject_t = space_qobject;
    using base_t = Base;
    using input_t = typename base_t::input_t::redirect_t;
    using x11_window = window<type>;
    using window_t = std::variant<x11_window*>;
    using window_group_t = x11::group<type>;
    using render_outline_t = typename base_t::render_t::qobject_t::outline_t;

    template<typename Render, typename Input>
    space(Render& render, Input& input)
        : base{input.base}
    {
        qobject = std::make_unique<space_qobject>([this] { space_start_reconfigure_timer(*this); });
        options = std::make_unique<win::options>(input.base.config.main);
        rule_book = std::make_unique<rules::book>();
        subspace_manager = std::make_unique<x11::subspace_manager>();

        outline = render_outline_t::create(render,
                                           [&, this] { return outline->create_visual(render); });
        deco = std::make_unique<deco::bridge<type>>(*this);
        appmenu = std::make_unique<dbus::appmenu>(dbus::create_appmenu_callbacks(*this));
        user_actions_menu = std::make_unique<win::user_actions_menu<type>>(*this);

        win::init_space(*this);

        singleton_interface::get_current_output_geometry = [this] {
            auto output = get_current_output(*this);
            return output ? output->geometry() : QRect();
        };

        this->input = input.integrate_space(*this);

        atoms = std::make_unique<base::x11::atoms>(base.x11_data.connection);
        edges = std::make_unique<edger_t>(*this);

        QObject::connect(
            subspace_manager->qobject.get(),
            &subspace_manager_qobject::subspace_removed,
            qobject.get(),
            [this] {
                auto const subspace_count = static_cast<int>(subspace_manager->subspaces.size());
                for (auto const& window : windows) {
                    std::visit(overload{[&](auto&& win) {
                                   if (!win->control) {
                                       return;
                                   }
                                   if (on_all_subspaces(*win)) {
                                       return;
                                   }
                                   if (get_subspace(*win) <= subspace_count) {
                                       return;
                                   }
                                   send_window_to_subspace(*this, win, subspace_count, true);
                               }},
                               window);
                }
            });

        x11::init_space(*this);

        event_filter = std::make_unique<win::x11::xcb_event_filter<type>>(*this);
        qApp->installNativeEventFilter(event_filter.get());
    }

    virtual ~space()
    {
        x11::clear_space(*this);
        win::clear_space(*this);
    }

    void resize(QSize const& size)
    {
        handle_desktop_resize(root_info.get(), size);
        win::handle_desktop_resize(*this, size);
    }

    void handle_subspace_changed(uint subspace)
    {
        x11::popagate_subspace_change(*this, subspace);
    }

    /// On X11 an internal window is an unmanaged and mapped by the window id.
    x11_window* findInternal(QWindow* window) const
    {
        if (!window) {
            return nullptr;
        }
        return find_unmanaged<x11_window>(*this, window->winId());
    }

    template<typename Win>
    QRect get_icon_geometry(Win const* /*win*/) const
    {
        return {};
    }

    using edger_t = screen_edger<type>;
    std::unique_ptr<win::screen_edge<edger_t>> create_screen_edge(edger_t& edger)
    {
        if (!edges_filter) {
            edges_filter = std::make_unique<screen_edges_filter<type>>(*this);
        }
        return std::make_unique<x11::screen_edge<edger_t>>(&edger, *atoms);
    }

    void update_space_area_from_windows(QRect const& desktop_area,
                                        std::vector<QRect> const& screens_geos,
                                        win::space_areas& areas)
    {
        for (auto const& win : windows) {
            std::visit(overload{[&](x11_window* win) {
                           if (win->control) {
                               update_space_areas(win, desktop_area, screens_geos, areas);
                           }
                       }},
                       win);
        }
    }

    void show_debug_console()
    {
        auto console = new debug::x11_console(*this);
        console->show();
    }

    void update_work_area() const
    {
        x11::update_work_areas(*this);
    }

    void update_tool_windows_visibility(bool also_hide)
    {
        x11::update_tool_windows_visibility(this, also_hide);
    }

    template<typename Win>
    void set_active_window(Win& window)
    {
        if (root_info) {
            x11::root_info_set_active_window(*root_info, window);
        }
    }

    void unset_active_window()
    {
        if (root_info) {
            x11::root_info_unset_active_window(*root_info);
        }
    }

    void debug(QString& support) const
    {
        x11::debug_support_info(*this, support);
    }

    bool tabbox_grab()
    {
        base::x11::update_time_from_clock(base);
        if (!base.mod.input->grab_keyboard()) {
            return false;
        }

        // Don't try to establish a global mouse grab using XGrabPointer, as that would prevent
        // using Alt+Tab while DND (#44972). However force passive grabs on all windows in order to
        // catch MouseRelease events and close the tabbox (#67416). All clients already have passive
        // grabs in their wrapper windows, so check only the active client, which may not have it.
        assert(!tabbox->grab.forced_global_mouse);
        tabbox->grab.forced_global_mouse = true;

        if (stacking.active) {
            std::visit(overload{[&](auto&& win) { win->control->update_mouse_grab(); }},
                       *stacking.active);
        }

        tabbox_filter = std::make_unique<x11::tabbox_filter<win::tabbox<type>>>(*tabbox);
        return true;
    }

    void tabbox_ungrab()
    {
        base::x11::update_time_from_clock(base);
        base.mod.input->ungrab_keyboard();

        assert(tabbox->grab.forced_global_mouse);
        tabbox->grab.forced_global_mouse = false;

        if (stacking.active) {
            std::visit(overload{[](auto&& win) { win->control->update_mouse_grab(); }},
                       *stacking.active);
        }

        tabbox_filter = {};
    }

    base_t& base;

    std::unique_ptr<qobject_t> qobject;
    std::unique_ptr<win::options> options;

    win::space_areas areas;
    std::unique_ptr<base::x11::atoms> atoms;
    std::unique_ptr<QQmlEngine> qml_engine;
    std::unique_ptr<rules::book> rule_book;

    std::unique_ptr<base::x11::event_filter> m_wasUserInteractionFilter;
    std::unique_ptr<base::x11::event_filter> m_movingClientFilter;
    std::unique_ptr<base::x11::event_filter> m_syncAlarmFilter;

    int initial_subspace{1};
    std::unique_ptr<base::x11::xcb::window> m_nullFocus;

    int block_focus{0};

    QPoint focusMousePos;

    // Timer to collect requests for 'reconfigure'
    QTimer reconfigureTimer;
    QTimer updateToolWindowsTimer;

    // Array of the previous restricted areas that window cannot be moved into
    std::vector<win::strut_rects> oldrestrictedmovearea;

    std::unique_ptr<x11::subspace_manager> subspace_manager;
    std::unique_ptr<x11::session_manager> session_manager;

    QTimer* m_quickTileCombineTimer{nullptr};
    win::quicktiles m_lastTilingMode{win::quicktiles::none};

    QWidget* active_popup{nullptr};

    std::vector<session_info*> session;

    // Delay(ed) window focus timer and client
    QTimer* delayFocusTimer{nullptr};

    bool showing_desktop{false};
    bool was_user_interaction{false};

    int session_active_client;
    int session_desktop;

    win::shortcut_dialog* client_keys_dialog{nullptr};
    bool global_shortcuts_disabled{false};

    // array of previous sizes of xinerama screens
    std::vector<QRect> oldscreensizes;

    // previous sizes od displayWidth()/displayHeight()
    QSize olddisplaysize;

    int set_active_client_recursion{0};

    base::x11::xcb::window shape_helper_window;

    uint32_t window_id{0};

    std::unique_ptr<render_outline_t> outline;
    std::unique_ptr<edger_t> edges;
    std::unique_ptr<deco::bridge<type>> deco;
    std::unique_ptr<dbus::appmenu> appmenu;
    std::unique_ptr<x11::root_info<space>> root_info;

    std::unique_ptr<input_t> input;
    std::unordered_map<std::string, xcb_cursor_t> xcb_cursors;

    std::unique_ptr<win::tabbox<type>> tabbox;
    std::unique_ptr<osd_notification<input_t>> osd;
    std::unique_ptr<kill_window<type>> window_killer;
    std::unique_ptr<win::user_actions_menu<type>> user_actions_menu;

    Mod mod;

    std::vector<window_t> windows;
    std::unordered_map<uint32_t, window_t> windows_map;
    std::vector<win::x11::group<type>*> groups;

    stacking_state<window_t> stacking;

    std::optional<window_t> active_popup_client;
    std::optional<window_t> client_keys_client;
    std::optional<window_t> move_resize_window;

private:
    std::unique_ptr<xcb_event_filter<type>> event_filter;
    std::unique_ptr<base::x11::event_filter> edges_filter;
    std::unique_ptr<base::x11::event_filter> tabbox_filter;
};

/**
 * Some fullscreen effects have to raise the screenedge on top of an input window, thus all windows
 * this function puts them back where they belong for regular use and is some cheap variant of
 * the regular propagate_clients function in that it completely ignores managed clients and
 * everything else and also does not update the NETWM property. Called from
 * Effects::destroyInputWindow so far.
 */
template<typename Space>
void stack_screen_edges_under_override_redirect(Space* space)
{
    if (!space->root_info) {
        return;
    }

    std::vector<xcb_window_t> windows;
    windows.push_back(space->root_info->supportWindow());

    auto const edges_wins = screen_edges_windows(*space->edges);
    windows.insert(windows.end(), edges_wins.begin(), edges_wins.end());

    base::x11::xcb::restack_windows(space->base.x11_data.connection, windows);
}

}
