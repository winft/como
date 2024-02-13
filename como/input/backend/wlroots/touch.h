/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "control/touch.h"
#include <como/input/backend/wlroots/device_helpers.h>

#include "config-como.h"
#include <como/base/utils.h>
#include <como/input/platform.h>
#include <como/input/touch.h>

extern "C" {
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_touch.h>
}

namespace como::input::backend::wlroots
{

template<typename Backend>
class touch;

template<typename Backend>
void touch_handle_destroy(struct wl_listener* listener, void* /*data*/)
{
    base::event_receiver<touch<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto touch = event_receiver_struct->receiver;
    if (touch->backend) {
        platform_remove_touch(touch, *touch->backend->frontend);
    }
    delete touch;
}

template<typename Backend>
void handle_down(struct wl_listener* listener, void* data)
{
    base::event_receiver<touch<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto touch = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_touch_down_event*>(data);

    auto event = touch_down_event{
        wlr_event->touch_id,
        QPointF(wlr_event->x, wlr_event->y),
        {
            touch,
            wlr_event->time_msec,
        },
    };

    Q_EMIT touch->qobject->down(event);
}

template<typename Backend>
void handle_up(struct wl_listener* listener, void* data)
{
    base::event_receiver<touch<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto touch = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_touch_up_event*>(data);

    auto event = touch_up_event{
        wlr_event->touch_id,
        {
            touch,
            wlr_event->time_msec,
        },
    };

    Q_EMIT touch->qobject->up(event);
}

template<typename Backend>
void touch_handle_motion(struct wl_listener* listener, void* data)
{
    base::event_receiver<touch<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto touch = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_touch_motion_event*>(data);

    auto event = touch_motion_event{
        wlr_event->touch_id,
        QPointF(wlr_event->x, wlr_event->y),
        {
            touch,
            wlr_event->time_msec,
        },
    };

    Q_EMIT touch->qobject->motion(event);
}

template<typename Backend>
void handle_cancel(struct wl_listener* listener, void* data)
{
    base::event_receiver<touch<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto touch = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_touch_cancel_event*>(data);

    auto event = touch_cancel_event{
        wlr_event->touch_id,
        {
            touch,
            wlr_event->time_msec,
        },
    };

    Q_EMIT touch->qobject->cancel(event);
}

template<typename Backend>
void touch_handle_frame(struct wl_listener* listener, void* /*data*/)
{
    base::event_receiver<touch<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto touch = event_receiver_struct->receiver;

    Q_EMIT touch->qobject->frame();
}

template<typename Backend>
class touch : public input::touch_impl<typename Backend::frontend_type::base_t>
{
public:
    using er = base::event_receiver<touch<Backend>>;

    touch(wlr_input_device* dev, Backend* backend)
        : touch_impl<typename Backend::frontend_type::base_t>(backend->frontend->base)
        , backend{backend}
    {
        auto native = wlr_touch_from_input_device(dev);

        if (auto libinput = get_libinput_device(dev)) {
            this->control
                = std::make_unique<touch_control>(libinput, backend->frontend->config.main);
        }
        this->output = this->get_output();

        destroyed.receiver = this;
        destroyed.event.notify = touch_handle_destroy<Backend>;
        wl_signal_add(&dev->events.destroy, &destroyed.event);

        down_rec.receiver = this;
        down_rec.event.notify = handle_down<Backend>;
        wl_signal_add(&native->events.down, &down_rec.event);

        up_rec.receiver = this;
        up_rec.event.notify = handle_up<Backend>;
        wl_signal_add(&native->events.up, &up_rec.event);

        motion_rec.receiver = this;
        motion_rec.event.notify = touch_handle_motion<Backend>;
        wl_signal_add(&native->events.motion, &motion_rec.event);

        cancel_rec.receiver = this;
        cancel_rec.event.notify = handle_cancel<Backend>;
        wl_signal_add(&native->events.cancel, &cancel_rec.event);

        frame_rec.receiver = this;
        frame_rec.event.notify = touch_handle_frame<Backend>;
        wl_signal_add(&native->events.frame, &frame_rec.event);
    }

    Backend* backend;

private:
    er destroyed;
    er down_rec;
    er up_rec;
    er motion_rec;
    er cancel_rec;
    er frame_rec;
};

}
