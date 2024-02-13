/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "helpers.h"

#include "base/wayland/screen_lock.h"
#include "base/wayland/server.h"
#include "input/event_filter.h"
#include "input/keyboard_redirect.h"
#include "input/qt_event.h"
#include <win/wayland/space_windows.h>

#include <KScreenLocker/KsldApp>
#include <Wrapland/Server/keyboard_pool.h>
#include <Wrapland/Server/pointer_pool.h>
#include <Wrapland/Server/seat.h>
#include <Wrapland/Server/touch_pool.h>

namespace como::input
{

template<typename Redirect>
class lock_screen_filter : public event_filter<Redirect>
{
public:
    explicit lock_screen_filter(Redirect& redirect)
        : event_filter<Redirect>(redirect)
    {
    }

    bool button(button_event const& event) override
    {
        if (!base::wayland::is_screen_locked(this->redirect.platform.base)) {
            return false;
        }

        auto seat = this->redirect.platform.base.server->seat();
        seat->setTimestamp(event.base.time_msec);

        if (pointerSurfaceAllowed()) {
            // TODO: can we leak presses/releases here when we move the mouse in between from an
            // allowed surface to disallowed one or vice versa?
            event.state == button_state::pressed ? seat->pointers().button_pressed(event.key)
                                                 : seat->pointers().button_released(event.key);
        }

        return true;
    }

    bool motion(motion_event const& event) override
    {
        if (!base::wayland::is_screen_locked(this->redirect.platform.base)) {
            return false;
        }

        auto seat = this->redirect.platform.base.server->seat();
        seat->setTimestamp(event.base.time_msec);

        if (pointerSurfaceAllowed()) {
            // TODO: should the pointer position always stay in sync, i.e. not do the check?
            auto pos = this->redirect.globalPointer();
            seat->pointers().set_position(pos.toPoint());
        }

        return true;
    }

    bool axis(axis_event const& event) override
    {
        if (!base::wayland::is_screen_locked(this->redirect.platform.base)) {
            return false;
        }

        auto seat = this->redirect.platform.base.server->seat();
        if (pointerSurfaceAllowed()) {
            seat->setTimestamp(event.base.time_msec);

            auto orientation = (event.orientation == axis_orientation::horizontal) ? Qt::Horizontal
                                                                                   : Qt::Vertical;
            seat->pointers().send_axis(orientation, event.delta);
        }
        return true;
    }

    bool key(key_event const& event) override
    {
        if (!base::wayland::is_screen_locked(this->redirect.platform.base)) {
            return false;
        }

        // send event to KSldApp for global accel
        // if event is set to accepted it means a whitelisted shortcut was triggered
        // in that case we filter it out and don't process it further
        auto qt_event = key_to_qt_event(event);
        qt_event.setAccepted(false);
        QCoreApplication::sendEvent(ScreenLocker::KSldApp::self(), &qt_event);
        if (qt_event.isAccepted()) {
            return true;
        }

        // continue normal processing
        this->redirect.keyboard->update();

        auto seat = this->redirect.platform.base.server->seat();
        seat->setTimestamp(event.base.time_msec);

        if (!keyboardSurfaceAllowed()) {
            // don't pass event to seat
            return true;
        }

        pass_to_wayland_server(this->redirect, event);
        return true;
    }

    bool key_repeat(key_event const& /*event*/) override
    {
        // If screen is locked Wayland client takes care of it.
        return base::wayland::is_screen_locked(this->redirect.platform.base);
    }

    bool touch_down(touch_down_event const& event) override
    {
        if (!base::wayland::is_screen_locked(this->redirect.platform.base)) {
            return false;
        }
        auto seat = this->redirect.platform.base.server->seat();
        seat->setTimestamp(event.base.time_msec);
        if (touchSurfaceAllowed()) {
            this->redirect.touch->insertId(event.id, seat->touches().touch_down(event.pos));
        }
        return true;
    }

    bool touch_motion(touch_motion_event const& event) override
    {
        if (!base::wayland::is_screen_locked(this->redirect.platform.base)) {
            return false;
        }
        auto seat = this->redirect.platform.base.server->seat();
        seat->setTimestamp(event.base.time_msec);
        if (touchSurfaceAllowed()) {
            const qint32 wraplandId = this->redirect.touch->mappedId(event.id);
            if (wraplandId != -1) {
                seat->touches().touch_move(wraplandId, event.pos);
            }
        }
        return true;
    }

    bool touch_up(touch_up_event const& event) override
    {
        if (!base::wayland::is_screen_locked(this->redirect.platform.base)) {
            return false;
        }
        auto seat = this->redirect.platform.base.server->seat();
        seat->setTimestamp(event.base.time_msec);
        if (touchSurfaceAllowed()) {
            const qint32 wraplandId = this->redirect.touch->mappedId(event.id);
            if (wraplandId != -1) {
                seat->touches().touch_up(wraplandId);
                this->redirect.touch->removeId(event.id);
            }
        }
        return true;
    }

    bool pinch_begin(pinch_begin_event const& /*event*/) override
    {
        // no touchpad multi-finger gestures on lock screen
        return base::wayland::is_screen_locked(this->redirect.platform.base);
    }

    bool pinch_update(pinch_update_event const& /*event*/) override
    {
        // no touchpad multi-finger gestures on lock screen
        return base::wayland::is_screen_locked(this->redirect.platform.base);
    }

    bool pinch_end(pinch_end_event const& /*event*/) override
    {
        // no touchpad multi-finger gestures on lock screen
        return base::wayland::is_screen_locked(this->redirect.platform.base);
    }

    bool swipe_begin(swipe_begin_event const& /*event*/) override
    {
        // no touchpad multi-finger gestures on lock screen
        return base::wayland::is_screen_locked(this->redirect.platform.base);
    }

    bool swipe_update(swipe_update_event const& /*event*/) override
    {
        // no touchpad multi-finger gestures on lock screen
        return base::wayland::is_screen_locked(this->redirect.platform.base);
    }

    bool swipe_end(swipe_end_event const& /*event*/) override
    {
        // no touchpad multi-finger gestures on lock screen
        return base::wayland::is_screen_locked(this->redirect.platform.base);
    }

private:
    template<typename Pool>
    bool is_surface_allowed(Pool const& device_pool) const
    {
        if (auto surface = device_pool.get_focus().surface) {
            if (auto win = win::wayland::space_windows_find(this->redirect.space, surface)) {
                return win->isLockScreen() || win->isInputMethod();
            }
            return false;
        }
        return true;
    }

    bool pointerSurfaceAllowed() const
    {
        return is_surface_allowed(this->redirect.platform.base.server->seat()->pointers());
    }

    bool keyboardSurfaceAllowed() const
    {
        return is_surface_allowed(this->redirect.platform.base.server->seat()->keyboards());
    }

    bool touchSurfaceAllowed() const
    {
        return is_surface_allowed(this->redirect.platform.base.server->seat()->touches());
    }
};

}
