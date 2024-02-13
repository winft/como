/*
    SPDX-FileCopyrightText: 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "client.h"
#include "event.h"
#include "geo.h"
#include "net/root_info.h"
#include "root_info_filter.h"
#include "stacking.h"
#include "window_find.h"

#include <como/utils/memory.h>
#include <como/win/activation.h>

#include <memory>
#include <xcb/xcb.h>

namespace como::win::x11
{

/**
 * NET WM Protocol handler class
 */
template<typename Space>
class root_info : public net::root_info
{
public:
    using window_t = typename Space::x11_window;

    static std::unique_ptr<root_info> create(Space& space)
    {
        auto con = space.base.x11_data.connection;
        xcb_window_t supportWindow = xcb_generate_id(con);
        const uint32_t values[] = {true};
        xcb_create_window(con,
                          XCB_COPY_FROM_PARENT,
                          supportWindow,
                          space.base.x11_data.root_window,
                          0,
                          0,
                          1,
                          1,
                          0,
                          XCB_COPY_FROM_PARENT,
                          XCB_COPY_FROM_PARENT,
                          XCB_CW_OVERRIDE_REDIRECT,
                          values);
        const uint32_t lowerValues[] = {XCB_STACK_MODE_BELOW}; // See usage in layers.cpp
        // we need to do the lower window with a roundtrip, otherwise NETRootInfo is not functioning
        unique_cptr<xcb_generic_error_t> error(
            xcb_request_check(con,
                              xcb_configure_window_checked(
                                  con, supportWindow, XCB_CONFIG_WINDOW_STACK_MODE, lowerValues)));
        if (error) {
            qCDebug(KWIN_CORE) << "Error occurred while lowering support window: "
                               << error->error_code;
        }

        // clang-format off
        const net::Properties properties = net::Supported |
            net::SupportingWMCheck |
            net::ClientList |
            net::ClientListStacking |
            net::DesktopGeometry |
            net::NumberOfDesktops |
            net::CurrentDesktop |
            net::ActiveWindow |
            net::WorkArea |
            net::CloseWindow |
            net::DesktopNames |
            net::WMName |
            net::WMVisibleName |
            net::WMDesktop |
            net::WMWindowType |
            net::WMState |
            net::WMStrut |
            net::WMIconGeometry |
            net::WMIcon |
            net::WMPid |
            net::WMMoveResize |
            net::WMFrameExtents |
            net::WMPing;
        const window_type_mask types = window_type_mask::normal |
            window_type_mask::desktop |
            window_type_mask::dock |
            window_type_mask::toolbar |
            window_type_mask::menu |
            window_type_mask::dialog |
            window_type_mask::override |
            window_type_mask::utility |
            window_type_mask::splash; // No compositing window types here unless we support them also as managed window types
        const net::States states = net::Modal |
            //net::Sticky | // Large desktops not supported (and probably never will be)
            net::MaxVert |
            net::MaxHoriz |
            // net::Shaded | // Shading not supported
            net::SkipTaskbar |
            net::KeepAbove |
            //net::StaysOnTop | // The same like KeepAbove
            net::SkipPager |
            net::Hidden |
            net::FullScreen |
            net::KeepBelow |
            net::DemandsAttention |
            net::SkipSwitcher |
            net::Focused;
        net::Properties2 properties2 = net::WM2UserTime |
            net::WM2StartupId |
            net::WM2AllowedActions |
            net::WM2RestackWindow |
            net::WM2MoveResizeWindow |
            net::WM2ExtendedStrut |
            net::WM2KDETemporaryRules |
            net::WM2ShowingDesktop |
            net::WM2DesktopLayout |
            net::WM2FullPlacement |
            net::WM2FullscreenMonitors |
            net::WM2KDEShadow |
            net::WM2OpaqueRegion |
            net::WM2GTKFrameExtents |
            net::WM2GTKShowWindowMenu;
        const net::Actions actions = net::ActionMove |
            net::ActionResize |
            net::ActionMinimize |
            // net::ActionShade | // Shading not supported
            //net::ActionStick | // Sticky state is not supported
            net::ActionMaxVert |
            net::ActionMaxHoriz |
            net::ActionFullScreen |
            net::ActionChangeDesktop |
            net::ActionClose;
        // clang-format on

        return std::make_unique<root_info<Space>>(space,
                                                  supportWindow,
                                                  "KWin",
                                                  properties,
                                                  types,
                                                  states,
                                                  properties2,
                                                  actions,
                                                  space.base.x11_data.screen_number);
    }

    root_info(Space& space,
              xcb_window_t w,
              const char* name,
              net::Properties properties,
              window_type_mask types,
              net::States states,
              net::Properties2 properties2,
              net::Actions actions,
              int scr = -1)
        : net::root_info(space.base.x11_data.connection,
                         w,
                         name,
                         properties,
                         types,
                         states,
                         properties2,
                         actions,
                         scr)
        , space{space}
        , m_activeWindow(activeWindow())
        , m_eventFilter(std::make_unique<root_info_filter<root_info<Space>>>(this))
    {
    }

    ~root_info() override
    {
        xcb_destroy_window(space.base.x11_data.connection, supportWindow());
    }

    Space& space;
    xcb_window_t m_activeWindow;

protected:
    void changeNumberOfDesktops(int n) override
    {
        subspace_manager_set_count(*space.subspace_manager, n);
    }

    void changeCurrentDesktop(int d) override
    {
        subspaces_set_current(*space.subspace_manager, d);
    }

    void changeActiveWindow(xcb_window_t w,
                            net::RequestSource src,
                            xcb_timestamp_t timestamp,
                            xcb_window_t active_window) override
    {
        using var_win = typename Space::window_t;

        if (auto c = find_controlled_window<window_t>(space, predicate_match::window, w)) {
            if (timestamp == XCB_CURRENT_TIME)
                timestamp = c->userTime();
            if (src != net::FromApplication && src != net::FromTool)
                src = net::FromTool;

            if (src == net::FromTool) {
                force_activate_window(space, *c);
            } else if (var_win(c) == most_recently_activated_window(space)) {
                return; // WORKAROUND? With > 1 plasma activities, we cause this ourselves. bug
                        // #240673
            } else {    // net::FromApplication
                window_t* c2;
                if (allow_window_activation(space, c, timestamp, false, true)) {
                    activate_window(space, *c);
                }

                // if activation of the requestor's window would be allowed, allow activation too
                else if (active_window != XCB_WINDOW_NONE
                         && (c2 = find_controlled_window<window_t>(
                                 space, predicate_match::window, active_window))
                             != nullptr
                         && allow_window_activation(
                             space,
                             c2,
                             net::timestampCompare(timestamp,
                                                   c2->userTime() > 0 ? timestamp : c2->userTime()),
                             false,
                             true)) {
                    activate_window(space, *c);
                } else
                    win::set_demands_attention(c, true);
            }
        }
    }

    void closeWindow(xcb_window_t w) override
    {
        if (auto win = find_controlled_window<window_t>(space, predicate_match::window, w)) {
            win->closeWindow();
        }
    }

    void moveResize(xcb_window_t w, int x_root, int y_root, unsigned long direction) override
    {
        if (auto win = find_controlled_window<window_t>(space, predicate_match::window, w)) {
            // otherwise grabbing may have old timestamp - this message should include timestamp
            base::x11::update_time_from_clock(space.base);
            x11::net_move_resize(win, x_root, y_root, static_cast<net::Direction>(direction));
        }
    }

    void moveResizeWindow(xcb_window_t w, int flags, int x, int y, int width, int height) override
    {
        if (auto win = find_controlled_window<window_t>(space, predicate_match::window, w)) {
            x11::net_move_resize_window(win, flags, x, y, width, height);
        }
    }

    void showWindowMenu(xcb_window_t w, int /*device_id*/, int x_root, int y_root) override
    {
        if (auto win = find_controlled_window<window_t>(space, predicate_match::window, w)) {
            auto pos = QPoint(x_root, y_root);
            space.user_actions_menu->show(QRect(pos, pos), win);
        }
    }

    void gotPing(xcb_window_t w, xcb_timestamp_t timestamp) override
    {
        if (auto c = find_controlled_window<window_t>(space, predicate_match::window, w))
            x11::pong(c, timestamp);
    }

    void restackWindow(xcb_window_t w,
                       net::RequestSource source,
                       xcb_window_t above,
                       int detail,
                       xcb_timestamp_t timestamp) override
    {
        if (auto c = find_controlled_window<window_t>(space, predicate_match::window, w)) {
            if (timestamp == XCB_CURRENT_TIME) {
                timestamp = c->userTime();
            }
            if (source != net::FromApplication && source != net::FromTool) {
                source = net::FromTool;
            }
            x11::restack_window(c, above, detail, source, timestamp, true);
        }
    }

    void changeShowingDesktop(bool showing) override
    {
        set_showing_desktop(space, showing);
    }

private:
    std::unique_ptr<root_info_filter<root_info<Space>>> m_eventFilter;
};

}
