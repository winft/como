/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "output.h"

#include <QSize>

namespace como::base
{

struct output_topology {
    QSize size;
    double max_scale{1.};
    output const* current{nullptr};
};

}

Q_DECLARE_METATYPE(como::base::output_topology);
