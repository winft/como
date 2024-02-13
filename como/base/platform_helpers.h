/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <base/app_singleton.h>
#include <base/singleton_interface.h>

#include <QObject>
#include <ranges>
#include <vector>

namespace como::base
{

template<typename Platform>
void platform_init(Platform& platform)
{
    auto qobject = platform.qobject.get();

    QObject::connect(
        qobject, &Platform::qobject_t::output_added, qobject, [&platform](auto output) {
            if (!platform.topology.current) {
                platform.topology.current = output;
            }
        });
    QObject::connect(
        qobject, &Platform::qobject_t::output_removed, qobject, [&platform](auto output) {
            if (output == platform.topology.current) {
                platform.topology.current = nullptr;
            }
        });

    singleton_interface::platform = qobject;
    singleton_interface::get_outputs = [&platform]() -> std::vector<base::output*> {
        // TODO(romangg): Use ranges::to once we use C++23.
        auto range = platform.outputs
            | std::views::transform([](auto out) { return static_cast<base::output*>(out); });
        return {range.begin(), range.end()};
    };

    if (singleton_interface::app_singleton) {
        Q_EMIT singleton_interface::app_singleton->platform_created();
    }
}

}
