/*
SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "como_export.h"

#include <QDate>
#include <QPair>
#include <QTime>

namespace KWin::render::post
{

/**
 * Calculates for a given location and date two of the
 * following sun timings in their temporal order:
 * - Nautical dawn and sunrise for the morning
 * - Sunset and nautical dusk for the evening
 * @since 5.12
 */

COMO_EXPORT QPair<QDateTime, QDateTime> calculate_sun_timings(const QDateTime& dateTime,
                                                              double latitude,
                                                              double longitude,
                                                              bool at_morning);

}
