/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "control/switch.h"
#include <como/input/backend/wlroots/device_helpers.h>

#include "config-como.h"
#include <como/base/utils.h>
#include <como/input/platform.h>
#include <como/input/switch.h>

extern "C" {
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_switch.h>
}

namespace como::input::backend::wlroots
{

template<typename Backend>
class switch_device;

template<typename Backend>
void switch_handle_destroy(struct wl_listener* listener, void* /*data*/)
{
    base::event_receiver<switch_device<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto switch_device = event_receiver_struct->receiver;
    if (switch_device->backend) {
        platform_remove_switch(switch_device, *switch_device->backend->frontend);
    }
    delete switch_device;
}

template<typename Backend>
void handle_toggle(struct wl_listener* listener, void* data)
{
    base::event_receiver<switch_device<Backend>>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto switch_device = event_receiver_struct->receiver;
    auto wlr_event = reinterpret_cast<wlr_switch_toggle_event*>(data);

    auto event = switch_toggle_event{
        static_cast<switch_type>(wlr_event->switch_type),
        static_cast<switch_state>(wlr_event->switch_state),
        {
            switch_device,
            wlr_event->time_msec,
        },
    };

    Q_EMIT switch_device->toggle(event);
}

template<typename Backend>
class switch_device : public input::switch_device
{
public:
    switch_device(wlr_input_device* dev, Backend* backend)
        : backend{backend}
    {
        auto native = wlr_switch_from_input_device(dev);

        if (auto libinput = get_libinput_device(dev)) {
            control = std::make_unique<switch_control>(libinput, backend->frontend->config.main);
        }

        destroyed.receiver = this;
        destroyed.event.notify = switch_handle_destroy<Backend>;
        wl_signal_add(&dev->events.destroy, &destroyed.event);

        toggle_rec.receiver = this;
        toggle_rec.event.notify = handle_toggle<Backend>;
        wl_signal_add(&native->events.toggle, &toggle_rec.event);
    }

    switch_device(switch_device const&) = delete;
    switch_device& operator=(switch_device const&) = delete;
    ~switch_device() override = default;

    Backend* backend;

private:
    using er = base::event_receiver<switch_device<Backend>>;
    er destroyed;
    er toggle_rec;
};

}
