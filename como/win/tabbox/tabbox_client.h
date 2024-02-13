/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QIcon>
#include <QString>
#include <QUuid>

namespace como::win
{

class tabbox_client
{
public:
    tabbox_client()
    {
    }

    virtual ~tabbox_client()
    {
    }

    virtual QString caption() const = 0;
    virtual QIcon icon() const = 0;
    virtual bool is_minimized() const = 0;
    virtual int x() const = 0;
    virtual int y() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual bool is_closeable() const = 0;
    virtual void close() = 0;
    virtual QUuid internal_id() const = 0;
};

}
