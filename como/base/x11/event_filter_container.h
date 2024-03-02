/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "como_export.h"

#include <QObject>

namespace como::base::x11
{

class event_filter;

class COMO_EXPORT event_filter_container : public QObject
{
public:
    explicit event_filter_container(event_filter* filter);

    event_filter* filter() const;

private:
    event_filter* m_filter;
};

}
