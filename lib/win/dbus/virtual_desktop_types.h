/*
    SPDX-FileCopyrightText: 2018 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDBusArgument>
#include <como_export.h>

namespace como::win::dbus
{

struct subspace_data {
    uint position;
    QString id;
    QString name;
};

using subspace_data_vector = QVector<subspace_data>;

}

COMO_EXPORT QDBusArgument const& operator<<(QDBusArgument& argument,
                                            como::win::dbus::subspace_data const& desk);
COMO_EXPORT QDBusArgument const& operator>>(QDBusArgument const& argument,
                                            como::win::dbus::subspace_data& desk);

Q_DECLARE_METATYPE(como::win::dbus::subspace_data)

COMO_EXPORT QDBusArgument const&
operator<<(QDBusArgument& argument, como::win::dbus::subspace_data_vector const& deskVector);
COMO_EXPORT QDBusArgument const& operator>>(QDBusArgument const& argument,
                                            como::win::dbus::subspace_data_vector& deskVector);

Q_DECLARE_METATYPE(como::win::dbus::subspace_data_vector)
