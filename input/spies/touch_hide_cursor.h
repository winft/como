/*
    SPDX-FileCopyrightText: 2018 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input/event_spy.h"

namespace KWin::input
{

class touch_hide_cursor_spy : public event_spy
{
public:
    void button(button_event const& event) override;
    void motion(motion_event const& event) override;
    void axis(axis_event const& event) override;

    void touch_down(touch_down_event const& event) override;

private:
    void showCursor();
    void hideCursor();

    bool m_cursorHidden = false;
};

}