/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "control/pointer.h"

#include "config-como.h"
#include <como/base/utils.h>
#include <como/input/backend/wlroots/device_helpers.h>
#include <como/input/pointer.h>

extern "C" {
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_pointer.h>
}

namespace como::input::backend::wlroots
{

template<typename Backend>
class pointer;

template<typename Backend>
void pointer_handle_destroy(struct wl_listener* listener, void* /*data*/)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;
    if (pointer->backend) {
        platform_remove_pointer(pointer, *pointer->backend->frontend);
    }
    delete pointer;
}

template<typename Backend>
void handle_motion(struct wl_listener* listener, void* data)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_pointer_motion_event*>(data);

    auto event = motion_event{
        QPointF(wlr_event->delta_x, wlr_event->delta_y),
        QPointF(wlr_event->unaccel_dx, wlr_event->unaccel_dy),
        {
            pointer,
            wlr_event->time_msec,
        },
    };

    Q_EMIT pointer->motion(event);
}

template<typename Backend>
void handle_motion_absolute(struct wl_listener* listener, void* data)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_pointer_motion_absolute_event*>(data);

    auto event = motion_absolute_event{
        QPointF(wlr_event->x, wlr_event->y),
        {
            pointer,
            wlr_event->time_msec,
        },
    };

    Q_EMIT pointer->motion_absolute(event);
}

template<typename Backend>
void handle_button(struct wl_listener* listener, void* data)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_pointer_button_event*>(data);

    auto event = button_event{
        wlr_event->button,
#if WLR_HAVE_WL_POINTER_ENUMS
        wlr_event->state == WL_POINTER_BUTTON_STATE_RELEASED ? button_state::released
                                                             : button_state::pressed,
#else
        wlr_event->state == WLR_BUTTON_RELEASED ? button_state::released : button_state::pressed,
#endif
        {
            pointer,
            wlr_event->time_msec,
        },
    };

    Q_EMIT pointer->button_changed(event);
}

template<typename Backend>
void handle_axis(struct wl_listener* listener, void* data)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_pointer_axis_event*>(data);

    auto get_source = [](auto wlr_source) {
        switch (wlr_source) {
#if WLR_HAVE_WL_POINTER_ENUMS
        case WL_POINTER_AXIS_SOURCE_WHEEL:
            return axis_source::wheel;
        case WL_POINTER_AXIS_SOURCE_FINGER:
            return axis_source::finger;
        case WL_POINTER_AXIS_SOURCE_CONTINUOUS:
            return axis_source::continuous;
        case WL_POINTER_AXIS_SOURCE_WHEEL_TILT:
            return axis_source::wheel_tilt;
#else
        case WLR_AXIS_SOURCE_WHEEL:
            return axis_source::wheel;
        case WLR_AXIS_SOURCE_FINGER:
            return axis_source::finger;
        case WLR_AXIS_SOURCE_CONTINUOUS:
            return axis_source::continuous;
        case WLR_AXIS_SOURCE_WHEEL_TILT:
            return axis_source::wheel_tilt;
#endif
        default:
            return axis_source::unknown;
        }
    };

    auto event = axis_event{
        get_source(wlr_event->source),
        static_cast<axis_orientation>(wlr_event->orientation),
        wlr_event->delta,
        wlr_event->delta_discrete,
        {
            pointer,
            wlr_event->time_msec,
        },
    };

    Q_EMIT pointer->axis_changed(event);
}

template<typename Backend>
void handle_swipe_begin(struct wl_listener* listener, void* data)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_pointer_swipe_begin_event*>(data);

    auto event = swipe_begin_event{
        wlr_event->fingers,
        {
            pointer,
            wlr_event->time_msec,
        },
    };

    Q_EMIT pointer->swipe_begin(event);
}

template<typename Backend>
void handle_swipe_update(struct wl_listener* listener, void* data)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_pointer_swipe_update_event*>(data);

    auto event = swipe_update_event{
        wlr_event->fingers,
        QPointF(wlr_event->dx, wlr_event->dy),
        {
            pointer,
            wlr_event->time_msec,
        },
    };

    Q_EMIT pointer->swipe_update(event);
}

template<typename Backend>
void handle_swipe_end(struct wl_listener* listener, void* data)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_pointer_swipe_end_event*>(data);

    auto event = swipe_end_event{
        wlr_event->cancelled,
        {
            pointer,
            wlr_event->time_msec,
        },
    };

    Q_EMIT pointer->swipe_end(event);
}

template<typename Backend>
void handle_pinch_begin(struct wl_listener* listener, void* data)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_pointer_pinch_begin_event*>(data);

    auto event = pinch_begin_event{
        wlr_event->fingers,
        {
            pointer,
            wlr_event->time_msec,
        },
    };

    Q_EMIT pointer->pinch_begin(event);
}

template<typename Backend>
void handle_pinch_update(struct wl_listener* listener, void* data)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_pointer_pinch_update_event*>(data);

    auto event = pinch_update_event{
        wlr_event->fingers,
        QPointF(wlr_event->dx, wlr_event->dy),
        wlr_event->scale,
        wlr_event->rotation,
        {
            pointer,
            wlr_event->time_msec,
        },
    };

    Q_EMIT pointer->pinch_update(event);
}

template<typename Backend>
void handle_pinch_end(struct wl_listener* listener, void* data)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_pointer_pinch_end_event*>(data);

    auto event = pinch_end_event{
        wlr_event->cancelled,
        {
            pointer,
            wlr_event->time_msec,
        },
    };

    Q_EMIT pointer->pinch_end(event);
}

template<typename Backend>
void handle_hold_begin(struct wl_listener* listener, void* data)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_pointer_hold_begin_event*>(data);

    auto event = hold_begin_event{
        wlr_event->fingers,
        {
            pointer,
            wlr_event->time_msec,
        },
    };

    Q_EMIT pointer->hold_begin(event);
}

template<typename Backend>
void handle_hold_end(struct wl_listener* listener, void* data)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_pointer_hold_end_event*>(data);

    auto event = hold_end_event{
        wlr_event->cancelled,
        {
            pointer,
            wlr_event->time_msec,
        },
    };

    Q_EMIT pointer->hold_end(event);
}

template<typename Backend>
void handle_frame(struct wl_listener* listener, void* /*data*/)
{
    base::event_receiver<pointer<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto pointer = event_receiver_struct->receiver;

    Q_EMIT pointer->frame();
}

template<typename Backend>
class pointer : public input::pointer
{
public:
    using er = base::event_receiver<pointer<Backend>>;

    pointer(wlr_input_device* dev, Backend* backend)
        : backend{backend}
    {
        auto native = wlr_pointer_from_input_device(dev);

        if (auto libinput = get_libinput_device(dev)) {
            control = std::make_unique<pointer_control>(libinput, backend->frontend->config.main);
        }

        destroyed.receiver = this;
        destroyed.event.notify = pointer_handle_destroy<Backend>;
        wl_signal_add(&dev->events.destroy, &destroyed.event);

        motion_rec.receiver = this;
        motion_rec.event.notify = handle_motion<Backend>;
        wl_signal_add(&native->events.motion, &motion_rec.event);

        motion_absolute_rec.receiver = this;
        motion_absolute_rec.event.notify = handle_motion_absolute<Backend>;
        wl_signal_add(&native->events.motion_absolute, &motion_absolute_rec.event);

        button_rec.receiver = this;
        button_rec.event.notify = handle_button<Backend>;
        wl_signal_add(&native->events.button, &button_rec.event);

        axis_rec.receiver = this;
        axis_rec.event.notify = handle_axis<Backend>;
        wl_signal_add(&native->events.axis, &axis_rec.event);

        swipe_begin_rec.receiver = this;
        swipe_begin_rec.event.notify = handle_swipe_begin<Backend>;
        wl_signal_add(&native->events.swipe_begin, &swipe_begin_rec.event);

        swipe_update_rec.receiver = this;
        swipe_update_rec.event.notify = handle_swipe_update<Backend>;
        wl_signal_add(&native->events.swipe_update, &swipe_update_rec.event);

        swipe_end_rec.receiver = this;
        swipe_end_rec.event.notify = handle_swipe_end<Backend>;
        wl_signal_add(&native->events.swipe_end, &swipe_end_rec.event);

        pinch_begin_rec.receiver = this;
        pinch_begin_rec.event.notify = handle_pinch_begin<Backend>;
        wl_signal_add(&native->events.pinch_begin, &pinch_begin_rec.event);

        pinch_update_rec.receiver = this;
        pinch_update_rec.event.notify = handle_pinch_update<Backend>;
        wl_signal_add(&native->events.pinch_update, &pinch_update_rec.event);

        pinch_end_rec.receiver = this;
        pinch_end_rec.event.notify = handle_pinch_end<Backend>;
        wl_signal_add(&native->events.pinch_end, &pinch_end_rec.event);

        hold_begin_rec.receiver = this;
        hold_begin_rec.event.notify = handle_hold_begin<Backend>;
        wl_signal_add(&native->events.hold_begin, &hold_begin_rec.event);

        hold_end_rec.receiver = this;
        hold_end_rec.event.notify = handle_hold_end<Backend>;
        wl_signal_add(&native->events.hold_end, &hold_end_rec.event);

        frame_rec.receiver = this;
        frame_rec.event.notify = handle_frame<Backend>;
        wl_signal_add(&native->events.frame, &frame_rec.event);
    }

    pointer(pointer const&) = delete;
    pointer& operator=(pointer const&) = delete;
    ~pointer() override = default;

    Backend* backend;

private:
    er destroyed;
    er motion_rec;
    er motion_absolute_rec;
    er button_rec;
    er axis_rec;
    er frame_rec;
    er swipe_begin_rec;
    er swipe_update_rec;
    er swipe_end_rec;
    er pinch_begin_rec;
    er pinch_update_rec;
    er pinch_end_rec;
    er hold_begin_rec;
    er hold_end_rec;
};

}
