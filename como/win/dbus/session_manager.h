/*
    SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/win/types.h>
#include <como_export.h>

#include <QObject>

namespace como::win::dbus
{

class COMO_EXPORT session_manager : public QObject
{
    Q_OBJECT
public:
    session_manager();

public Q_SLOTS:
    virtual void setState(uint /*state*/)
    {
    }
    virtual void loadSession(QString const& /*name*/)
    {
    }
    virtual void aboutToSaveSession(QString const& /*name*/)
    {
    }
    virtual void finishSaveSession(QString const& /*name*/)
    {
    }
    virtual void quit()
    {
    }

Q_SIGNALS:
    void stateChanged(session_state prev, session_state next);
    void loadSessionRequested(QString const& name);
    void prepareSessionSaveRequested(QString const& name);
    void finishSessionSaveRequested(QString const& name);
};

}
