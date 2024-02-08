/*
SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "como_export.h"

#include <QObject>
#include <QString>

namespace KWin
{
namespace Perf
{

namespace Ftrace
{

/**
 * Internal perf API for consumers
 */
void COMO_EXPORT mark(const QString& message);
void COMO_EXPORT begin(const QString& message, ulong ctx);
void COMO_EXPORT end(const QString& message, ulong ctx);

bool COMO_EXPORT setEnabled(bool enable);

}
}
}
