/*
    SPDX-FileCopyrightText: 2004 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/win/rules/book_settings.h>
#include <como/win/rules/window.h>

#include <QTimer>
#include <deque>

namespace como::win::rules
{

class book_settings;
class ruling;

class COMO_EXPORT book_qobject : public QObject
{
    Q_OBJECT
Q_SIGNALS:
    void updates_enabled();
};

class COMO_EXPORT book
{
public:
    book();
    ~book();

    void setUpdatesDisabled(bool disable);
    bool areUpdatesDisabled() const;

    void load();
    void save();

    void requestDiskStorage();

    std::unique_ptr<book_qobject> qobject;
    std::unique_ptr<book_settings> settings;
    std::deque<ruling*> m_rules;

private:
    void deleteAll();

    QTimer* m_updateTimer;
    bool m_updatesDisabled;
};

}
