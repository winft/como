/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/win/subspace.h>
#include <como_export.h>

#include <QObject>
#include <QPointF>

namespace como::win
{

class COMO_EXPORT subspace_manager_qobject : public QObject
{
    Q_OBJECT
Q_SIGNALS:
    void countChanged(uint previousCount, uint newCount);
    void rowsChanged(uint rows);

    void subspace_created(como::win::subspace*);
    void subspace_removed(como::win::subspace*);

    void current_changed(como::win::subspace* prev, como::win::subspace* next);

    /**
     * For realtime subspace switching animations. Offset is current total change in subspace
     * coordinate. x and y are negative if switching left/down. Example: x = 0.6 means 60% of the
     * way to the subspace to the right.
     */
    void current_changing(como::win::subspace* current, QPointF offset);
    void current_changing_cancelled();

    void layoutChanged(int columns, int rows);
    void nav_wraps_changed();
};

}
