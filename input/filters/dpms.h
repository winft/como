/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input/event.h"
#include "input/event_filter.h"
#include "input/wayland/platform.h"

#include <QElapsedTimer>

namespace KWin::input
{

class dpms_filter : public event_filter
{
public:
    dpms_filter(wayland::platform* input);
    bool key(key_event const& event) override;

    bool button(button_event const& event) override;
    bool motion(motion_event const& event) override;
    bool axis(axis_event const& event) override;

    bool touch_down(touch_down_event const& event) override;
    bool touch_motion(touch_motion_event const& event) override;
    bool touch_up(touch_up_event const& event) override;

private:
    void notify();

    wayland::platform* input;
    QElapsedTimer m_doubleTapTimer;
    QVector<int32_t> m_touchPoints;
    bool m_secondTap = false;
};

}