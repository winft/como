/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "utils/algorithm.h"

#include <algorithm>
#include <optional>
#include <string>

namespace como::win
{

struct appmenu_address {
    appmenu_address() = default;
    appmenu_address(std::string const& name, std::string const& path)
        : name{name}
        , path{path}
    {
    }

    bool operator==(appmenu_address const& other) const
    {
        return name == other.name && path == other.path;
    }
    bool empty() const
    {
        return name.empty() && path.empty();
    }

    std::string name;
    std::string path;
};

struct appmenu {
    bool active{false};
    appmenu_address address;
};

template<typename Space>
std::optional<typename Space::window_t> find_window_with_appmenu(Space const& space,
                                                                 appmenu_address const& address)
{
    auto it = std::find_if(space.windows.begin(), space.windows.end(), [address](auto win) {
        return std::visit(overload{[&](auto&& win) {
                              return win->control && win->control->appmenu.address == address;
                          }},
                          win);
    });

    if (it == space.windows.end()) {
        return {};
    }

    return *it;
}

}
