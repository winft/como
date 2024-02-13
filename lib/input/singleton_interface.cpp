/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "singleton_interface.h"

namespace como::input
{

input::cursor* singleton_interface::cursor{nullptr};
input::idle_qobject* singleton_interface::idle_qobject{nullptr};
input::platform_qobject* singleton_interface::platform_qobject{nullptr};

}
