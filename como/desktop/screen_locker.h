/*
    SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

#include <como_export.h>

namespace Wrapland::Server
{
class Client;
}

namespace como::desktop
{

class COMO_EXPORT screen_locker : public QObject
{
    Q_OBJECT
public:
    virtual bool is_locked() const = 0;

    virtual Wrapland::Server::Client* get_client() const = 0;

Q_SIGNALS:
    void locked(bool locked);
    void unlocked();
};

}
