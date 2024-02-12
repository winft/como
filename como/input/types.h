/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "utils/flags.h"

#include <QMetaType>

namespace como::input
{

enum class keyboard_leds {
    none = 0,
    num_lock = 1 << 0,
    caps_lock = 1 << 1,
    scroll_lock = 1 << 2,
};

enum class TabletEventType {
    Axis,
    Proximity,
    Tip,
};

}

ENUM_FLAGS(como::input::keyboard_leds)
Q_DECLARE_METATYPE(como::input::keyboard_leds)
