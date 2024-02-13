/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/input/event_filter.h>
#include <como/input/logging.h>
#include <como/input/pointer_redirect.h>
#include <como/input/qt_event.h>

namespace como::input
{

/**
 * Useful when there's no proper tablet support on the clients
 */
template<typename Redirect>
class fake_tablet_filter : public event_filter<Redirect>
{
public:
    explicit fake_tablet_filter(Redirect& redirect)
        : event_filter<Redirect>(redirect)
    {
    }

    bool tabletToolEvent(QTabletEvent* event) override
    {
        auto get_event = [&event](button_state state) {
            return button_event{qt_mouse_button_to_button(Qt::LeftButton),
                                state,
                                {nullptr, static_cast<uint32_t>(event->timestamp())}};
        };

        switch (event->type()) {
        case QEvent::TabletMove:
        case QEvent::TabletEnterProximity:
            this->redirect.pointer->processMotion(event->globalPosF(), event->timestamp());
            break;
        case QEvent::TabletPress:
            this->redirect.pointer->process_button(get_event(button_state::pressed));
            break;
        case QEvent::TabletRelease:
            this->redirect.pointer->process_button(get_event(button_state::released));
            break;
        case QEvent::TabletLeaveProximity:
            break;
        default:
            qCWarning(KWIN_CORE) << "Unexpected tablet event type" << event;
            break;
        }

        return true;
    }
};

}
