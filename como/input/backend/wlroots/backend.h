/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/input/logging.h>
#include <como/input/wayland/platform.h>

#include <como/input/backend/wlroots/keyboard.h>
#include <como/input/backend/wlroots/pointer.h>
#include <como/input/backend/wlroots/switch.h>
#include <como/input/backend/wlroots/touch.h>

extern "C" {
#include <wlr/backend/multi.h>
#include <wlr/types/wlr_input_device.h>
}

#include <gsl/pointers>
#include <ranges>

namespace como::input::backend::wlroots
{

template<typename Backend>
void backend_handle_device(struct wl_listener* listener, [[maybe_unused]] void* data)
{
    base::event_receiver<Backend>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto input = event_receiver_struct->receiver;

    auto device = reinterpret_cast<wlr_input_device*>(data);

    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        qCDebug(KWIN_INPUT) << "Keyboard device added:" << device->name;
        platform_add_keyboard(new keyboard<Backend>(device, input), *input->frontend);
        break;
    case WLR_INPUT_DEVICE_POINTER:
        qCDebug(KWIN_INPUT) << "Pointer device added:" << device->name;
        platform_add_pointer(new pointer<Backend>(device, input), *input->frontend);
        break;
    case WLR_INPUT_DEVICE_SWITCH:
        qCDebug(KWIN_INPUT) << "Switch device added:" << device->name;
        platform_add_switch(new switch_device<Backend>(device, input), *input->frontend);
        break;
    case WLR_INPUT_DEVICE_TOUCH:
        qCDebug(KWIN_INPUT) << "Touch device added:" << device->name;
        platform_add_touch(new touch<Backend>(device, input), *input->frontend);
        break;
    default:
        // TODO(romangg): Handle other device types.
        qCDebug(KWIN_INPUT) << "Device type unhandled.";
    }
}

template<typename Frontend>
class backend
{
public:
    using type = backend<Frontend>;
    using frontend_type = Frontend;

    backend(Frontend& frontend)
        : frontend{&frontend}
        , native{frontend.base.backend.native}
    {
        add_device.receiver = this;
        add_device.event.notify = backend_handle_device<type>;

        wl_signal_add(&native->events.new_input, &add_device.event);
    }

    backend(backend const&) = delete;
    backend& operator=(backend const&) = delete;

    virtual ~backend()
    {
        unset_backend<wlroots::keyboard<type>>(frontend->keyboards);
        unset_backend<wlroots::pointer<type>>(frontend->pointers);
        unset_backend<wlroots::switch_device<type>>(frontend->switches);
        unset_backend<wlroots::touch<type>>(frontend->touchs);
    }

    gsl::not_null<Frontend*> frontend;
    wlr_backend* native{nullptr};

private:
    base::event_receiver<backend> add_device;

    template<typename Dev>
    void unset_backend(auto& container)
    {
        std::ranges::for_each(container,
                              [](auto dev) { static_cast<Dev*>(dev)->backend = nullptr; });
    }
};

}
