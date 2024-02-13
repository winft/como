/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como_export.h>

#include <QPointer>

namespace como::base::x11
{
class event_filter;
class event_filter_container;

class COMO_EXPORT event_filter_manager
{
public:
    std::vector<QPointer<event_filter_container>> filters;
    std::vector<QPointer<event_filter_container>> generic_filters;

    void register_filter(event_filter* filter);
    void unregister_filter(event_filter* filter);
};

}
