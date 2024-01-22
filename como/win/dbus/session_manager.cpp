/*
    SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "session_manager.h"

// Include first to not clash with later X definitions in other includes.
#include "sessionadaptor.h"

namespace como::win::dbus
{

session_manager::session_manager()
{
    new SessionAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Session"), this);
}

}
