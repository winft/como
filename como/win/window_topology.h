/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/win/subspace.h>
#include <como/win/types.h>
#include <vector>

namespace como::win
{

template<typename Output>
struct window_topology {
    win::layer layer{layer::unknown};
    Output const* central_output{nullptr};
    std::vector<subspace*> subspaces;
};

}
