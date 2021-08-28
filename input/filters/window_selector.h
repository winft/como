/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "../event_filter.h"

namespace KWin
{
class Toplevel;

namespace input
{

class window_selector_filter : public event_filter
{
public:
    bool button(button_event const& event) override;
    bool motion(motion_event const& event) override;
    bool axis(axis_event const& event) override;

    bool key(key_event const& event) override;
    bool key_repeat(key_event const& event) override;

    bool touchDown(qint32 id, const QPointF& pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF& pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

    bool isActive() const;
    void start(std::function<void(KWin::Toplevel*)> callback);
    void start(std::function<void(const QPoint&)> callback);

private:
    void deactivate();
    void cancel();
    void accept(const QPoint& pos);
    void accept(const QPointF& pos);

    bool m_active = false;
    std::function<void(KWin::Toplevel*)> m_callback;
    std::function<void(const QPoint&)> m_pointSelectionFallback;
    QMap<quint32, QPointF> m_touchPoints;
};

}
}
