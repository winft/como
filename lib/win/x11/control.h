/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "command.h"

#include "win/control.h"
#include "win/input.h"
#include "win/meta.h"
#include "win/move.h"

namespace como::win::x11
{

template<typename Win>
class control : public win::control<Win>
{
public:
    using control_t = typename win::control<Win>;

    control(Win* window)
        : control_t(window)
        , m_window{window}
    {
    }

    void set_subspaces(std::vector<subspace*> /*subs*/) override
    {
        m_window->net_info->setDesktop(get_subspace(*m_window));
    }

    void set_skip_pager(bool set) override
    {
        control_t::set_skip_pager(set);
        m_window->net_info->setState(this->skip_pager() ? net::SkipPager : net::States(),
                                     net::SkipPager);
    }

    void set_skip_switcher(bool set) override
    {
        control_t::set_skip_switcher(set);
        m_window->net_info->setState(this->skip_switcher() ? net::SkipSwitcher : net::States(),
                                     net::SkipSwitcher);
    }

    void set_skip_taskbar(bool set) override
    {
        control_t::set_skip_taskbar(set);
        m_window->net_info->setState(this->skip_taskbar() ? net::SkipTaskbar : net::States(),
                                     net::SkipTaskbar);
    }

    void update_mouse_grab() override
    {
        xcb_ungrab_button(m_window->space.base.x11_data.connection,
                          XCB_BUTTON_INDEX_ANY,
                          m_window->xcb_windows.wrapper,
                          XCB_MOD_MASK_ANY);

        if (m_window->space.tabbox->forced_global_mouse_grab()) {
            // see TabBox::establishTabBoxGrab()
            m_window->xcb_windows.wrapper.grab_button(XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
            return;
        }

        // When a passive grab is activated or deactivated, the X server will generate crossing
        // events as if the pointer were suddenly to warp from its current position to some position
        // in the grab window. Some /broken/ X11 clients do get confused by such EnterNotify and
        // LeaveNotify events so we release the passive grab for the active window.
        //
        // The passive grab below is established so the window can be raised or activated when it
        // is clicked.
        auto const& qopts = m_window->space.options->qobject;

        if ((qopts->focusPolicyIsReasonable() && !this->active)
            || (qopts->isClickRaise() && !is_most_recently_raised(m_window))) {
            if (qopts->commandWindow1() != mouse_cmd::nothing) {
                establish_command_window_grab(m_window, XCB_BUTTON_INDEX_1);
            }
            if (qopts->commandWindow2() != mouse_cmd::nothing) {
                establish_command_window_grab(m_window, XCB_BUTTON_INDEX_2);
            }
            if (qopts->commandWindow3() != mouse_cmd::nothing) {
                establish_command_window_grab(m_window, XCB_BUTTON_INDEX_3);
            }
            if (qopts->commandWindowWheel() != mouse_cmd::nothing) {
                establish_command_window_grab(m_window, XCB_BUTTON_INDEX_4);
                establish_command_window_grab(m_window, XCB_BUTTON_INDEX_5);
            }
        }

        // We want to grab <command modifier> + buttons no matter what state the window is in. The
        // client will receive funky EnterNotify and LeaveNotify events, but there is nothing that
        // we can do about it, unfortunately.

        if (!m_window->space.global_shortcuts_disabled) {
            if (qopts->commandAll1() != mouse_cmd::nothing) {
                establish_command_all_grab(m_window, XCB_BUTTON_INDEX_1);
            }
            if (qopts->commandAll2() != mouse_cmd::nothing) {
                establish_command_all_grab(m_window, XCB_BUTTON_INDEX_2);
            }
            if (qopts->commandAll3() != mouse_cmd::nothing) {
                establish_command_all_grab(m_window, XCB_BUTTON_INDEX_3);
            }
            if (qopts->commandAllWheel() != mouse_wheel_cmd::nothing) {
                establish_command_all_grab(m_window, XCB_BUTTON_INDEX_4);
                establish_command_all_grab(m_window, XCB_BUTTON_INDEX_5);
            }
        }
    }

    void destroy_decoration() override
    {
        if (decoration(m_window)) {
            auto const grav = calculate_gravitation(m_window, true);
            control_t::destroy_decoration();
            move(m_window, grav);
        }
        m_window->xcb_windows.input.reset();
    }

    QSize adjusted_frame_size(QSize const& frame_size, size_mode mode) override
    {
        auto const client_size = frame_to_client_size(m_window, frame_size);
        return size_for_client_size(m_window, client_size, mode, false);
    }

    bool can_fullscreen() const override
    {
        if (!this->rules.checkFullScreen(true)) {
            return false;
        }
        if (this->rules.checkStrictGeometry(true)) {
            // check geometry constraints (rule to obey is set)
            auto const fsarea
                = space_window_area(m_window->space, area_option::fullscreen, m_window);
            if (size_for_client_size(m_window, fsarea.size(), win::size_mode::any, true)
                != fsarea.size()) {
                // the app wouldn't fit exactly fullscreen geometry due to its strict geometry
                // requirements
                return false;
            }
        }
        // don't check size constrains - some apps request fullscreen despite requesting fixed size
        // also better disallow weird types to go fullscreen
        return !is_special_window(m_window);
    }

private:
    Win* m_window;
};

}
