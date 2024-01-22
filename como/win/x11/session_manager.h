/*
    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/win/dbus/session_manager.h>
#include <como/win/types.h>
#include <como_export.h>

#include <QRect>

namespace como::win::x11
{

class COMO_EXPORT session_manager : public dbus::session_manager
{
public:
    ~session_manager() override;

    session_state state() const;

public:
    void setState(uint state) override;
    void loadSession(const QString& name) override;
    void aboutToSaveSession(const QString& name) override;
    void finishSaveSession(const QString& name) override;
    void quit() override;

private:
    void setState(session_state state);
    session_state m_sessionState{session_state::normal};
};

struct session_info {
    QByteArray sessionId;
    QByteArray windowRole;
    QByteArray wmCommand;
    QByteArray wmClientMachine;
    QByteArray resourceName;
    QByteArray resourceClass;

    QRect geometry;
    QRect restore;
    QRect fsrestore;

    int maximized;
    int fullscreen;
    int desktop;

    bool minimized;
    bool onAllDesktops;
    bool keepAbove;
    bool keepBelow;
    bool skipTaskbar;
    bool skipPager;
    bool skipSwitcher;
    bool noBorder;

    win_type windowType;
    QString shortcut;

    // means 'was active in the saved session'
    bool active;
    int stackingOrder;
    float opacity;
};

enum sm_save_phase {
    sm_save_phase0,     // saving global state in "phase 0"
    sm_save_phase2,     // saving window state in phase 2
    sm_save_phase2_full // complete saving in phase2, there was no phase 0
};

}
