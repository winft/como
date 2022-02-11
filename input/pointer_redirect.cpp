/*
    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "pointer_redirect.h"

#include "event_spy.h"
#include "redirect.h"

namespace KWin::input
{

bool pointer_redirect::s_cursorUpdateBlocking{false};

pointer_redirect::pointer_redirect(input::redirect* redirect)
    : device_redirect(redirect)
{
}

void pointer_redirect::process_button(button_event const& event)
{
    redirect->processSpies(std::bind(&event_spy::button, std::placeholders::_1, event));
}

}