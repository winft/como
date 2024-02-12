/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "device.h"

namespace KWin::input::control
{

class COMO_EXPORT switch_device : public device
{
    Q_OBJECT
public:
    switch_device();

    virtual bool is_lid_switch() const = 0;
    virtual bool is_tablet_mode_switch() const = 0;
};

}
