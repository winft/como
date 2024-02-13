/*
SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "base/x11/event_filter.h"
#include "base/x11/xcb/proto.h"
#include "win/x11/key_server.h"
#include <base/x11/data.h>

namespace como::win::x11
{

template<typename Tabbox>
class tabbox_filter : public base::x11::event_filter
{
public:
    explicit tabbox_filter(Tabbox& tabbox)
        : base::x11::event_filter(*tabbox.space.base.x11_event_filters,
                                  QVector<int>{XCB_KEY_PRESS,
                                               XCB_KEY_RELEASE,
                                               XCB_MOTION_NOTIFY,
                                               XCB_BUTTON_PRESS,
                                               XCB_BUTTON_RELEASE})
        , tabbox{tabbox}
    {
    }

    bool event(xcb_generic_event_t* event) override
    {
        if (!tabbox.is_grabbed()) {
            return false;
        }
        const uint8_t event_type = event->response_type & ~0x80;
        switch (event_type) {
        case XCB_BUTTON_PRESS:
        case XCB_BUTTON_RELEASE: {
            auto e = reinterpret_cast<xcb_button_press_event_t*>(event);
            xcb_allow_events(
                tabbox.space.base.x11_data.connection, XCB_ALLOW_ASYNC_POINTER, XCB_CURRENT_TIME);
            if (!tabbox.is_shown() && tabbox.is_displayed()) {
                if (auto& effects = tabbox.space.base.mod.render->effects;
                    effects && effects->isMouseInterception()) {
                    // pass on to effects, effects will filter out the event
                    return false;
                }
            }
            if (event_type == XCB_BUTTON_PRESS) {
                return button_press(e);
            }
            return false;
        }
        case XCB_MOTION_NOTIFY: {
            motion(event);
            break;
        }
        case XCB_KEY_PRESS: {
            key_press(event);
            return true;
        }
        case XCB_KEY_RELEASE:
            key_release(event);
            return true;
        }
        return false;
    }

private:
    bool button_press(xcb_button_press_event_t* event)
    {
        // press outside Tabbox?
        QPoint pos(event->root_x, event->root_y);
        if ((!tabbox.is_shown() && tabbox.is_displayed())
            || (!tabbox_handle->contains_pos(pos)
                && (event->detail == XCB_BUTTON_INDEX_1 || event->detail == XCB_BUTTON_INDEX_2
                    || event->detail == XCB_BUTTON_INDEX_3))) {
            tabbox.close(); // click outside closes tab
            return true;
        }
        if (event->detail == XCB_BUTTON_INDEX_5 || event->detail == XCB_BUTTON_INDEX_4) {
            // mouse wheel event
            const QModelIndex index = tabbox_handle->next_prev(event->detail == XCB_BUTTON_INDEX_5);
            if (index.isValid()) {
                tabbox.set_current_index(index);
            }
            return true;
        }
        return false;
    }

    void motion(xcb_generic_event_t* event)
    {
        auto const& x11_data = tabbox.space.base.x11_data;
        auto* mouse_event = reinterpret_cast<xcb_motion_notify_event_t*>(event);
        const QPoint rootPos(mouse_event->root_x, mouse_event->root_y);
        // TODO: this should be in ScreenEdges directly
        tabbox.space.edges->check(
            rootPos, base::x11::xcb_time_to_chrono(x11_data, x11_data.time), true);
        xcb_allow_events(x11_data.connection, XCB_ALLOW_ASYNC_POINTER, XCB_CURRENT_TIME);
    }

    void key_press(xcb_generic_event_t* event)
    {
        int key_qt;
        xcb_key_press_event_t* key_event = reinterpret_cast<xcb_key_press_event_t*>(event);
        x11::key_server::xcbKeyPressEventToQt(key_event, &key_qt);
        tabbox.key_press(key_qt);
    }

    void key_release(xcb_generic_event_t* event)
    {
        const auto ev = reinterpret_cast<xcb_key_release_event_t*>(event);
        unsigned int mk = ev->state
            & (x11::key_server::modXShift() | x11::key_server::modXCtrl()
               | x11::key_server::modXAlt() | x11::key_server::modXMeta());
        // ev.state is state before the key release, so just checking mk being 0 isn't enough
        // using XQueryPointer() also doesn't seem to work well, so the check that all
        // modifiers are released: only one modifier is active and the currently released
        // key is this modifier - if yes, release the grab
        int mod_index = -1;
        for (int i = XCB_MAP_INDEX_SHIFT; i <= XCB_MAP_INDEX_5; ++i)
            if ((mk & (1 << i)) != 0) {
                if (mod_index >= 0)
                    return;
                mod_index = i;
            }
        bool release = false;
        if (mod_index == -1)
            release = true;
        else {
            base::x11::xcb::modifier_mapping xmk(tabbox.space.base.x11_data.connection);
            if (xmk) {
                xcb_keycode_t* keycodes = xmk.keycodes();
                const int max_index = xmk.size();
                for (int i = 0; i < xmk->keycodes_per_modifier; ++i) {
                    const int index = xmk->keycodes_per_modifier * mod_index + i;
                    if (index >= max_index) {
                        continue;
                    }
                    if (keycodes[index] == ev->detail) {
                        release = true;
                    }
                }
            }
        }
        if (release) {
            tabbox.modifiers_released();
        }
    }

    Tabbox& tabbox;
};

}
