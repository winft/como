/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como_export.h>
#include <win/subspace.h>

#include <QPoint>
#include <QSize>
#include <vector>

namespace KWin::win
{

class COMO_EXPORT subspace_grid
{
public:
    subspace_grid();
    ~subspace_grid();

    void update(QSize const& size, Qt::Orientation orientation, std::vector<subspace*> const& subs);
    QPoint gridCoords(subspace* vd) const;

    subspace* at(const QPoint& coords) const;
    int width() const;
    int height() const;
    QSize const& size() const;

private:
    QSize m_size;
    std::vector<std::vector<subspace*>> m_grid;
};

}
