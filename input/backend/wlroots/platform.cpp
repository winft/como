/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "platform.h"

#include "keyboard.h"
#include "pointer.h"
#include "switch.h"
#include "touch.h"

#include "input/logging.h"

#include <cassert>

namespace KWin::input::backend::wlroots
{

void handle_device(struct wl_listener* listener, [[maybe_unused]] void* data)
{
    base::event_receiver<platform>* event_receiver_struct
        = wl_container_of(listener, event_receiver_struct, event);
    auto input = event_receiver_struct->receiver;

    auto device = reinterpret_cast<wlr_input_device*>(data);

    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        qCDebug(KWIN_INPUT) << "Keyboard device added:" << device->name;
        platform_add_keyboard(new keyboard<platform>(device, input), *input);
        break;
    case WLR_INPUT_DEVICE_POINTER:
        qCDebug(KWIN_INPUT) << "Pointer device added:" << device->name;
        platform_add_pointer(new pointer<platform>(device, input), *input);
        break;
    case WLR_INPUT_DEVICE_SWITCH:
        qCDebug(KWIN_INPUT) << "Switch device added:" << device->name;
        platform_add_switch(new switch_device<platform>(device, input), *input);
        break;
    case WLR_INPUT_DEVICE_TOUCH:
        qCDebug(KWIN_INPUT) << "Touch device added:" << device->name;
        platform_add_touch(new touch<platform>(device, input), *input);
        break;
    default:
        // TODO(romangg): Handle other device types.
        qCDebug(KWIN_INPUT) << "Device type unhandled.";
    }
}

platform::platform(base::wayland::platform& base)
    : wayland::platform<base::wayland::platform>(base)
    , base{static_cast<base::backend::wlroots::platform&>(base)}
{
    add_device.receiver = this;
    add_device.event.notify = handle_device;

    auto wlroots_base = dynamic_cast<base::backend::wlroots::platform const*>(&base);
    assert(wlroots_base);
    wl_signal_add(&wlroots_base->backend->events.new_input, &add_device.event);
}

}
