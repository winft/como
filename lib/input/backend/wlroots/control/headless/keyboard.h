/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input/control/keyboard.h"
#include <como_export.h>

namespace como::input::backend::wlroots::headless
{

struct keyboard_control_data {
    bool is_enabled{false};
    keyboard_leds leds{keyboard_leds::none};

    bool supports_disable_events{false};
    bool is_alpha_numeric_keyboard{false};
};

class COMO_EXPORT keyboard_control : public input::control::keyboard
{
    Q_OBJECT

public:
    bool supports_disable_events() const override;
    bool is_enabled() const override;
    bool set_enabled_impl(bool enabled) override;

    bool is_alpha_numeric_keyboard() const override;
    void update_leds(keyboard_leds leds) override;

    keyboard_control_data data;
};

}
